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

int gettimeofday(struct timeval* value, struct timezone* zone)
{
    struct TimeRequest request = { 0 };
    struct TimeVal now;
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

    if (value) {
        request.tr_node.io_Message.mn_Length = sizeof(request);
        if (OpenDevice(TIMERNAME, UNIT_MICROHZ, &request.tr_node, 0)) {
            errno = EIO;
            return -1;
        }
        /* A local base avoids shared initialization and shutdown races. */
        struct Device* TimerBase = request.tr_node.io_Device;
        GetSysTime(&now);
        CloseDevice(&request.tr_node);

        /* timer.device uses local time since 1978; POSIX uses UTC since 1970.
         * Preserve Odyssey's optional DST_OFFSET convention (seconds). */
        const char* dst = getenv("DST_OFFSET");
        value->tv_sec = now.tv_secs + 252460800L + minutesWest * 60
            + (dst ? strtol(dst, NULL, 10) : 0);
        value->tv_usec = now.tv_micro;
    }
    if (zone) {
        zone->tz_minuteswest = minutesWest;
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
    struct TimeRequest request = { 0 };
    struct MsgPort* port = CreateMsgPort();
    if (!port) {
        errno = ENOMEM;
        return -1;
    }
    request.tr_node.io_Message.mn_ReplyPort = port;
    request.tr_node.io_Message.mn_Length = sizeof(request);
    if (OpenDevice(TIMERNAME, UNIT_MICROHZ, &request.tr_node, 0)) {
        DeleteMsgPort(port);
        errno = EIO;
        return -1;
    }
    request.tr_node.io_Command = TR_ADDREQUEST;
    request.tr_time.tv_secs = seconds;
    request.tr_time.tv_micro = microseconds;
    int error = DoIO(&request.tr_node);
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
        struct TimeRequest request = { 0 };
        struct EClockVal now;
        request.tr_node.io_Message.mn_Length = sizeof(request);
        if (OpenDevice(TIMERNAME, UNIT_ECLOCK, &request.tr_node, 0)) {
            errno = EIO;
            return -1;
        }
        struct Device* TimerBase = request.tr_node.io_Device;
        ULONG frequency = ReadEClock(&now);
        CloseDevice(&request.tr_node);
        if (!frequency) {
            errno = EIO;
            return -1;
        }
        uint64_t ticks = ((uint64_t)now.ev_hi << 32) | now.ev_lo;
        value->tv_sec = ticks / frequency;
        value->tv_nsec = ((ticks % frequency) * 1000000000ULL) / frequency;
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
    struct TimeRequest request = { 0 };
    struct EClockVal clock;
    request.tr_node.io_Message.mn_Length = sizeof(request);
    if (OpenDevice(TIMERNAME, UNIT_ECLOCK, &request.tr_node, 0))
        abort();
    struct Device* TimerBase = request.tr_node.io_Device;
    ULONG frequency = ReadEClock(&clock);
    CloseDevice(&request.tr_node);
    if (!frequency)
        abort();
    return ((double)clock.ev_hi * 4294967296.0 + clock.ev_lo) / frequency;
}
