/* Keep the C library entry point separate from SDK compiler-helper aliases.
 *
 * Oct29 newlib's mktime calls div(), but its div implementation shares an
 * archive member with __divsi3, __modsi3 and ldiv. Pulling that member defeats
 * the explicit GCC helper selection and conflicts with newlib's other ldiv.
 * These ordinary C operators use the selected GCC helpers and retain the
 * required quotient truncation toward zero and numerator-signed remainder.
 */
#include <stdlib.h>

div_t div(int numerator, int denominator)
{
    div_t result;
    result.quot = numerator / denominator;
    result.rem = numerator % denominator;
    return result;
}
