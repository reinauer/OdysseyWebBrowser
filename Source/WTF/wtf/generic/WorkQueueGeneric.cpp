/*
 * Copyright (C) 2016 Konstantin Tokavev <annulen@yandex.ru>
 * Copyright (C) 2016 Yusuke Suzuki <utatane.tea@gmail.com>
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/WorkQueue.h>

#include <wtf/threads/BinarySemaphore.h>

#if OS(AMIGAOS3)
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>

// The SDK's exit code waits for every thread, including detached ones, so
// queue threads must end first. Some queues are never destroyed. The set
// holds a reference: a queue thread that hits a fatal error ends without
// removing its run loop.
static Lock queueRunLoopsLock;

static HashSet<RefPtr<RunLoop>>& queueRunLoops()
{
    static NeverDestroyed<HashSet<RefPtr<RunLoop>>> runLoops;
    return runLoops;
}

extern "C" void wtfAmigaOS3StopWorkQueues(void)
{
    Locker locker { queueRunLoopsLock };
    for (auto& runLoop : queueRunLoops()) {
        runLoop->stop();
        runLoop->dispatch([] {
            RunLoop::current().stop();
        });
    }
}
#endif

Ref<WorkQueue> WorkQueue::constructMainWorkQueue()
{
    return adoptRef(*new WorkQueue(RunLoop::main()));
}

WorkQueue::WorkQueue(RunLoop& runLoop)
    : m_runLoop(&runLoop)
{
}

void WorkQueue::platformInitialize(const char* name, Type, QOS)
{
    BinarySemaphore semaphore;
    Thread::create(name, [&] {
        m_runLoop = &RunLoop::current();
#if OS(AMIGAOS3)
        RunLoop* runLoop = m_runLoop;
        {
            Locker locker { queueRunLoopsLock };
            queueRunLoops().add(runLoop);
        }
#endif
        semaphore.signal();
        m_runLoop->run();
#if OS(AMIGAOS3)
        Locker locker { queueRunLoopsLock };
        queueRunLoops().remove(runLoop);
#endif
    })->detach();
    semaphore.wait();
}

void WorkQueue::platformInvalidate()
{
    if (m_runLoop) {
        Ref<RunLoop> protector(*m_runLoop);
        protector->stop();
        protector->dispatch([] {
            RunLoop::current().stop();
        });
    }
}

void WorkQueue::dispatch(Function<void()>&& function)
{
    RefPtr<WorkQueue> protect(this);
    m_runLoop->dispatch([protect, function = WTFMove(function)] {
        function();
    });
}

void WorkQueue::dispatchAfter(Seconds delay, Function<void()>&& function)
{
    RefPtr<WorkQueue> protect(this);
    m_runLoop->dispatchAfter(delay, [protect, function = WTFMove(function)] {
        function();
    });
}
