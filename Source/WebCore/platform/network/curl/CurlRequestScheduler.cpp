/*
 * Copyright (C) 2013 Apple Inc.  All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
 * Copyright (C) 2017 NAVER Corp.
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
#include "CurlRequestScheduler.h"

#if USE(CURL)

#include "CurlRequestSchedulerClient.h"

#if PLATFORM(MUI)
#include <proto/exec.h>
#if OS(AMIGAOS)
#define ODYSSEY
#endif
#include <unistd.h>
#include <proto/bsdsocket.h>
#include <sys/time.h>
#include <bsdsocket/socketbasetags.h>
#if !OS(AMIGAOS) && !OS(AMIGAOS3)
#include <aros/debug.h>
#endif
#undef send
#if OS(AMIGAOS3)
#include "AmigaOS3AmiSSL.h"
#else
struct Library *SocketBase;
#endif
#if OS(AMIGAOS)
struct SocketIFace *ISocket = NULL;
#endif

#if OS(AROS)
#include <aros/debug.h>
#define CDBG(x) bug x
#elif OS(MORPHOS)
extern "C" void dprintf(const char *fmt, ...);
#define CDBG(x) dprintf x
#elif OS(AMIGAOS3)
#include <cstdio>
#define CDBG(x) std::printf x
#else
#define CDBG(x) DebugPrintF x
#endif

void init_SocketBase()
{
#if OS(AMIGAOS3)
    RELEASE_ASSERT(odysseyAmigaOS3AmiSSLWorkerInitialize());
#else
    SocketBase = OpenLibrary("bsdsocket.library", 4L);
#if OS(AMIGAOS)
    ISocket = (SocketIFace *)GetInterface(SocketBase, "main", 1, NULL);
#endif
    SocketBaseTags(
        SBTM_SETVAL(SBTC_ERRNOPTR(sizeof(errno))), reinterpret_cast<uintptr_t>(&errno),
        SBTM_SETVAL(SBTC_LOGTAGPTR),       reinterpret_cast<uintptr_t>("cURL"),
        TAG_DONE);
#endif
}
void close_SocketBase()
{
#if OS(AMIGAOS3)
    odysseyAmigaOS3AmiSSLWorkerCleanup();
#else
    CloseLibrary(SocketBase);
#if OS(AMIGAOS)
    DropInterface((struct Interface*) ISocket);
    ISocket = NULL;
#endif    
    SocketBase = NULL;
#endif
}
#endif

namespace WebCore {

CurlRequestScheduler::CurlRequestScheduler(long maxConnects, long maxTotalConnections, long maxHostConnections)
    : m_maxConnects(maxConnects)
    , m_maxTotalConnections(maxTotalConnections)
    , m_maxHostConnections(maxHostConnections)
{

}

bool CurlRequestScheduler::add(CurlRequestSchedulerClient* client)
{
    ASSERT(isMainThread());

    if (!client)
        return false;

    if (!startTransfer(client))
        return false;
    startOrWakeUpThread();

    return true;
}

void CurlRequestScheduler::cancel(CurlRequestSchedulerClient* client)
{
    ASSERT(isMainThread());

    if (!client)
        return;

    cancelTransfer(client);
}

bool CurlRequestScheduler::callOnWorkerThread(WTF::Function<void()>&& task)
{
    {
        Locker locker { m_mutex };
#if OS(AMIGAOS3)
        if (m_stopped)
            return false;
#endif
        m_taskQueue.append(WTFMove(task));
    }

    startOrWakeUpThread();
    return true;
}

void CurlRequestScheduler::startOrWakeUpThread()
{
    ASSERT(isMainThread());

    {
        Locker locker { m_mutex };
        if (m_runThread) {
            wakeUpThreadIfPossible();
            return;
        }
#if PLATFORM(MUI)
        if (m_stopped)
            return;
#endif
    }

    if (m_thread)
        m_thread->waitForCompletion();

    {
        Locker locker { m_mutex };
        m_runThread = true;
    }

    m_thread = Thread::create("curlThread", [this] {
#if PLATFORM(MUI)
        init_SocketBase();
        CDBG(("[CURL] worker thread started SocketBase=%p\n", SocketBase));
        /* Increase priority so that network data is transported immediatelly */
        SetTaskPri(FindTask(NULL), 1);
#endif
        workerThread();

        Locker locker { m_mutex };
        m_runThread = false;
#if PLATFORM(MUI)
        close_SocketBase();
#endif
    }, ThreadType::Network);
}

void CurlRequestScheduler::wakeUpThreadIfPossible()
{
#if !PLATFORM(MUI)
    // curl_multi_wakeup() does not work reliably on AmigaOS/MorphOS/AROS
    // (internal socketpair/pipe mechanism may block with clib4).
    // Instead we rely on a short poll timeout in the worker loop.
    Locker locker { m_multiHandleMutex };
    if (!m_curlMultiHandle)
        return;

    m_curlMultiHandle->wakeUp();
#endif
}

void CurlRequestScheduler::stopThreadIfNoMoreJobRunning()
{
    ASSERT(!isMainThread());
#if !PLATFORM(MUI) // || OS(AMIGAOS)
    /* Keep the original curlThread running until browser quits */
    Locker locker { m_mutex };
    if (m_activeJobs.size() || m_taskQueue.size())
        return;

    m_runThread = false;
#endif
}

#if PLATFORM(MUI)
void CurlRequestScheduler::stopCurlThread()
{
	stopThread();
}

void CurlRequestScheduler::setMaxTotalConnections(long val)
{
    {
        Locker locker { m_mutex };
#if OS(AMIGAOS3)
        if (m_stopped)
            return;
#endif
        if (m_maxTotalConnections == val)
            return;
        m_maxTotalConnections = val;
        // CurlMultiHandle is worker-owned; update it on that thread, not while it performs requests.
        m_taskQueue.append([this, val] {
            if (m_curlMultiHandle)
                m_curlMultiHandle->setMaxTotalConnections(val);
        });
    }
    startOrWakeUpThread();
}

CurlStreamID CurlRequestScheduler::createStream(const URL& url, CurlStream::Client& client)
{
    ASSERT(isMainThread());

    do {
        m_currentStreamID = (m_currentStreamID + 1 != invalidCurlStreamID) ? m_currentStreamID + 1 : 1;
    } while (m_clientList.contains(m_currentStreamID));

    auto streamID = m_currentStreamID;
    m_clientList.add(streamID, &client);
#if OS(AMIGAOS3)
    {
        Locker locker { m_mutex };
        if (!m_stopped)
            m_requestedStreams.add(streamID);
    }
#endif

    bool queued = callOnWorkerThread([this, streamID, url = url.isolatedCopy()]() mutable {
#if OS(AMIGAOS3)
        {
            Locker locker { m_mutex };
            if (m_stopped || !m_requestedStreams.contains(streamID))
                return;
        }
#endif
        m_streamList.add(streamID, CurlStream::create(*this, streamID, WTFMove(url)));
    });
    if (!queued) {
        callOnMainThread([this, streamID] {
            if (auto* client = m_clientList.get(streamID))
                client->didFail(streamID, CURLE_ABORTED_BY_CALLBACK);
        });
    }

    return streamID;
}

void CurlRequestScheduler::destroyStream(CurlStreamID streamID)
{
    ASSERT(isMainThread());

    if (m_clientList.contains(streamID))
        m_clientList.remove(streamID);
#if OS(AMIGAOS3)
    {
        Locker locker { m_mutex };
        m_requestedStreams.remove(streamID);
    }
#endif

    callOnWorkerThread([this, streamID]() {
        if (m_streamList.contains(streamID))
            m_streamList.remove(streamID);
    });
}

void CurlRequestScheduler::send(CurlStreamID streamID, UniqueArray<uint8_t>&& data, size_t length)
{
    ASSERT(isMainThread());

    callOnWorkerThread([this, streamID, data = WTFMove(data), length]() mutable {
        if (auto stream = m_streamList.get(streamID))
            stream->send(WTFMove(data), length);
    });
}

void CurlRequestScheduler::callClientOnMainThread(CurlStreamID streamID, WTF::Function<void(CurlStream::Client&)>&& task)
{
    ASSERT(!isMainThread());

    callOnMainThread([this, streamID, task = WTFMove(task)]() {
        if (auto client = m_clientList.get(streamID))
            task(*client);
    });
}

#if OS(AMIGAOS3)
bool CurlRequestScheduler::addStreamHandle(CURL* handle, CurlStream& stream)
{
    ASSERT(!isMainThread());
    if (!handle || m_curlMultiHandle->addHandle(handle) != CURLM_OK)
        return false;
    ASSERT(!m_streamHandles.contains(handle));
    m_streamHandles.add(handle, &stream);
    return true;
}

void CurlRequestScheduler::removeStreamHandle(CURL* handle)
{
    ASSERT(!isMainThread());
    ASSERT(m_streamHandles.contains(handle));
    m_streamHandles.remove(handle);
    m_curlMultiHandle->removeHandle(handle);
}
#endif
#endif

void CurlRequestScheduler::stopThread()
{
    {
        Locker locker { m_mutex };
#if OS(AMIGAOS3)
        m_stopped = true;
        m_requestedStreams.clear();
#endif
        m_runThread = false;
    }

    if (m_thread) {
        wakeUpThreadIfPossible();
        m_thread->waitForCompletion();
        m_thread = nullptr;
    }
#if OS(AMIGAOS3)
    // Discard queued stream notifications after their consumers shut down.
    m_clientList.clear();
#endif
}

void CurlRequestScheduler::executeTasks()
{
    ASSERT(!isMainThread());

    Vector<WTF::Function<void()>> taskQueue;

    {
        Locker locker { m_mutex };
        taskQueue = WTFMove(m_taskQueue);
    }

    for (auto& task : taskQueue)
        task();
}

void CurlRequestScheduler::workerThread()
{
    ASSERT(!isMainThread());

    long maxTotalConnections;
    {
        Locker locker { m_mutex };
        maxTotalConnections = m_maxTotalConnections;
    }
    {
        Locker locker { m_multiHandleMutex };
        m_curlMultiHandle.emplace();
        m_curlMultiHandle->setMaxConnects(m_maxConnects);
        m_curlMultiHandle->setMaxTotalConnections(maxTotalConnections);
        m_curlMultiHandle->setMaxHostConnections(m_maxHostConnections);
    }

    while (true) {
        {
            Locker locker { m_mutex };
            if (!m_runThread)
                break;
        }

        executeTasks();

#if OS(AMIGAOS3)
        {
            Locker locker { m_mutex };
            if (!m_runThread)
                break;
        }
#endif
#if OS(MORPHOS)
        const int selectTimeoutMS = INT_MAX;
        CURLMcode mc = m_curlMultiHandle->poll({ }, selectTimeoutMS);
        if (mc != CURLM_OK)
            break;
        int activeCount = 0;
        mc = m_curlMultiHandle->perform(activeCount);
        if (mc != CURLM_OK)
            break;
#else
        int activeCount = 0;
        CURLMcode mc = m_curlMultiHandle->perform(activeCount);
        if (mc != CURLM_OK) {
            CDBG(("[CURL] perform failed mc=%d\n", (int)mc));
#if OS(AMIGAOS3)
            // An internal multi failure is not an HTTP/TLS request error.
            // Do not tear down task-owned handles while the main task can
            // still release deferred responses; shutdown requires its join.
            RELEASE_ASSERT_NOT_REACHED();
#else
            break;
#endif
        }

        const int selectTimeoutMS = 10;
#if PLATFORM(MUI)
    // curl_multi_poll() is unreliable on MUI and can stall request completion.
    // Use a deterministic tick loop instead.
    usleep(selectTimeoutMS * 1000);
#else
        mc = m_curlMultiHandle->poll({ }, selectTimeoutMS);
    if (mc != CURLM_OK)
            break;
#endif
#endif
        // check the curl messages indicating completed transfers
        // and free their resources
        while (true) {
            int messagesInQueue = 0;
            CURLMsg* msg = m_curlMultiHandle->readInfo(messagesInQueue);
            if (!msg)
                break;

            ASSERT(msg->msg == CURLMSG_DONE);
            if (auto client = m_clientMaps.inlineGet(msg->easy_handle))
                completeTransfer(client, msg->data.result);
#if OS(AMIGAOS3)
            else if (auto* stream = m_streamHandles.get(msg->easy_handle))
                stream->didCompleteConnection(msg->data.result);
#endif
        }

#if OS(AMIGAOS3)
        for (auto& stream : m_streamList.values())
            stream->tryToTransfer();
#endif

        stopThreadIfNoMoreJobRunning();
    }

#if OS(AMIGAOS3)
    {
        Locker locker { m_mutex };
        RELEASE_ASSERT(m_stopped && !m_runThread);
    }
    // Drain retained completion/cancellation callbacks. Setup tasks skip
    // stopped or cancelled work, so this cannot start new connections.
    executeTasks();
    Vector<CurlRequestSchedulerClient*> clients;
    {
        Locker locker { m_mutex };
        for (auto* client : m_activeJobs)
            clients.append(client);
    }
    for (auto* client : clients)
        cancelTransfer(client);
    executeTasks();
    m_streamList.clear();
    RELEASE_ASSERT(m_streamHandles.isEmpty());
    RELEASE_ASSERT(m_clientMaps.isEmpty());
#endif
    {
        Locker locker { m_multiHandleMutex };
        m_curlMultiHandle.reset();
    }
#if OS(AMIGAOS3)
    CurlHandle::closeHandlesOnWorkerThread();
#endif
}

bool CurlRequestScheduler::startTransfer(CurlRequestSchedulerClient* client)
{
    auto task = [this, client]() {
#if OS(AMIGAOS3)
        {
            Locker locker { m_mutex };
            if (m_stopped || !m_activeJobs.contains(client))
                return;
        }
#endif
        CURL* handle = client->setupTransfer();
        if (!handle) {
            CDBG(("[CURL] setupTransfer returned NULL -> CURLE_FAILED_INIT\n"));
            completeTransfer(client, CURLE_FAILED_INIT);
            return;
        }

        auto addResult = m_curlMultiHandle->addHandle(handle);
        if (addResult != CURLM_OK) {
            CDBG(("[CURL] addHandle failed\n"));
            completeTransfer(client, CURLE_FAILED_INIT);
            return;
        }

        ASSERT(!m_clientMaps.contains(handle));
        m_clientMaps.set(handle, client);
    };

    Locker locker { m_mutex };
#if OS(AMIGAOS3)
    if (m_stopped)
        return false;
#endif
    client->retain();
    m_activeJobs.add(client);
    m_taskQueue.append(WTFMove(task));
    return true;
}

void CurlRequestScheduler::completeTransfer(CurlRequestSchedulerClient* client, CURLcode result)
{
    finalizeTransfer(client, [client, result]() {
        client->didCompleteTransfer(result);
    });
}

void CurlRequestScheduler::cancelTransfer(CurlRequestSchedulerClient* client)
{
    finalizeTransfer(client, [client]() {
        client->didCancelTransfer();
    });
}

void CurlRequestScheduler::finalizeTransfer(CurlRequestSchedulerClient* client, Function<void()> completionHandler)
{
    Locker locker { m_mutex };

    if (!m_activeJobs.contains(client))
        return;

    m_activeJobs.remove(client);

    auto task = [this, client, completionHandler = WTFMove(completionHandler)]() {
        if (client->handle()) {
            if (m_clientMaps.contains(client->handle())) {
                m_clientMaps.remove(client->handle());
                m_curlMultiHandle->removeHandle(client->handle());
            }
        }

        completionHandler();

        callOnMainThread([client]() {
            client->release();
        });
    };

    m_taskQueue.append(WTFMove(task));
}

}

#endif
