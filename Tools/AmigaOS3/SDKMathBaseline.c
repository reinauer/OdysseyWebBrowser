/* Standalone, unmodified SDK diagnostic. No Odyssey runtime is linked. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <exec/execbase.h>
#include <proto/exec.h>

static unsigned checks, failures;
static uint64_t bits(double value) { uint64_t n; memcpy(&n, &value, 8); return n; }
static uint32_t floatbits(float value) { uint32_t n; memcpy(&n, &value, 4); return n; }
static double value(uint64_t n) { double result; memcpy(&result, &n, 8); return result; }
static void checkDouble(const char* name, double actual, uint64_t expected, int nan)
{
    uint64_t n = bits(actual);
    int ok = nan ? ((n & UINT64_C(0x7ff0000000000000)) == UINT64_C(0x7ff0000000000000)
        && (n & UINT64_C(0x000fffffffffffff))) : n == expected;
    printf("%s %s actual=%08lx:%08lx expected=%08lx:%08lx%s\n", ok ? "PASS" : "FAIL", name,
        (unsigned long)(n >> 32), (unsigned long)n,
        (unsigned long)(expected >> 32), (unsigned long)expected, nan ? " (any NaN)" : "");
    ++checks; if (!ok) ++failures;
}
static void checkFloat(const char* name, float actual, uint32_t expected)
{
    uint32_t n = floatbits(actual);
    printf("%s %s actual=%08lx expected=%08lx\n", n == expected ? "PASS" : "FAIL",
        name, (unsigned long)n, (unsigned long)expected);
    ++checks; if (n != expected) ++failures;
}
int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    struct ExecBase* base = (struct ExecBase*)*(void**)4;
    printf("Exec %u.%u AttnFlags=%04x\n", base->LibNode.lib_Version,
        base->LibNode.lib_Revision, base->AttnFlags);
    const char* names[] = { "version.library", "mathieeedoubbas.library",
        "mathieeedoubtrans.library", "mathieeesingbas.library", NULL };
    for (unsigned i = 0; names[i]; ++i) {
        struct Library* library = OpenLibrary(names[i], 0);
        if (!library) { printf("MISSING %s\n", names[i]); return 20; }
        printf("LIB %s %u.%u ID=%s\n", names[i], library->lib_Version,
            library->lib_Revision, library->lib_IdString);
        CloseLibrary(library);
    }
    volatile double uintMax = 4294967295.0;
    volatile double tie = 1.000000178813934326171875, negTie = -1.000000178813934326171875;
    volatile double max = DBL_MAX, two = 2, half = .5, one = 1, zero = 0;
    volatile double inf = value(UINT64_C(0x7ff0000000000000));
    volatile double nan = value(UINT64_C(0x7ff8000000000000));
    checkFloat("double UINT_MAX to float", (float)uintMax, UINT32_C(0x4f800000));
    checkFloat("double positive halfway ties-even", (float)tie, UINT32_C(0x3f800002));
    checkFloat("double negative halfway ties-even", (float)negTie, UINT32_C(0xbf800002));
    checkDouble("overflow multiply", max * two, UINT64_C(0x7ff0000000000000), 0);
    checkDouble("overflow add", max + max, UINT64_C(0x7ff0000000000000), 0);
    checkDouble("overflow divide", max / half, UINT64_C(0x7ff0000000000000), 0);
    checkDouble("infinity add", inf + one, UINT64_C(0x7ff0000000000000), 0);
    checkDouble("infinity multiply", inf * two, UINT64_C(0x7ff0000000000000), 0);
    checkDouble("infinity divide", inf / two, UINT64_C(0x7ff0000000000000), 0);
    checkDouble("invalid infinity subtract", inf - inf, 0, 1);
    checkDouble("invalid infinity divide", inf / inf, 0, 1);
    checkDouble("invalid infinity multiply zero", inf * zero, 0, 1);
    checkDouble("invalid zero divide zero", zero / zero, 0, 1);
    checkDouble("NaN add", nan + one, 0, 1);
    checkDouble("NaN multiply", nan * two, 0, 1);
    checkDouble("NaN divide", nan / two, 0, 1);
    unsigned predicates = (nan == one) | ((nan != one) << 1) | ((nan < one) << 2)
        | ((nan <= one) << 3) | ((nan > one) << 4) | ((nan >= one) << 5);
    printf("%s NaN comparisons actual=%02x expected=02\n", predicates == 2 ? "PASS" : "FAIL", predicates);
    ++checks; if (predicates != 2) ++failures;
    volatile unsigned long unsignedMax = UINT32_MAX;
    checkDouble("unsigned UINT_MAX to double", (double)unsignedMax, UINT64_C(0x41efffffffe00000), 0);
    printf("Observed SDK arithmetic: %u checks, %u mismatches\n", checks, failures);
    puts("COMPLETE unmodified SDK arithmetic baseline");
    /* A zero return means observation completed, not that arithmetic passed. */
    return 0;
}
