/* Native C library/compiler division ABI and archive-extraction regression. */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

extern int __divsi3(int, int);
extern int __modsi3(int, int);
extern unsigned __udivsi3(unsigned, unsigned);
extern unsigned __umodsi3(unsigned, unsigned);

int main(void)
{
    static const struct { int numerator, denominator, quotient, remainder; } cases[] = {
        { 73, 7, 10, 3 }, { -73, 7, -10, -3 },
        { 73, -7, -10, 3 }, { -73, -7, 10, -3 },
        { 1, 2, 0, 1 }, { -1, 2, 0, -1 },
        { 0, 7, 0, 0 }, { INT_MIN, 1, INT_MIN, 0 },
        { INT_MAX, -1, -INT_MAX, 0 }, { INT_MIN, 3, -715827882, -2 },
        { INT_MIN, -3, 715827882, -2 }, { INT_MAX, 3, 715827882, 1 },
        { INT_MIN, INT_MIN, 1, 0 }, { INT_MAX, INT_MIN, 0, INT_MAX }
    };
    div_t (*volatile division)(int, int) = div;
    ldiv_t (*volatile longDivision)(long, long) = ldiv;
    unsigned checks = 0, failures = 0;
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        div_t a = division(cases[i].numerator, cases[i].denominator);
        ldiv_t b = longDivision(cases[i].numerator, cases[i].denominator);
        ++checks;
        if (a.quot != cases[i].quotient || a.rem != cases[i].remainder)
            ++failures;
        ++checks;
        if (b.quot != cases[i].quotient || b.rem != cases[i].remainder)
            ++failures;
        ++checks;
        if (__divsi3(cases[i].numerator, cases[i].denominator) != cases[i].quotient
            || __modsi3(cases[i].numerator, cases[i].denominator) != cases[i].remainder)
            ++failures;
    }
    ++checks;
    if (__udivsi3(UINT_MAX, 3) != 1431655765U || __umodsi3(UINT_MAX, 3))
        ++failures;
    ++checks;
    if (__udivsi3(0x80000000U, 0x7fffffffU) != 1 || __umodsi3(0x80000000U, 0x7fffffffU) != 1)
        ++failures;

    /* mktime's real SDK div reference triggered the full-browser failure. */
    setenv("TZ", "UTC0", 1);
    tzset();
    struct tm date = { 0 };
    date.tm_year = 70;
    date.tm_mday = 2;
    time_t stamp = mktime(&date);
    ++checks;
    if (stamp != 86400 || date.tm_year != 70 || date.tm_mon || date.tm_mday != 2)
        ++failures;
    printf("Integer division: %u checks, %u failures; mktime=%ld\n", checks, failures, (long)stamp);
    return failures ? 20 : 0;
}
