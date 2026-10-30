// Exercise real two-bit tagged structures with the m68k two-byte stack ABI.
#include "config.h"
#include "WeakImpl.h"
#include "JSCJSValueInlines.h"
#include "WeakHandleOwner.h"
#include <wtf/WordLock.h>
#include <atomic>
#include <new>
#include <pthread.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdint.h>

static WTF::WordLock lock;
static std::atomic<unsigned> ready { 0 };
static unsigned count;
extern "C" void* taggedPointerWorker(void*)
{
    ++ready;
    for (unsigned i = 0; i < 100; ++i) {
        lock.lock();
        ++count;
        if (!(i % 10))
            Delay(1); // Force contenders beyond WordLock's spinning path.
        lock.unlock();
    }
    return nullptr;
}

#if CPU(M68K)
extern "C" void* taggedPointerWorkerOffset(void*);
// Change the valid stack alignment residue by two without violating the
// classic ABI. The callee must align its own tagged stack queue node.
asm(".text\n.align 2\n.globl _taggedPointerWorkerOffset\n"
    "_taggedPointerWorkerOffset:\n"
    "move.l 4(%sp),%a0\nsubq.l #2,%sp\nmove.l %a0,-(%sp)\n"
    "jsr _taggedPointerWorker\naddq.l #6,%sp\nrts\n");
#endif

int main()
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    unsigned failures = 0;
    // Alignment of an embedded owner must preserve both tag bits too.
    struct EmbeddedOwner { uint16_t prefix; JSC::WeakHandleOwner owner; } embedded;
    static JSC::WeakHandleOwner staticOwner;
    JSC::WeakHandleOwner stackOwner;
    alignas(4) unsigned char ownerStorage[sizeof(JSC::WeakHandleOwner) + 4];
    auto* placedOwner = new (ownerStorage + (alignof(JSC::WeakHandleOwner) < 4 ? 2 : 0)) JSC::WeakHandleOwner;
    for (auto* owner : { &embedded.owner, &staticOwner, &stackOwner, placedOwner }) {
        JSC::WeakImpl weak(JSC::JSValue(reinterpret_cast<JSC::JSCell*>(0x1000)), owner, nullptr);
        if (weak.state() != JSC::WeakImpl::Live || weak.weakHandleOwner() != owner)
            ++failures;
        weak.setState(JSC::WeakImpl::Dead);
        if (weak.weakHandleOwner() != owner)
            ++failures;
        weak.setState(JSC::WeakImpl::Finalized);
        if (weak.weakHandleOwner() != owner)
            ++failures;
    }
    placedOwner->~WeakHandleOwner();
    printf("WeakHandleOwner tagged storage: %u failures (alignment %u)\n", failures, unsigned(alignof(JSC::WeakHandleOwner)));
    pthread_t workers[4];
    unsigned created = 0;
    lock.lock();
    for (unsigned i = 0; i < 4; ++i) {
        void* (*entry)(void*) = taggedPointerWorker;
#if CPU(M68K)
        if (i & 1)
            entry = taggedPointerWorkerOffset;
#endif
        if (pthread_create(&workers[i], nullptr, entry, nullptr))
            break;
        ++created;
    }
    while (ready != created)
        Delay(1);
    Delay(5);
    lock.unlock();
    for (unsigned i = 0; i < created; ++i) {
        if (pthread_join(workers[i], nullptr))
            ++failures;
    }
    if (created != 4 || count != 400)
        ++failures;
    printf("Tagged pointer test: %u failures (WordLock count %u)\n", failures, count);
    return failures ? 20 : 0;
}
