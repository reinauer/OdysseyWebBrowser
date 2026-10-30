/* ICU smoke test for the AmigaOS 3 cross-built libraries and data package. */
#include <unicode/utypes.h>
#include <unicode/uclean.h>
#include <unicode/ustring.h>
#include <unicode/unorm2.h>
#include <unicode/ucol.h>
#include <unicode/ubrk.h>
#include <unicode/unum.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(bool passed, const char* name)
{
    printf("%s %s\n", passed ? "PASS" : "FAIL", name);
    if (!passed)
        ++failures;
}

struct WorkerState {
    unsigned index;
    bool passed;
};

#ifdef ICU_THREAD_DIAGNOSTICS
#define TRACE_WORKER(stage) do { \
    printf("ICU worker %u iteration %u: %s\n", state->index, iteration, stage); \
    fflush(stdout); \
} while (0)
#else
#define TRACE_WORKER(stage) do { } while (0)
#endif

static void* exerciseConcurrentICU(void* context)
{
    WorkerState* state = static_cast<WorkerState*>(context);
    bool* passed = &state->passed;
    const UChar a[] = { 'a', 0 }, b[] = { 'b', 0 };
    *passed = true;
    for (unsigned iteration = 0; iteration < 16; ++iteration) {
        UErrorCode status = U_ZERO_ERROR;
        TRACE_WORKER("opening collator");
        UCollator* collator = ucol_open("de_DE", &status);
        TRACE_WORKER("comparing collator");
        if (U_FAILURE(status) || !collator ||
            ucol_strcoll(collator, a, 1, b, 1) != UCOL_LESS)
            *passed = false;
        TRACE_WORKER("closing collator");
        if (collator)
            ucol_close(collator);
        UChar output[64];
        TRACE_WORKER("opening formatter");
        UNumberFormat* format = unum_open(UNUM_DECIMAL, 0, 0, "en_US", 0, &status);
        TRACE_WORKER("formatting number");
        int32_t length = format ? unum_formatDouble(format, 1234.5, output, 64, 0, &status) : 0;
        if (U_FAILURE(status) || length != 7)
            *passed = false;
        TRACE_WORKER("closing formatter");
        if (format)
            unum_close(format);
        TRACE_WORKER("iteration complete");
    }
    return 0;
}

int main()
{
    UErrorCode status = U_ZERO_ERROR;
    u_init(&status);
    check(U_SUCCESS(status), "ICU initialization with linked big-endian data");
    if (U_FAILURE(status)) {
        printf("ICU error: %s\n", u_errorName(status));
        return 1;
    }
    UChar input[] = { 'e', 0x301, 0 };
    UChar output[64];
    const UNormalizer2* nfc = unorm2_getNFCInstance(&status);
    int32_t length = unorm2_normalize(nfc, input, 2, output, 64, &status);
    check(U_SUCCESS(status) && length == 1 && output[0] == 0xe9,
          "NFC normalization");
    UCollator* collator = ucol_open("de_DE", &status);
    const UChar a[] = { 'a', 0 }, b[] = { 'b', 0 };
    check(U_SUCCESS(status) && collator &&
          ucol_strcoll(collator, a, 1, b, 1) == UCOL_LESS, "locale collation");
    if (collator)
        ucol_close(collator);
    const UChar words[] = { 'o', 'n', 'e', ' ', 't', 'w', 'o', 0 };
    UBreakIterator* iterator = ubrk_open(UBRK_WORD, "en_US", words, 7, &status);
    check(U_SUCCESS(status) && iterator && ubrk_first(iterator) == 0 &&
          ubrk_next(iterator) == 3, "word break dictionary and rules");
    if (iterator)
        ubrk_close(iterator);
    UNumberFormat* format = unum_open(UNUM_DECIMAL, 0, 0, "en_US", 0, &status);
    length = format ? unum_formatDouble(format, 1234.5, output, 64, 0, &status) : 0;
    const UChar expected[] = { '1', ',', '2', '3', '4', '.', '5', 0 };
    check(U_SUCCESS(status) && length == 7 &&
          !memcmp(output, expected, 7 * sizeof(UChar)), "decimal formatting");
    if (format)
        unum_close(format);
    pthread_t threads[2];
    WorkerState states[2] = { { 0, false }, { 1, false } };
    bool started[2] = { false, false };
    bool joined = true;
    pthread_attr_t attributes;
    int attrStatus = pthread_attr_init(&attributes);
    if (!attrStatus) {
        attrStatus = pthread_attr_setstacksize(&attributes, 256 * 1024);
        size_t stackSize = 0;
        if (!attrStatus)
            attrStatus = pthread_attr_getstacksize(&attributes, &stackSize);
        printf("ICU worker stack: %lu bytes\n", static_cast<unsigned long>(stackSize));
        fflush(stdout);
        if (!attrStatus) {
            for (unsigned i = 0; i < 2; ++i)
                started[i] = !pthread_create(&threads[i], &attributes, exerciseConcurrentICU, &states[i]);
        }
        pthread_attr_destroy(&attributes);
    }
    for (unsigned i = 0; i < 2; ++i) {
        if (started[i])
            joined = !pthread_join(threads[i], 0) && joined;
    }
    check(started[0] && started[1] && joined && states[0].passed && states[1].passed,
          "concurrent locale caches and formatters");
    u_cleanup();
    printf("ICU %s: %d failures\n", U_ICU_VERSION, failures);
    return failures ? 1 : 0;
}
