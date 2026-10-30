// Exercise the real C++ runtime, including facilities that a single-thread
// libstdc++ silently implements without synchronization.
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <stdlib.h>
#include <stdio.h>
#include <proto/dos.h>

#ifndef _GLIBCXX_HAS_GTHREADS
#error This test requires the pthread-enabled C++ runtime overlay
#endif
#ifndef _GLIBCXX_USE_CLOCK_MONOTONIC
#error The runtime must use the native monotonic clock
#endif

static_assert(__gnu_cxx::__default_lock_policy != __gnu_cxx::_S_single,
    "shared ownership must synchronize reference counts");

static std::atomic<unsigned> staticInitializations { 0 };
static std::atomic<unsigned> destructions { 0 };
static thread_local unsigned tlsValue;

static unsigned guardedStatic()
{
    static unsigned value = [] {
        ++staticInitializations;
        Delay(2);
        return 0x12345678u;
    }();
    return value;
}

struct SharedPayload {
    unsigned value { 0xabcdef01 };
    ~SharedPayload() { ++destructions; }
};

int main()
{
    printf("C++ runtime contention test starting\n");
    fflush(stdout);
    std::mutex mutex;
    std::condition_variable condition;
    std::once_flag once;
    std::once_flag nestedOnce;
    unsigned onceCalls = 0;
    unsigned nestedCalls = 0;
    unsigned protectedCount = 0;
    bool start = false;
    std::atomic<unsigned> failures { 0 };
    volatile unsigned unsignedBoundary = 0x80000000U;
    if (static_cast<double>(unsignedBoundary) != 2147483648.0)
        ++failures;
    volatile float floatBoundary = static_cast<float>(unsignedBoundary);
    if (floatBoundary != 2147483648.0f)
        ++failures;
    unsignedBoundary = 0xffffffffU;
    if (static_cast<double>(unsignedBoundary) != 4294967295.0)
        ++failures;
    floatBoundary = static_cast<float>(unsignedBoundary);
    if (floatBoundary != 4294967296.0f)
        ++failures;
    auto beforeSleep = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    auto elapsedSleep = std::chrono::steady_clock::now() - beforeSleep;
    printf("Monotonic sleep elapsed: %ld microseconds\n", static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(elapsedSleep).count()));
    fflush(stdout);
    if (elapsedSleep < std::chrono::milliseconds(2))
        ++failures;
    auto payload = std::make_shared<SharedPayload>();
    std::weak_ptr<SharedPayload> weak = payload;
    unsigned* addresses[4] = { };
    std::thread workers[4];
    auto stage = [&](unsigned worker, const char* message) {
        std::lock_guard<std::mutex> lock(mutex);
        printf("runtime worker %u: %s\n", worker, message);
        fflush(stdout);
    };

    for (unsigned i = 0; i < 4; ++i) {
        workers[i] = std::thread([&, i] {
            tlsValue = i + 1;
            addresses[i] = &tlsValue;
            {
                std::unique_lock<std::mutex> lock(mutex);
                condition.wait(lock, [&] { return start; });
            }
            stage(i, "started");
            if (guardedStatic() != 0x12345678u)
                ++failures;
            stage(i, "static guard complete");
            std::call_once(once, [&] {
                ++onceCalls;
                Delay(2);
                std::call_once(nestedOnce, [&] { ++nestedCalls; });
            });
            stage(i, "once complete");
            for (unsigned n = 0; n < 1000; ++n) {
                unsigned size = 64 + ((n * 13 + i * 17) % 1024);
                auto* memory = static_cast<unsigned char*>(malloc(size));
                if (!memory) {
                    ++failures;
                    return;
                }
                for (unsigned j = 0; j < size; ++j)
                    memory[j] = static_cast<unsigned char>(j + i);
                auto strong = weak.lock();
                if (!strong || strong->value != 0xabcdef01)
                    ++failures;
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    ++protectedCount;
                }
                if (!(n % 100))
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                for (unsigned j = 0; j < size; ++j) {
                    if (memory[j] != static_cast<unsigned char>(j + i))
                        ++failures;
                }
                free(memory);
                if (tlsValue != i + 1)
                    ++failures;
            }
            stage(i, "allocation and ownership stress complete");
        });
    }
    {
        std::lock_guard<std::mutex> lock(mutex);
        start = true;
    }
    condition.notify_all();
    for (auto& worker : workers)
        worker.join();
    payload.reset();
    if (weak.lock() || !weak.expired() || destructions != 1)
        ++failures;
    if (staticInitializations != 1 || onceCalls != 1 || nestedCalls != 1 || protectedCount != 4000)
        ++failures;
    printf("Runtime counters: static=%u once=%u protected=%u destroyed=%u\n", staticInitializations.load(), onceCalls, protectedCount, destructions.load());
    for (unsigned i = 0; i < 4; ++i) {
        for (unsigned j = 0; j < i; ++j) {
            if (addresses[i] == addresses[j])
                ++failures;
        }
    }
    printf("C++ runtime: %u failures (TLS, static guards, once, mutex, condition, shared/weak, std::thread, sleep, malloc, unsigned conversion)\n", failures.load());
    return failures ? 20 : 0;
}
