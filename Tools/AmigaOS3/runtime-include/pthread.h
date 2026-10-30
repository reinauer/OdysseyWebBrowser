#pragma once

#include <time.h>
#include_next <pthread.h>

// Preserve the SDK's deferred cancellation check while allowing qualified
// calls such as ::nanosleep in libstdc++ headers. Its original function-like
// macro expands to a parenthesized expression, which cannot follow ::.
#undef nanosleep
static inline int odyssey_pthread_nanosleep(const struct timespec* duration, struct timespec* remaining)
{
    pthread_testcancel();
    return nanosleep(duration, remaining);
}
#define nanosleep odyssey_pthread_nanosleep
