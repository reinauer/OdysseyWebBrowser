#include <cmath>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <type_traits>
#include <pthread.h>

static unsigned failures;
static void check(bool pass, const char* name)
{
    std::printf("%s %s\n", pass ? "PASS" : "FAIL", name);
    if (!pass)
        ++failures;
}

static uint64_t bits(double value)
{
    uint64_t result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

static uint32_t bits(float value)
{
    uint32_t result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

static void* concurrentArithmetic(void* pointer)
{
    auto* errors = static_cast<unsigned*>(pointer);
    volatile double maximum = DBL_MAX, two = 2, zero = 0;
    volatile double nan = std::numeric_limits<double>::quiet_NaN();
    for (unsigned i = 1; i <= 200; ++i) {
        volatile double value = i;
        if (!std::isinf(maximum * two) || !std::isnan(zero / zero)
            || value * two / two != value || nan < value || nan >= value)
            ++*errors;
    }
    return nullptr;
}

static_assert(std::is_same<decltype(std::round(1)), double>::value);
static_assert(std::is_same<decltype(std::round(1.f)), float>::value);
static_assert(std::is_same<decltype(std::lround(1.f)), long>::value);
static_assert(std::is_same<decltype(std::hypot(1.f, 2.f)), float>::value);
static_assert(std::is_same<decltype(std::hypot(1.f, 2)), double>::value);
static_assert(std::is_same<decltype(std::nextafter(1, 2)), double>::value);
static_assert(std::is_same<decltype(std::nextafter(1.f, 2.)), double>::value);

int main()
{
    volatile double zero = 0, negativeZero = -0., half = .5, one = 1, two = 2;
    volatile float zeroF = 0, negativeZeroF = -0.f, halfF = .5f, oneF = 1, twoF = 2;
    check(bits(std::round(zero)) == 0 && bits(std::round(negativeZero)) == UINT64_C(0x8000000000000000), "round double signed zero");
    check(bits(std::round(zeroF)) == 0 && bits(std::round(negativeZeroF)) == UINT32_C(0x80000000), "round float signed zero");
    check(std::round(half) == 1 && std::round(-half) == -1 && std::round(one + half) == 2 && std::round(-one - half) == -2, "round double ties away from zero");
    check(std::round(halfF) == 1 && std::round(-halfF) == -1 && std::round(oneF + halfF) == 2, "round float ties away from zero");
    check(std::lround(one + half) == 2 && std::lround(-one - half) == -2 && std::lround(oneF + halfF) == 2, "lround float and double ties");
    check(bits(std::nextafter(one, two)) == UINT64_C(0x3ff0000000000001), "nextafter double adjacent value");
    check(bits(std::nextafter(oneF, twoF)) == UINT32_C(0x3f800001), "nextafter float adjacent value");
    check(bits(std::nextafter(zero, -one)) == UINT64_C(0x8000000000000001), "nextafter double negative subnormal");
    check(bits(std::nextafter(zeroF, -oneF)) == UINT32_C(0x80000001), "nextafter float negative subnormal");
    check(bits(std::nextafter(negativeZero, zero)) == 0 && bits(std::nextafter(negativeZeroF, zeroF)) == 0, "nextafter equal zero takes destination sign");
    volatile double three = 3, four = 4, six = 6, large = DBL_MAX / 2, maximum = DBL_MAX;
    volatile float threeF = 3, fourF = 4, largeF = FLT_MAX / 2, maximumF = FLT_MAX;
    check(std::hypot(three, four) == 5 && std::hypot(threeF, fourF) == 5 && std::hypot(3, 4) == 5, "hypot float double and integral promotion");
    check(std::hypot(large, large) > large && std::isfinite(std::hypot(large, large)), "hypot double avoids intermediate overflow");
    check(std::hypot(largeF, largeF) > largeF && std::isfinite(std::hypot(largeF, largeF)), "hypot float avoids intermediate overflow");
    check(std::isinf(std::hypot(maximum, maximum)) && std::isinf(std::hypot(maximumF, maximumF)), "hypot true overflow returns infinity");
    check(bits(std::hypot(negativeZero, zero)) == 0 && bits(std::hypot(negativeZeroF, zeroF)) == 0, "hypot zero result is positive");
    check(std::abs(std::hypot(two, three, six) - 7) <= 2 * DBL_EPSILON * 7
        && std::abs(std::hypot(2, 3, 6) - 7) <= 2 * DBL_EPSILON * 7, "existing C++17 three-argument hypot within rounding tolerance");
    auto signedZero = bits(std::nextafter(negativeZero, zero));
    auto overflow = bits(std::hypot(maximum, maximum));
    auto threeArg = bits(std::hypot(two, three, six));
    auto multiply = bits(maximum * two);
    auto add = bits(maximum + maximum);
    std::printf("Diagnostic nextzero=%08lx:%08lx float=%08lx overflow=%08lx:%08lx float=%08lx hypot3=%08lx:%08lx\n",
        static_cast<unsigned long>(signedZero >> 32), static_cast<unsigned long>(signedZero),
        static_cast<unsigned long>(bits(std::nextafter(negativeZeroF, zeroF))),
        static_cast<unsigned long>(overflow >> 32), static_cast<unsigned long>(overflow),
        static_cast<unsigned long>(bits(std::hypot(maximumF, maximumF))),
        static_cast<unsigned long>(threeArg >> 32), static_cast<unsigned long>(threeArg));
    std::printf("Diagnostic overflow multiply=%08lx:%08lx add=%08lx:%08lx\n",
        static_cast<unsigned long>(multiply >> 32), static_cast<unsigned long>(multiply),
        static_cast<unsigned long>(add >> 32), static_cast<unsigned long>(add));
    volatile double nan = std::numeric_limits<double>::quiet_NaN();
    volatile float nanF = std::numeric_limits<float>::quiet_NaN();
    volatile double infinity = std::numeric_limits<double>::infinity();
    check(std::isinf(maximum * two) && std::isinf(maximum + maximum)
        && std::isinf(maximum / half), "double arithmetic overflow returns infinity");
    check(std::isinf(infinity + one) && std::isinf(infinity * two) && std::isinf(infinity / two), "double arithmetic propagates infinity");
    check(std::isnan(infinity - infinity) && std::isnan(infinity / infinity)
        && std::isnan(infinity * zero), "invalid double arithmetic produces NaN");
    check(std::isnan(nan + one) && std::isnan(nan * two) && std::isnan(nan / two), "double arithmetic propagates NaN");
    check(!(nan == one) && nan != one && !(nan < one) && !(nan <= one)
        && !(nan > one) && !(nan >= one), "double comparison predicates handle unordered operands");
    volatile double smallest = std::numeric_limits<double>::denorm_min();
    check(bits(smallest / two) == 0 && bits((smallest + smallest + smallest) / two) == 2,
        "double gradual underflow rounds ties to even");
    check(std::fmax(nan, two) == two && std::fmax(two, nan) == two
        && std::fmax(nanF, twoF) == twoF && std::fmax(twoF, nanF) == twoF, "fmax ignores one NaN in either order");
    check(std::copysign(two, negativeZero) == -two && std::copysign(twoF, negativeZeroF) == -twoF
        && std::copysign(-2, 1) == 2, "copysign handles signed zero and integral promotion");
    check(bits(std::trunc(-half)) == UINT64_C(0x8000000000000000)
        && bits(std::trunc(-halfF)) == UINT32_C(0x80000000), "trunc preserves negative zero");
    check(std::trunc(one + half) == 1 && std::trunc(-oneF - halfF) == -1, "trunc rounds toward zero");
    volatile double minusEight = -8;
    volatile float minusEightF = -8;
    check(std::abs(std::cbrt(minusEight) + 2) < 1e-14 && std::abs(std::cbrt(minusEightF) + 2) < 1e-6f
        && std::abs(std::cbrt(-8) + 2) < 1e-14, "cbrt negative inputs and integral promotion");
    pthread_t workers[4];
    unsigned errors[4] = { }, started = 0;
    for (; started < 4; ++started) {
        if (pthread_create(&workers[started], nullptr, concurrentArithmetic, &errors[started]))
            break;
    }
    bool threadsOK = started == 4;
    for (unsigned i = 0; i < started; ++i) {
        if (pthread_join(workers[i], nullptr) || errors[i])
            threadsOK = false;
    }
    check(threadsOK, "concurrent software double arithmetic and comparisons");
    std::printf("math test: %u failures\n", failures);
    return failures ? 20 : 0;
}
