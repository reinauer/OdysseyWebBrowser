#include <pthread.h>
#include <stdio.h>
#include <string.h>

extern "C" int odysseyAmigaOS3RandomBytes(unsigned char*, size_t);

struct Result {
    unsigned char bytes[128] { };
    bool success { false };
};

static void* readEntropy(void* context)
{
    auto& result = *static_cast<Result*>(context);
    result.success = odysseyAmigaOS3RandomBytes(result.bytes, sizeof(result.bytes));
    return nullptr;
}

int main()
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    puts("Checking task-local AmiSSL random contexts");
    Result first, second;
    readEntropy(&first);
    readEntropy(&second);
    unsigned failures = !first.success || !second.success
        || !memcmp(first.bytes, second.bytes, sizeof(first.bytes));
    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes))
        return 20;
    if (pthread_attr_setstacksize(&attributes, 256 * 1024))
        return 20;
    Result workers[2];
    pthread_t threads[2];
    unsigned created = 0;
    for (; created < 2; ++created) {
        if (pthread_create(&threads[created], &attributes, readEntropy, &workers[created])) {
            ++failures;
            break;
        }
    }
    pthread_attr_destroy(&attributes);
    for (unsigned i = 0; i < created; ++i) {
        if (pthread_join(threads[i], nullptr) || !workers[i].success)
            ++failures;
    }
    if (!memcmp(workers[0].bytes, workers[1].bytes, sizeof(first.bytes)))
        ++failures;
    printf("AmiSSL entropy test: %u failures\n", failures);
    return failures ? 20 : 0;
}
