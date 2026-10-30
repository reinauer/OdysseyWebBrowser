#pragma once

#include_next <time.h>

// The native port supplies nanosleep, but the SDK hides its declaration
// behind the broader, unsupported _POSIX_TIMERS feature group.
#ifdef __cplusplus
extern "C" {
#endif
__stdargs int nanosleep(const struct timespec*, struct timespec*);
#ifdef __cplusplus
}
#endif
