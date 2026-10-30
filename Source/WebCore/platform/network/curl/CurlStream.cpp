/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CurlStream.h"

#include "CurlStreamScheduler.h"
#include "SocketStreamError.h"

#if OS(AMIGAOS3)
#include "AmigaOS3AmiSSL.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#endif

#if USE(CURL)

namespace WebCore {

#if OS(AMIGAOS3)
static void traceStream(CurlStreamID streamID, const char* operation, CURLcode result, size_t bytes = 0)
{
    static FILE* output = []() -> FILE* {
        const char* path = getenv("OWB_CURL_STREAM_TRACE");
        return path && *path ? fopen(path, "a") : nullptr;
    }();
    if (!output)
        return;
    int savedErrno = errno;
    int savedSocketError = *odysseyAmigaOS3SocketError();
    struct timeval now;
    gettimeofday(&now, nullptr);
    fprintf(output, "CurlStream %llu %s code=%d bytes=%lu errno=%d socket_errno=%d time=%ld.%06ld\n",
        static_cast<unsigned long long>(streamID), operation, static_cast<int>(result),
        static_cast<unsigned long>(bytes), savedErrno, savedSocketError, static_cast<long>(now.tv_sec), static_cast<long>(now.tv_usec));
    fflush(output);
    errno = savedErrno;
}
#endif

CurlStream::CurlStream(CurlStreamScheduler& scheduler, CurlStreamID streamID, URL&& url)
    : m_scheduler(scheduler)
    , m_streamID(streamID)
{
    ASSERT(!isMainThread());

    m_curlHandle = WTF::makeUnique<CurlHandle>();

    url.setProtocol(url.protocolIs("wss") ? "https" : "http");
    m_curlHandle->setUrl(WTFMove(url));

    m_curlHandle->enableConnectionOnly();

#if OS(AMIGAOS3)
    // The shared request worker must keep servicing HTTP while TCP, proxy
    // negotiation or TLS connection setup is pending.
    m_addedToMulti = m_scheduler.addStreamHandle(m_curlHandle->handle(), *this);
    if (!m_addedToMulti)
        notifyFailure(CURLE_FAILED_INIT);
#else
    auto errorCode = m_curlHandle->perform();
    if (errorCode != CURLE_OK) {
        notifyFailure(errorCode);
        return;
    }

    m_scheduler.callClientOnMainThread(m_streamID, [streamID = m_streamID](Client& client) {
        client.didOpen(streamID);
    });
#endif
}

#if OS(AMIGAOS3)
void CurlStream::didCompleteConnection(CURLcode result)
{
    traceStream(m_streamID, "connect", result);
    ASSERT(!isMainThread());
    ASSERT(m_addedToMulti && !m_connected);
    if (result != CURLE_OK) {
        notifyFailure(result);
        return;
    }

    // Connect-only easy handles must remain in the multi handle while
    // curl_easy_send/recv use the connection, even after CURLMSG_DONE.
    m_connected = true;
    m_scheduler.callClientOnMainThread(m_streamID, [streamID = m_streamID](Client& client) {
        client.didOpen(streamID);
    });
}
#endif

CurlStream::~CurlStream()
{
    ASSERT(!isMainThread());
    destroyHandle();
}

void CurlStream::destroyHandle()
{
    if (!m_curlHandle)
        return;

#if OS(AMIGAOS3)
    if (m_addedToMulti) {
        m_scheduler.removeStreamHandle(m_curlHandle->handle());
        m_addedToMulti = false;
    }
    m_connected = false;
#endif
    m_curlHandle = nullptr;
}

void CurlStream::send(UniqueArray<uint8_t>&& buffer, size_t length)
{
    ASSERT(!isMainThread());

    if (!m_curlHandle)
        return;

    m_sendBuffers.append(std::make_pair(WTFMove(buffer), length));
}

void CurlStream::appendMonitoringFd(fd_set& readfds, fd_set& writefds, fd_set& exceptfds, int& maxfd)
{
    ASSERT(!isMainThread());

    if (!m_curlHandle)
        return;

    auto socket = m_curlHandle->getActiveSocket();
    if (!socket.has_value()) {
        notifyFailure(socket.error());
        return;
    }

    FD_SET(*socket, &readfds);
    FD_SET(*socket, &exceptfds);

    if (m_sendBuffers.size())
        FD_SET(*socket, &writefds);

    if (maxfd < static_cast<int>(*socket))
        maxfd = *socket;
}

void CurlStream::tryToTransfer(const fd_set& readfds, const fd_set& writefds, const fd_set& exceptfds)
{
    ASSERT(!isMainThread());

    if (!m_curlHandle)
        return;

    auto socket = m_curlHandle->getActiveSocket();
    if (!socket.has_value()) {
        notifyFailure(socket.error());
        return;
    }

    if (FD_ISSET(*socket, &readfds) || FD_ISSET(*socket, &exceptfds))
        tryToReceive();

    if (FD_ISSET(*socket, &writefds))
        tryToSend();
}

void CurlStream::tryToReceive()
{
    if (!m_curlHandle)
        return;

#if OS(AMIGAOS3)
    // Keep the buffer across CURLE_AGAIN instead of allocating on every
    // scheduler tick while an otherwise idle WebSocket remains connected.
    if (!m_receiveBuffer)
        m_receiveBuffer = makeUniqueArray<uint8_t>(kReceiveBufferSize);
    auto& receiveBuffer = m_receiveBuffer;
#else
    auto receiveBuffer = makeUniqueArray<uint8_t>(kReceiveBufferSize);
#endif
    size_t bytesReceived = 0;

    auto errorCode = m_curlHandle->receive(receiveBuffer.get(), kReceiveBufferSize, bytesReceived);
#if OS(AMIGAOS3)
    if (errorCode != CURLE_AGAIN)
        traceStream(m_streamID, "receive", errorCode, bytesReceived);
#endif
    if (errorCode != CURLE_OK) {
        if (errorCode != CURLE_AGAIN)
            notifyFailure(errorCode);
        return;
    }

    // 0 bytes indicates a closed connection.
    if (!bytesReceived)
        destroyHandle();

    m_scheduler.callClientOnMainThread(m_streamID, [streamID = m_streamID, buffer = WTFMove(receiveBuffer), length = bytesReceived](Client& client) mutable {
        client.didReceiveData(streamID, buffer.get(), length);
    });
}

#if OS(AMIGAOS3)
void CurlStream::tryToTransfer()
{
    ASSERT(!isMainThread());
    if (!m_connected)
        return;
    // Connect-only curl handles use nonblocking send/receive. Service one
    // chunk per scheduler tick, including data already buffered by TLS
    // that would not be visible to a socket-readiness check.
    tryToReceive();
    tryToSend();
}
#endif

void CurlStream::tryToSend()
{
    if (!m_curlHandle || !m_sendBuffers.size())
        return;

    auto& elem = m_sendBuffers.first();
    size_t bytesSent = 0;

    auto errorCode = m_curlHandle->send(std::get<0>(elem).get() + m_sendBufferOffset, std::get<1>(elem) - m_sendBufferOffset, bytesSent);
#if OS(AMIGAOS3)
    if (errorCode != CURLE_AGAIN)
        traceStream(m_streamID, "send", errorCode, bytesSent);
#endif
    if (errorCode != CURLE_OK) {
        if (errorCode != CURLE_AGAIN)
            notifyFailure(errorCode);
        return;
    }

    m_sendBufferOffset += bytesSent;

    if (m_sendBufferOffset >= std::get<1>(elem)) {
        m_sendBuffers.remove(0);
        m_sendBufferOffset = 0;
    }

    m_scheduler.callClientOnMainThread(m_streamID, [streamID = m_streamID, length = bytesSent](Client& client) {
        client.didSendData(streamID, length);
    });
}

void CurlStream::notifyFailure(CURLcode errorCode)
{
#if OS(AMIGAOS3)
    traceStream(m_streamID, "failure", errorCode);
#endif
    destroyHandle();

    m_scheduler.callClientOnMainThread(m_streamID, [streamID = m_streamID, errorCode](Client& client) mutable {
        client.didFail(streamID, errorCode);
    });
}

}

#endif
