#include <wtf/Platform.h>
#include <wtf/SaturatedArithmetic.h>
#include <cstdio>
#include <cstdint>
#include <limits>

template<typename T> static unsigned checkType()
{
    const T minimum = std::numeric_limits<T>::min();
    const T maximum = std::numeric_limits<T>::max();
    const T values[] = { minimum, minimum + 1, -1, 0, 1, maximum - 1, maximum };
    unsigned failures = 0;
    auto clamp = [=](int64_t value) -> T {
        return value < minimum ? minimum : value > maximum ? maximum : value;
    };
    for (T a : values) {
        for (T b : values) {
            failures += WTF::saturatedSum<T>(a, b) != clamp(int64_t(a) + b);
            failures += WTF::saturatedDifference<T>(a, b) != clamp(int64_t(a) - b);
        }
    }
    return failures;
}

int main()
{
    static_assert(!std::is_same_v<int, int32_t>);
    unsigned failures = checkType<int>() + checkType<int32_t>();
    std::printf("Native saturated arithmetic: 196 checks, %u failures\n", failures);
    return failures ? 20 : 0;
}
