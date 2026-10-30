// Native AmigaOS3 integration test for the initial WTF platform backend.
#include <wtf/FastMalloc.h>
#include <wtf/OSAllocator.h>
#include <wtf/PageBlock.h>
#include <wtf/MonotonicTime.h>
#include <wtf/WallTime.h>
#include <wtf/StackBounds.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/SaturatedArithmetic.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/muimaster.h>

#if !OS(AMIGAOS3) || !CPU(M68K) || !CPU(BIG_ENDIAN) || !USE(JSVALUE32_64) || !ENABLE(C_LOOP)
#error Incorrect OS3 platform configuration
#endif
#if ENABLE(JIT) || ENABLE(FTL_JIT) || ENABLE(WEBASSEMBLY)
#error Unsupported code generators enabled
#endif

struct Library* MUIMasterBase;
static unsigned failures;
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); ++failures; \
} } while (0)

static void checkAllocations()
{
    const size_t alignments[] = { 1, 2, 4, 8, 16, 4096, 16384 };
    const size_t sizes[] = { 0, 1, 17, 4095, 4096, 8193 };
    for (auto alignment : alignments) {
        for (auto size : sizes) {
            void* memory = WTF::tryFastAlignedMalloc(alignment, size);
            CHECK(memory);
            if (!memory)
                continue;
            CHECK(!(reinterpret_cast<uintptr_t>(memory) & (alignment - 1)));
            memset(memory, 0xa5, size);
            WTF::fastAlignedFree(memory);
        }
    }
    CHECK(!WTF::tryFastAlignedMalloc(0, 16));
    CHECK(!WTF::tryFastAlignedMalloc(3, 16));
    CHECK(!WTF::tryFastAlignedMalloc(4096, SIZE_MAX));
    CHECK(!WTF::tryFastAlignedMalloc(size_t(1) << 31, SIZE_MAX));
    WTF::fastAlignedFree(nullptr);

    size_t size = 2 * WTF::pageSize();
    auto* memory = static_cast<unsigned char*>(WTF::OSAllocator::reserveUncommitted(size));
    CHECK(WTF::isPageAligned(memory));
    for (size_t i = 0; i < size; ++i)
        CHECK(memory[i] == 0);
    memset(memory, 0xa5, size);
    WTF::OSAllocator::decommit(memory + WTF::pageSize(), WTF::pageSize());
    WTF::OSAllocator::commit(memory + WTF::pageSize(), WTF::pageSize(), true, false);
    for (size_t i = 0; i < size; ++i)
        CHECK(memory[i] == (i < WTF::pageSize() ? 0xa5 : 0));
    WTF::OSAllocator::releaseDecommitted(memory, size);

    void* committed = WTF::OSAllocator::reserveAndCommit(size);
    CHECK(WTF::isPageAligned(committed));
    WTF::OSAllocator::decommitAndRelease(committed, size);
}

static bool stackContainsLocal()
{
    int local;
    auto bounds = WTF::StackBounds::currentThreadStackBounds();
    return !bounds.isEmpty() && bounds.size() && bounds.contains(&local)
        && bounds.contains(WTF::currentStackPointer());
}

static void* worker(void* context)
{
    *static_cast<bool*>(context) = stackContainsLocal();
    return context;
}

static void checkThreads()
{
    CHECK(stackContainsLocal());
    bool validWorkerStack = false;
    pthread_t thread;
    int status = pthread_create(&thread, nullptr, worker, &validWorkerStack);
    CHECK(!status);
    if (!status) {
        void* result = nullptr;
        CHECK(!pthread_join(thread, &result));
        CHECK(result == &validWorkerStack);
        CHECK(validWorkerStack);
    }
}

static void checkTime()
{
    auto before = WTF::MonotonicTime::now();
    struct timeval now;
    CHECK(!gettimeofday(&now, nullptr));
    CHECK(now.tv_sec > 252460800L);
    CHECK(now.tv_usec >= 0 && now.tv_usec < 1000000);
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
    struct timespec deadline = { now.tv_sec, (now.tv_usec + 20000) * 1000L };
    if (deadline.tv_nsec >= 1000000000L) {
        ++deadline.tv_sec;
        deadline.tv_nsec -= 1000000000L;
    }
    CHECK(!pthread_mutex_lock(&mutex));
    CHECK(pthread_cond_timedwait(&condition, &mutex, &deadline) == ETIMEDOUT);
    // The SDK's NDK timeval uses unsigned fields. An inline timersub must
    // still normalize the microsecond borrow across a second boundary.
    CHECK(!gettimeofday(&now, nullptr));
    deadline = { now.tv_sec + 1, 0 };
    CHECK(pthread_cond_timedwait(&condition, &mutex, &deadline) == ETIMEDOUT);
    CHECK(!gettimeofday(&now, nullptr));
    deadline = { now.tv_sec - 1, now.tv_usec * 1000L };
    CHECK(pthread_cond_timedwait(&condition, &mutex, &deadline) == ETIMEDOUT);
    deadline.tv_nsec = 1000000000L;
    CHECK(pthread_cond_timedwait(&condition, &mutex, &deadline) == EINVAL);
    CHECK(!pthread_mutex_unlock(&mutex));
    CHECK(!pthread_cond_destroy(&condition));
    CHECK(!pthread_mutex_destroy(&mutex));
    auto after = WTF::MonotonicTime::now();
    CHECK(after >= before);
    CHECK((after - before).seconds() < 5);
    CHECK(WTF::WallTime::now().secondsSinceEpoch().seconds() >= now.tv_sec);
}

unsigned checkAmigaOS3MUICallbacks();
unsigned checkAmigaOS3ClipboardNotifications();

static void checkMUI()
{
    MUIMasterBase = OpenLibrary("muimaster.library", 19);
    CHECK(MUIMasterBase);
    if (!MUIMasterBase) {
        const char* dependencies[] = { "asl.library", "commodities.library",
            "rexxsyslib.library", "keymap.library", "locale.library",
            "datatypes.library", "iffparse.library", "diskfont.library",
            "icon.library", "workbench.library" };
        for (auto name : dependencies) {
            auto* library = OpenLibrary(name, 0);
            printf("MUI dependency %s: %s\n", name, library ? "present" : "missing");
            if (library)
                CloseLibrary(library);
        }
        return;
    }
    Object* text = MUI_NewObject(MUIC_Text, MUIA_Text_Contents,
        "Odyssey OS3 platform test", TAG_DONE);
    CHECK(text);
    if (text)
        MUI_DisposeObject(text);
    failures += checkAmigaOS3MUICallbacks();
    failures += checkAmigaOS3ClipboardNotifications();
    CloseLibrary(MUIMasterBase);
    MUIMasterBase = nullptr;
}

unsigned checkAmigaOS3ThreadContexts();

static void checkArithmetic()
{
    uint32_t result;
    CHECK(WTF::safeAdd(uint32_t(10), -3, result) && result == 7);
    CHECK(!WTF::safeAdd(uint32_t(0), -1, result));
    CHECK(!WTF::safeAdd(UINT32_MAX, 1, result));
    CHECK(WTF::safeSub(uint32_t(10), int32_t(3), result) && result == 7);
    CHECK(!WTF::safeSub(uint32_t(0), int32_t(1), result));
    CHECK(WTF::safeMultiply(uint32_t(10), int32_t(3), result) && result == 30);
    CHECK(!WTF::safeMultiply(UINT32_MAX, 2, result));
    CHECK(WTF::saturatedSum<uint32_t>(1u, 2u, 3u) == 6);
    CHECK(WTF::saturatedSum<uint32_t>(UINT32_MAX, 1u) == UINT32_MAX);
    CHECK(WTF::saturatedSum<int32_t>(INT32_MAX, int32_t(1)) == INT32_MAX);
    CHECK(WTF::saturatedDifference<int32_t>(INT32_MIN, int32_t(1)) == INT32_MIN);
}

int main(int argc, char** argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    puts("Checking WTF allocation and page reservations");
    checkAllocations();
    puts("Checking Exec stack bounds and SDK pthreads");
    checkThreads();
    puts("Checking POSIX time and pthread condition timeout");
    checkTime();
    checkArithmetic();
    failures += checkAmigaOS3ThreadContexts();
    if (argc == 2 && !strcmp(argv[1], "--mui"))
        checkMUI();
    printf("Odyssey OS3 platform test: %u failures\n", failures);
    return failures ? 20 : 0;
}
