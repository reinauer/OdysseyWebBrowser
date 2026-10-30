/* Native checks of the production JSC math entry points. */
#include "config.h"
#include "MathCommon.h"
#include <math.h>
#include <stdio.h>

static int failures;
static void check(bool passed, const char* name)
{
    printf("%s %s\n", passed ? "PASS" : "FAIL", name);
    if (!passed)
        ++failures;
}

int main()
{
    using namespace JSC::Math;
    volatile unsigned values[] = { 0, 1, 0x7fffffffU, 0x80000000U, 0xffffffffU };
    const double expected[] = { 0, 1, 2147483647.0, 2147483648.0, 4294967295.0 };
    const float expectedFloat[] = { 0, 1, 2147483648.0f, 2147483648.0f, 4294967296.0f };
    bool unsignedConversion = true;
    for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        unsignedConversion &= static_cast<double>(values[i]) == expected[i];
        volatile float converted = static_cast<float>(values[i]);
        unsignedConversion &= converted == expectedFloat[i];
    }
    check(unsignedConversion, "unsigned 32-bit to floating-point conversion");
    volatile double tie = 1.000000059604644775390625;
    volatile float rounded = static_cast<float>(tie);
    bool tiesToEven = rounded == 1.0f;
    tie = 1.000000178813934326171875;
    rounded = static_cast<float>(tie);
    tiesToEven &= rounded == 1.0000002384185791015625f;
    check(tiesToEven, "double to float ties round to even");
    constexpr double ln2 = 0.69314718055994530942;
    check(fabs(asinhDouble(0.75) - ln2) < 1e-12 &&
          fabs(acoshDouble(1.25) - ln2) < 1e-12 &&
          fabs(atanhDouble(0.6) - ln2) < 1e-12,
          "double inverse hyperbolic functions");
    check(fabs(asinhFloat(0.75f) - ln2) < 1e-6 &&
          fabs(acoshFloat(1.25f) - ln2) < 1e-6 &&
          fabs(atanhFloat(0.6f) - ln2) < 1e-6,
          "float inverse hyperbolic functions");
    printf("double log2(8)=%.17g cbrt(8)=%.17g expm1(ln2)=%.17g log1p(1)=%.17g\n",
           log2Double(8), cbrtDouble(8), expm1Double(ln2), log1pDouble(1));
    check(fabs(log2Double(8) - 3) < 1e-12 && fabs(cbrtDouble(8) - 2) < 1e-12 &&
          fabs(expm1Double(ln2) - 1) < 1e-12 &&
          fabs(log1pDouble(1) - ln2) < 1e-12,
          "double logarithms roots and exponential");
    check(fabs(log2Float(8) - 3) < 1e-6 && fabs(cbrtFloat(8) - 2) < 1e-6 &&
          fabs(expm1Float(static_cast<float>(ln2)) - 1) < 1e-6 &&
          fabs(log1pFloat(1) - ln2) < 1e-6,
          "float logarithms roots and exponential");
    check(std::signbit(log1pDouble(-0.0)) &&
          std::signbit(log1pFloat(-0.0f)) &&
          std::signbit(truncDouble(-0.5)) &&
          std::signbit(truncFloat(-0.5f)), "negative zero preservation");
    printf("JSC math: %d failures\n", failures);
    return failures ? 1 : 0;
}
