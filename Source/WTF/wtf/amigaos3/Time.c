/* Classic AmigaOS time functions required by the SDK's pthread library. */
#define _DEFAULT_SOURCE 1
#define __USE_NEW_TIMEVAL__ 1
#include <sys/time.h>
#include <errno.h>
#include <stdlib.h>
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
