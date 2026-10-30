// Target libstdc++ adaptation: include from the installed cmath, not WebKit.
// Newlib has real float/double functions but lacks extended long-double C99
// math. Do not enable _GLIBCXX_USE_C99_MATH_FUNCS for this partial API.
#pragma once

#if __cplusplus >= 201103L && !defined(_GLIBCXX_USE_C99_MATH_FUNCS)
namespace std _GLIBCXX_VISIBILITY(default)
{
_GLIBCXX_BEGIN_NAMESPACE_VERSION

using ::round;
using ::roundf;
using ::lround;
using ::lroundf;
using ::hypot;
using ::hypotf;
using ::fmax;
using ::fmaxf;
using ::copysign;
using ::copysignf;
using ::cbrt;
using ::cbrtf;
using ::trunc;
using ::truncf;

inline float round(float __x) { return ::roundf(__x); }
inline long lround(float __x) { return ::lroundf(__x); }
inline float hypot(float __x, float __y) { return ::hypotf(__x, __y); }
// Older newlib nextafter returns x for x == y, retaining the wrong sign
// for opposite signed zeros. C99/C++ require returning y in this case.
inline double nextafter(double __x, double __y) { return __x == __y ? __y : ::nextafter(__x, __y); }
inline float nextafterf(float __x, float __y) { return __x == __y ? __y : ::nextafterf(__x, __y); }
inline float nextafter(float __x, float __y) { return std::nextafterf(__x, __y); }
inline float fmax(float __x, float __y) { return ::fmaxf(__x, __y); }
inline float copysign(float __x, float __y) { return ::copysignf(__x, __y); }
inline float cbrt(float __x) { return ::cbrtf(__x); }
inline float trunc(float __x) { return ::truncf(__x); }

// Reject unsupported extended precision instead of silently narrowing it.
long double round(long double) = delete;
long lround(long double) = delete;
long double hypot(long double, long double) = delete;
long double nextafter(long double, long double) = delete;
long double fmax(long double, long double) = delete;
long double copysign(long double, long double) = delete;
long double cbrt(long double) = delete;
long double trunc(long double) = delete;

template<typename _Tp>
inline typename __gnu_cxx::__enable_if<__is_integer<_Tp>::__value, double>::__type
round(_Tp __x) { return ::round(static_cast<double>(__x)); }

template<typename _Tp>
inline typename __gnu_cxx::__enable_if<__is_integer<_Tp>::__value, long>::__type
lround(_Tp __x) { return ::lround(static_cast<double>(__x)); }

template<typename _Tp>
inline typename __gnu_cxx::__enable_if<__is_integer<_Tp>::__value, double>::__type
cbrt(_Tp __x) { return ::cbrt(static_cast<double>(__x)); }

template<typename _Tp>
inline typename __gnu_cxx::__enable_if<__is_integer<_Tp>::__value, double>::__type
trunc(_Tp __x) { return ::trunc(static_cast<double>(__x)); }

template<typename _Tp, typename _Up>
inline typename __gnu_cxx::__promote_2<_Tp, _Up>::__type
hypot(_Tp __x, _Up __y)
{
    using __type = typename __gnu_cxx::__promote_2<_Tp, _Up>::__type;
    return std::hypot(__type(__x), __type(__y));
}

template<typename _Tp, typename _Up>
inline typename __gnu_cxx::__promote_2<_Tp, _Up>::__type
nextafter(_Tp __x, _Up __y)
{
    using __type = typename __gnu_cxx::__promote_2<_Tp, _Up>::__type;
    return std::nextafter(__type(__x), __type(__y));
}

template<typename _Tp, typename _Up>
inline typename __gnu_cxx::__promote_2<_Tp, _Up>::__type
fmax(_Tp __x, _Up __y)
{
    using __type = typename __gnu_cxx::__promote_2<_Tp, _Up>::__type;
    return std::fmax(__type(__x), __type(__y));
}

template<typename _Tp, typename _Up>
inline typename __gnu_cxx::__promote_2<_Tp, _Up>::__type
copysign(_Tp __x, _Up __y)
{
    using __type = typename __gnu_cxx::__promote_2<_Tp, _Up>::__type;
    return std::copysign(__type(__x), __type(__y));
}

_GLIBCXX_END_NAMESPACE_VERSION
}
#endif
