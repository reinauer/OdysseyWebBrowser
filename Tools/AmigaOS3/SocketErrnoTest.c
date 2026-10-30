/* Exercise curl's real receive error path with a controlled task switch. */
#define _DEFAULT_SOURCE 1
#include <curl/curl.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <proto/bsdsocket.h>
#include "../../Source/WebCore/platform/network/curl/AmigaOS3AmiSSL.h"

#ifndef SOCKET_ERRNO_PROBE_URL
#define SOCKET_ERRNO_PROBE_URL "http://127.0.0.1:18788/"
#endif

static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static int phase, armed, globalBefore, globalAfter, socketBefore, socketAfter;
static unsigned failures;
static const char* url = SOCKET_ERRNO_PROBE_URL;

/* Called only by the private cf-socket.c copy, after sread and before curl
 * classifies its error. The main task changes its own errno while we wait. */
void odysseySocketErrnoProbeAfterReceive(long result)
{
    if (!armed || result >= 0)
        return;
    armed = 0;
    globalBefore = errno;
    socketBefore = Errno();
    pthread_mutex_lock(&gate);
    phase = 1;
    pthread_cond_broadcast(&changed);
    while (phase == 1)
        pthread_cond_wait(&changed, &gate);
    pthread_mutex_unlock(&gate);
    globalAfter = errno;
    socketAfter = Errno();
}

static void* worker(void* unused)
{
    (void)unused;
    CURLM* multi = NULL;
    CURL* easy = NULL;
    int initialized = odysseyAmigaOS3AmiSSLWorkerInitialize();
    if (!initialized) {
        ++failures;
        goto cleanup;
    }
#ifdef SOCKET_ERRNO_PROBE_BASELINE
    /* Reproduce the old production binding without changing SDK files or
     * the installed library. Fixed mode retains the production binding. */
    struct TagItem tags[] = {
        { SBTM_SETVAL(SBTC_ERRNOPTR(sizeof(errno))), (ULONG)&errno },
        { TAG_DONE, 0 }
    };
    if (SocketBaseTagList(tags)) {
        ++failures;
        goto cleanup;
    }
#endif
    multi = curl_multi_init();
    easy = curl_easy_init();
    if (!multi || !easy) {
        ++failures;
        goto cleanup;
    }
    curl_easy_setopt(easy, CURLOPT_URL, url);
    curl_easy_setopt(easy, CURLOPT_PROXY, "");
    curl_easy_setopt(easy, CURLOPT_CONNECT_ONLY, 1L);
    curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT, 8L);
    curl_multi_add_handle(multi, easy);
    int complete = 0;
    CURLcode connection = CURLE_FAILED_INIT;
    for (unsigned i = 0; i < 800 && !complete; ++i) {
        int active, remaining;
        curl_multi_perform(multi, &active);
        CURLMsg* message;
        while ((message = curl_multi_info_read(multi, &remaining))) {
            if (message->msg == CURLMSG_DONE) {
                complete = 1;
                connection = message->data.result;
            }
        }
        usleep(10000);
    }
    printf("Connection completion=%d result=%d worker errno=%p\n",
        complete, connection, &errno);
    if (!complete || connection != CURLE_OK) {
        ++failures;
        goto cleanup;
    }
    armed = 1;
    char buffer[32];
    size_t count = 0;
    CURLcode received = curl_easy_recv(easy, buffer, sizeof(buffer), &count);
    long osError = 0;
    curl_easy_getinfo(easy, CURLINFO_OS_ERRNO, &osError);
    printf("Global errno %d -> %d; socket error %d -> %d; "
        "curl=%d OS_ERRNO=%ld bytes=%lu\n", globalBefore, globalAfter,
        socketBefore, socketAfter, received, osError, (unsigned long)count);
    if (globalAfter != ENOENT || socketBefore != EWOULDBLOCK || count)
        ++failures;
#ifdef SOCKET_ERRNO_PROBE_BASELINE
    if (globalBefore != EWOULDBLOCK || socketAfter != EWOULDBLOCK
        || received != CURLE_RECV_ERROR || osError != ENOENT)
        ++failures;
#else
    if (socketAfter != EWOULDBLOCK || received != CURLE_AGAIN || osError)
        ++failures;
#endif

cleanup:
    pthread_mutex_lock(&gate);
    if (!phase) {
        phase = -1;
        pthread_cond_broadcast(&changed);
    }
    pthread_mutex_unlock(&gate);
    if (easy) {
        if (multi)
            curl_multi_remove_handle(multi, easy);
        curl_easy_cleanup(easy);
    }
    if (multi)
        curl_multi_cleanup(multi);
    if (initialized)
        odysseyAmigaOS3AmiSSLWorkerCleanup();
    return NULL;
}

int main(int argc, char** argv)
{
    char urlBuffer[512];
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 1) {
        if (!strncmp(argv[1], "http://", 7))
            url = argv[1];
        else {
            /* run-platform-test.py --script copies this one-line URL file. */
            FILE* input = fopen(argv[1], "r");
            if (!input || !fgets(urlBuffer, sizeof(urlBuffer), input))
                return 20;
            fclose(input);
            urlBuffer[strcspn(urlBuffer, "\r\n")] = 0;
            url = urlBuffer;
        }
    }
    if (strncmp(url, "http://", 7))
        return 20;
    printf("Socket errno probe: URL=%s owner errno=%p\n", url, &errno);
    if (!odysseyAmigaOS3AmiSSLInitialize())
        return 20;
    if (curl_global_init(CURL_GLOBAL_ALL)) {
        odysseyAmigaOS3AmiSSLCleanup();
        return 20;
    }
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, 262144);
    pthread_t thread;
    int result = pthread_create(&thread, &attributes, worker, NULL);
    pthread_attr_destroy(&attributes);
    if (result)
        ++failures;
    else {
        pthread_mutex_lock(&gate);
        while (!phase)
            pthread_cond_wait(&changed, &gate);
        if (phase == 1) {
            phase = 2;
            errno = ENOENT;
            pthread_cond_broadcast(&changed);
        }
        pthread_mutex_unlock(&gate);
        if (pthread_join(thread, NULL))
            ++failures;
    }
    curl_global_cleanup();
    odysseyAmigaOS3AmiSSLCleanup();
#ifdef SOCKET_ERRNO_PROBE_BASELINE
    printf("Socket errno baseline: %u failures reproducing the defect\n", failures);
#else
    printf("Socket errno fixed: %u failures preserving nonblocking receive\n", failures);
#endif
    return failures ? 20 : 0;
}
