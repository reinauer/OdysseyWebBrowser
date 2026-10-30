#include <wtf/amigaos3/ThreadContext.h>
#include <wtf/amigaos3/ConservativeScan.h>
#include <atomic>
#include <mutex>
#include <pthread.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>

namespace {

struct Worker {
    WTF::AmigaOS3ThreadContext context;
    volatile unsigned heartbeat { 0 };
    volatile unsigned char ready { 0 };
    volatile unsigned char stop { 0 };
    void* stackOrigin { nullptr };
};

struct OnceState {
    std::once_flag flag;
    std::once_flag nestedFlag;
    unsigned calls { 0 };
    unsigned nestedCalls { 0 };
    unsigned published { 0 };
    std::atomic<bool> initializerEntered { false };
    std::atomic<bool> releaseInitializer { false };
};

struct OnceWorker {
    OnceState* state;
    unsigned value;
    unsigned* tlsAddress { nullptr };
    bool passed { false };
};

static thread_local unsigned tlsValue;

void* runOnceWorker(void* argument)
{
    auto& worker = *static_cast<OnceWorker*>(argument);
    tlsValue = worker.value;
    worker.tlsAddress = &tlsValue;
    for (unsigned i = 0; i < 100; ++i) {
        std::call_once(worker.state->flag, [&] {
            ++worker.state->calls;
            // Keep pthread_once's cleanup handler live while the parent
            // creates more threads. Relocating pthread records here corrupts
            // the embedded cleanup list and cached worker exit state.
            worker.state->initializerEntered.store(true);
            while (!worker.state->releaseInitializer.load())
                Delay(1);
            std::call_once(worker.state->nestedFlag, [&] {
                ++worker.state->nestedCalls;
            });
            worker.state->published = 0xfeedbeef;
        });
        if (worker.state->published != 0xfeedbeef)
            return nullptr;
        Delay(1);
        if (tlsValue != worker.value)
            return nullptr;
    }
    worker.passed = true;
    return nullptr;
}

unsigned checkOnceAndTLS()
{
    OnceState state;
    OnceWorker workers[] = { { &state, 1 }, { &state, 2 }, { &state, 3 }, { &state, 4 } };
    pthread_t threads[4];
    unsigned created = 0;
    unsigned failures = 0;
    for (; created < 4; ++created) {
        if (pthread_create(&threads[created], nullptr, runOnceWorker, &workers[created])) {
            ++failures;
            break;
        }
        if (!created) {
            for (unsigned tries = 0; tries < 100 && !state.initializerEntered.load(); ++tries)
                Delay(1);
            if (!state.initializerEntered.load())
                ++failures;
        }
    }
    state.releaseInitializer.store(true);
    for (unsigned i = 0; i < created; ++i) {
        if (pthread_join(threads[i], nullptr) || !workers[i].passed)
            ++failures;
        for (unsigned j = 0; j < i; ++j) {
            if (workers[i].tlsAddress == workers[j].tlsAddress)
                ++failures;
        }
    }
    if (state.calls != 1 || state.nestedCalls != 1)
        ++failures;
    printf("Once contention and compiler TLS: %u failures\n", failures);
    return failures;
}

void* runWorker(void* argument)
{
    auto& worker = *static_cast<Worker*>(argument);
    worker.context.initialize();
    worker.stackOrigin = FindTask(nullptr)->tc_SPUpper;
    worker.ready = 1;
    // No calls or spills inside this loop: these markers are live only in
    // D2/A2 until Exec preempts us and saves the complete register frame.
    asm volatile(
        "move.l #0x13572468,%%d2\n\t"
        "move.l #0x24681358,%%a2\n"
        "1: addq.l #1,(%0)\n\t"
        "tst.b (%1)\n\t"
        "beq 1b"
        : : "a" (&worker.heartbeat), "a" (&worker.stop)
        : "d2", "a2", "cc", "memory");
    worker.context.didExit();
    return nullptr;
}

}

unsigned checkAmigaOS3ThreadContexts()
{
    unsigned failures = 0;
    Worker workers[2];
    pthread_t threads[2];
    unsigned created = 0;
    for (; created < 2; ++created) {
        if (pthread_create(&threads[created], nullptr, runWorker, &workers[created])) {
            ++failures;
            break;
        }
    }
    if (created == 2) {
        for (unsigned tries = 0; tries < 100 && (!workers[0].ready || !workers[1].ready); ++tries)
            Delay(1);
        Delay(1);
        if (!workers[0].ready || !workers[1].ready)
            ++failures;
        else {
            for (unsigned iteration = 0; iteration < 100; ++iteration) {
                bool suspended[2] = { workers[0].context.suspend(), workers[1].context.suspend() };
                bool valid = suspended[0] && suspended[1];
                unsigned before[2] = { workers[0].heartbeat, workers[1].heartbeat };
                for (unsigned i = 0; i < 2; ++i) {
                    if (!suspended[i])
                        continue;
                    bool dataRoot = false;
                    bool addressRoot = false;
                    WTF::forEachAmigaOS3ConservativePointer(workers[i].context.stackPointer(), workers[i].stackOrigin, [&](void* candidate) {
                        dataRoot |= reinterpret_cast<uintptr_t>(candidate) == 0x13572468;
                        addressRoot |= reinterpret_cast<uintptr_t>(candidate) == 0x24681358;
                    });
                    valid &= dataRoot && addressRoot;
                }
                // More nesting than Exec's signed-byte disable counter can
                // represent; the production helper must use one Exec level.
                if (suspended[0]) {
                    for (unsigned i = 0; i < 160; ++i)
                        valid &= workers[0].context.suspend();
                    for (unsigned i = 0; i < 160; ++i)
                        workers[0].context.resume();
                }
                valid &= workers[0].heartbeat == before[0] && workers[1].heartbeat == before[1];
                for (unsigned i = 0; i < 2; ++i) {
                    if (suspended[i])
                        workers[i].context.resume();
                }
                if (!valid)
                    ++failures;
                Delay(1);
                if (workers[0].heartbeat == before[0] || workers[1].heartbeat == before[1])
                    ++failures;
            }
        }
    }
    for (unsigned i = 0; i < created; ++i)
        workers[i].stop = 1;
    for (unsigned i = 0; i < created; ++i) {
        if (pthread_join(threads[i], nullptr))
            ++failures;
        if (workers[i].context.suspend()) {
            ++failures;
            workers[i].context.resume();
        }
    }

    alignas(4) unsigned char wordAlignedRoot[8] = { 0, 0, 0x12, 0x34, 0x56, 0x78, 0, 0 };
    bool found = false;
    unsigned candidates = 0;
    WTF::forEachAmigaOS3ConservativePointer(wordAlignedRoot, wordAlignedRoot + sizeof(wordAlignedRoot), [&](void* candidate) {
        found |= reinterpret_cast<uintptr_t>(candidate) == 0x12345678;
        ++candidates;
    });
    if (!found || candidates != 3)
        ++failures;
    printf("Exec thread snapshots: %u failures (D2/A2, word alignment, nested suspension, exit)\n", failures);
    fflush(stdout);
    failures += checkOnceAndTLS();
    return failures;
}
