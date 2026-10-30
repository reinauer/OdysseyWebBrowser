/* Native curl/AmiSSL smoke test. TLS verification is enabled on every request. */
#define _DEFAULT_SOURCE 1
#define __USE_NEW_TIMEVAL__ 1
#include <curl/curl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <proto/bsdsocket.h>
#include <utility/tagitem.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <pthread.h>
#include "../../Source/WebCore/platform/network/curl/AmigaOS3AmiSSL.h"

static unsigned failures;
static void check(int passed, const char* name)
{
    printf("%s %s\n", passed ? "PASS" : "FAIL", name);
    if (!passed)
        ++failures;
}

static void testFiles(void)
{
    const char* path = "T:odyssey-curl-files";
    char data[32];
    struct stat info;
    int fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0600);
    int valid = fd >= 0;
    if (valid) {
        valid = write(fd, "abcdefghij", 10) == 10 && lseek(fd, 3, SEEK_SET) == 3
            && !ftruncate(fd, 5) && lseek(fd, 0, SEEK_CUR) == 3
            && !fstat(fd, &info) && info.st_size == 5;
        if (valid) {
            valid = !ftruncate(fd, 32) && lseek(fd, 0, SEEK_CUR) == 3
                && lseek(fd, 0, SEEK_SET) == 0 && read(fd, data, 32) == 32
                && !memcmp(data, "abcde", 5);
            for (unsigned i = 5; valid && i < sizeof(data); ++i)
                valid = !data[i];
        }
        if (valid) {
            errno = 0;
            valid = !isatty(fd) && errno == ENOTTY;
        }
        close(fd);
        unlink(path);
    }
    check(valid, "native file truncation, zero extension and descriptor type");
}

struct Response {
    char text[128];
    size_t length;
};

static size_t receive(char* bytes, size_t size, size_t count, void* context)
{
    struct Response* response = context;
    size_t length = size * count;
    if (length > sizeof(response->text) - response->length - 1)
        return 0;
    memcpy(response->text + response->length, bytes, length);
    response->length += length;
    response->text[response->length] = 0;
    return length;
}

static CURLcode request(const char* url, const char* ca, int expectBody)
{
    struct Response response = { { 0 }, 0 };
    char error[CURL_ERROR_SIZE] = { 0 };
    CURL* curl = curl_easy_init();
    if (!curl)
        return CURLE_FAILED_INIT;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_PROXY, "");
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receive);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error);
    if (ca)
        curl_easy_setopt(curl, CURLOPT_CAINFO, ca);
    CURLcode result = curl_easy_perform(curl);
    if (result)
        printf("curl %s: %s (%s)\n", url, curl_easy_strerror(result), error);
    if (!result && expectBody) {
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        if (status != 200 || strcmp(response.text, "odyssey-amigaos3-curl\n"))
            result = CURLE_WEIRD_SERVER_REPLY;
    }
    curl_easy_cleanup(curl);
    return result;
}

/* Only the disposable native test changes its clock. The host fixture supplies
 * UTC; timer.device stores local seconds since 1978. Production uses the
 * user's existing clock and locale preferences unchanged. */
static int setFixtureClock(void)
{
    struct timeval before;
    struct timezone zone;
    if (gettimeofday(&before, &zone))
        return 0;
    FILE* input = fopen("SYS:curl-test-utc.txt", "r");
    unsigned long utc = 0;
    if (!input)
        return 0;
    int parsed = fscanf(input, "%lu", &utc);
    fclose(input);
    printf("fixture before UTC %ld locale minutes west %d host UTC %lu\n",
        (long)before.tv_sec, zone.tz_minuteswest, utc);
    if (parsed != 1 || utc < 252460800UL)
        return 0;
    struct MsgPort* port = CreateMsgPort();
    if (!port)
        return 0;
    struct TimeRequest request = { 0 };
    request.tr_node.io_Message.mn_ReplyPort = port;
    request.tr_node.io_Message.mn_Length = sizeof(request);
    int success = 0;
    if (!OpenDevice(TIMERNAME, UNIT_MICROHZ, &request.tr_node, 0)) {
        request.tr_node.io_Command = TR_SETSYSTIME;
        request.tr_time.tv_secs = utc - 252460800UL - zone.tz_minuteswest * 60;
        request.tr_time.tv_micro = 0;
        success = !DoIO(&request.tr_node);
        CloseDevice(&request.tr_node);
    }
    DeleteMsgPort(port);
    struct timeval after = { 0 };
    success = success && !gettimeofday(&after, NULL) && after.tv_sec == utc;
    printf("fixture corrected UTC %ld\n", (long)after.tv_sec);
    return success;
}

static void* worker(void* unused)
{
    (void)unused;
    int initialized = odysseyAmigaOS3AmiSSLWorkerInitialize();
    check(initialized, "worker AmiSSL and socket initialization");
    if (!initialized)
        return NULL;
    check(request("http://localhost:18787/", NULL, 1) == CURLE_OK,
          "worker HTTP response");
    check(request("https://localhost:18443/", NULL, 0) == CURLE_PEER_FAILED_VERIFICATION,
          "untrusted certificate rejected");
    check(request("https://localhost:18443/", "SYS:curl-test-ca.pem", 1) == CURLE_OK,
          "HTTPS with verified certificate and hostname");
    check(request("https://127.0.0.1:18443/", "SYS:curl-test-ca.pem", 0) == CURLE_PEER_FAILED_VERIFICATION,
          "certificate hostname mismatch rejected");
    odysseyAmigaOS3AmiSSLWorkerCleanup();
    return NULL;
}

int main(void)
{
    check(setFixtureClock(), "isolated guest UTC clock synchronization");
    testFiles();
    int initialized = odysseyAmigaOS3AmiSSLInitialize();
    check(initialized, "owner AmiSSL and socket initialization");
    if (!initialized)
        return 1;
    CURLcode result = curl_global_init(CURL_GLOBAL_DEFAULT);
    check(result == CURLE_OK, "curl global initialization");
    if (result) {
        odysseyAmigaOS3AmiSSLCleanup();
        return 1;
    }
    const curl_version_info_data* version = curl_version_info(CURLVERSION_NOW);
    printf("curl %s TLS %s\n", version->version, version->ssl_version);
    check(version->features & CURL_VERSION_SSL, "AmiSSL TLS backend");
    for (unsigned i = 0; i < 2; ++i) {
        pthread_attr_t attr;
        pthread_t thread;
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, 262144);
        int created = pthread_create(&thread, &attr, worker, NULL);
        pthread_attr_destroy(&attr);
        check(!created, "create network worker");
        if (!created)
            check(!pthread_join(thread, NULL), "join network worker after AmiSSL cleanup");
        check(request("http://localhost:18787/", NULL, 1) == CURLE_OK,
              "owner socket restored after worker shutdown");
    }
    curl_global_cleanup();
    odysseyAmigaOS3AmiSSLCleanup();
    printf("curl native: %u failures\n", failures);
    return failures ? 1 : 0;
}
