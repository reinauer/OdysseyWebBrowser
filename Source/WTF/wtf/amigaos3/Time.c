/* Classic AmigaOS time functions required by the SDK's pthread library. */
#define _DEFAULT_SOURCE 1
#define __USE_NEW_TIMEVAL__ 1
#include <sys/time.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <devices/timer.h>
#include <libraries/locale.h>
#include <proto/exec.h>
#include <proto/locale.h>
#include <proto/timer.h>
#include <stabs.h>

/* timer.device is opened once for the whole program. GetSysTime() and
 * ReadEClock() only need its base, and sleeping tasks clone this request.
 * The first callers may race; the loser closes its own unit again. The
 * device is ROM-resident and never expunged, so its base stays valid after
 * the request is closed at exit; a sleep after that opens its own request. */
static struct TimeRequest sharedRequest;
static struct Device* volatile sharedTimer;
static volatile int sharedRequestClosed;

static struct Device* timerDevice(void)
{
    struct Device* device = sharedTimer;
    if (device)
        return device;
    struct TimeRequest request = { 0 };
    request.tr_node.io_Message.mn_Length = sizeof(request);
    if (OpenDevice(TIMERNAME, UNIT_MICROHZ, &request.tr_node, 0))
        return NULL;
    Forbid();
    if (!sharedTimer) {
        sharedRequest = request;
        sharedTimer = request.tr_node.io_Device;
        request.tr_node.io_Device = NULL;
    }
    device = sharedTimer;
    Permit();
    if (request.tr_node.io_Device)
        CloseDevice(&request.tr_node);
    return device;
}

/* C++ destructors run at exit before the SDK's pthread exit function joins
 * the remaining threads, which may still sleep. Close the shared request
 * after that join (priority -6) instead. */
static void __attribute__((used)) closeTimerDevice(void)
{
    Forbid();
    int open = sharedTimer && !sharedRequestClosed;
    sharedRequestClosed = 1;
    Permit();
    if (open)
        CloseDevice(&sharedRequest.tr_node);
}
ADD2EXIT(closeTimerDevice, -50);

/* The locale's offset and DST_OFFSET are read once: opening locale.library
 * and reading an environment variable cost more than the clock itself. A
 * time zone change takes effect when the program is restarted. */
static volatile int zoneKnown;
static long zoneMinutesWest;
static long zoneAdjustSeconds;

static void readZone(void)
{
    long minutesWest = 0;
    struct LocaleBase* LocaleBase = (struct LocaleBase*)OpenLibrary("locale.library", 38);
    if (LocaleBase) {
        struct Locale* locale = OpenLocale(NULL);
        if (locale) {
            minutesWest = locale->loc_GMTOffset;
            CloseLocale(locale);
        }
        CloseLibrary((struct Library*)LocaleBase);
    }
    /* Preserve Odyssey's optional DST_OFFSET convention (seconds). */
    const char* dst = getenv("DST_OFFSET");
    zoneMinutesWest = minutesWest;
    zoneAdjustSeconds = minutesWest * 60 + (dst ? strtol(dst, NULL, 10) : 0);
    zoneKnown = 1;
}

int gettimeofday(struct timeval* value, struct timezone* zone)
{
    if (!zoneKnown)
        readZone();
    if (value) {
        struct Device* TimerBase = timerDevice();
        struct TimeVal now;
        if (!TimerBase) {
            errno = EIO;
            return -1;
        }
        GetSysTime(&now);
        /* timer.device uses local time since 1978; POSIX uses UTC since 1970. */
        value->tv_sec = now.tv_secs + 252460800L + zoneAdjustSeconds;
        value->tv_usec = now.tv_micro;
    }
    if (zone) {
        zone->tz_minuteswest = zoneMinutesWest;
        zone->tz_dsttime = 0;
    }
    return 0;
}

/* newlib's _gettimeofday_r (and hence time()) calls the syscall spelling. */
int _gettimeofday(struct timeval* value, void* zone)
{
    return gettimeofday(value, (struct timezone*)zone);
}

static int waitForTimer(unsigned long seconds, unsigned long microseconds)
{
    if (!timerDevice()) {
        errno = EIO;
        return -1;
    }
    struct MsgPort* port = CreateMsgPort();
    if (!port) {
        errno = ENOMEM;
        return -1;
    }
    /* A copy of an opened request may be used for I/O; only the original
     * is closed. */
    struct TimeRequest request;
    Forbid();
    int own = sharedRequestClosed;
    if (!own)
        request = sharedRequest;
    Permit();
    if (own) {
        request = (struct TimeRequest) { 0 };
        request.tr_node.io_Message.mn_Length = sizeof(request);
        if (OpenDevice(TIMERNAME, UNIT_MICROHZ, &request.tr_node, 0)) {
            DeleteMsgPort(port);
            errno = EIO;
            return -1;
        }
    }
    request.tr_node.io_Message.mn_ReplyPort = port;
    request.tr_node.io_Command = TR_ADDREQUEST;
    request.tr_node.io_Flags = 0;
    request.tr_time.tv_secs = seconds;
    request.tr_time.tv_micro = microseconds;
    int error = DoIO(&request.tr_node);
    if (own)
        CloseDevice(&request.tr_node);
    DeleteMsgPort(port);
    if (error) {
        errno = EIO;
        return -1;
    }
    return 0;
}

int nanosleep(const struct timespec* duration, struct timespec* remaining)
{
    if (!duration || duration->tv_sec < 0 || duration->tv_nsec < 0 || duration->tv_nsec >= 1000000000L) {
        errno = EINVAL;
        return -1;
    }
    unsigned long seconds = duration->tv_sec;
    /* Round up: a successful sleep must last at least the requested time. */
    unsigned long microseconds = (duration->tv_nsec + 999L) / 1000L;
    if (microseconds == 1000000) {
        ++seconds;
        microseconds = 0;
    }
    if (waitForTimer(seconds, microseconds))
        return -1;
    if (remaining) {
        remaining->tv_sec = 0;
        remaining->tv_nsec = 0;
    }
    return 0;
}

int usleep(useconds_t microseconds)
{
    return waitForTimer(microseconds / 1000000, microseconds % 1000000);
}

unsigned sleep(unsigned seconds)
{
    return waitForTimer(seconds, 0) ? seconds : 0;
}

int clock_gettime(clockid_t clock, struct timespec* value)
{
    if (!value) {
        errno = EFAULT;
        return -1;
    }
    if (clock == CLOCK_REALTIME) {
        struct timeval now;
        if (gettimeofday(&now, NULL))
            return -1;
        value->tv_sec = now.tv_sec;
        value->tv_nsec = now.tv_usec * 1000L;
        return 0;
    }
    if (clock == CLOCK_MONOTONIC) {
        struct Device* TimerBase = timerDevice();
        struct EClockVal now;
        ULONG frequency = TimerBase ? ReadEClock(&now) : 0;
        if (!frequency) {
            errno = EIO;
            return -1;
        }
#ifdef __HAVE_68881__
        /* curl reads this clock several times per pass. The FPU replaces
         * three libgcc 64-bit divisions; doubles hold the tick count exactly
         * for centuries, and truncation keeps the result monotonic. */
        double seconds = ((double)now.ev_hi * 4294967296.0 + now.ev_lo) / frequency;
        unsigned long wholeSeconds = (unsigned long)seconds;
        value->tv_sec = wholeSeconds;
        long nanoseconds = (long)((seconds - wholeSeconds) * 1e9);
        value->tv_nsec = nanoseconds > 999999999L ? 999999999L : nanoseconds;
#else
        uint64_t ticks = ((uint64_t)now.ev_hi << 32) | now.ev_lo;
        value->tv_sec = ticks / frequency;
        value->tv_nsec = ((ticks % frequency) * 1000000000ULL) / frequency;
#endif
        return 0;
    }
    errno = EINVAL;
    return -1;
}

/* The SDK pthread archive calls this symbol rather than the BSD macro. */
#undef timersub
void timersub(const struct timeval* left, const struct timeval* right, struct timeval* result)
{
    long seconds = left->tv_sec - right->tv_sec;
    long microseconds = left->tv_usec - right->tv_usec;
    if (microseconds < 0) {
        --seconds;
        microseconds += 1000000;
    }
    result->tv_sec = seconds;
    result->tv_usec = microseconds;
}

/* EClock is independent of wall-clock and timezone changes. */
double amigaos3MonotonicTime(void)
{
    struct Device* TimerBase = timerDevice();
    struct EClockVal clock;
    if (!TimerBase)
        abort();
    ULONG frequency = ReadEClock(&clock);
    if (!frequency)
        abort();
    return ((double)clock.ev_hi * 4294967296.0 + clock.ev_lo) / frequency;
}
