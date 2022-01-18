# Eval

This document describes the evaluation language used in the expression
parameters of the `Eval*` nodes.

Basic operators (`+`, `-`, `*`, `/`) are supported, as well as parenthesis.
Whitespaces (including line breaks) are allowed as well.


## Constants

| Constant | Description                                            |
|----------|--------------------------------------------------------|
| `e`      | [Euler's number][euler], base of the natural logarithm |
| `phi`    | [Golden ratio][phi]                                    |
| `pi`     | [π][pi]                                                |
| `tau`    | [2π][tau]                                              |

[euler]: https://en.wikipedia.org/wiki/E_(mathematical_constant)
[phi]: https://en.wikipedia.org/wiki/Golden_ratio
[pi]: https://en.wikipedia.org/wiki/Pi
[tau]: https://en.wikipedia.org/wiki/Tau_(mathematical_constant)


## Functions

| Function            | Description                                                                    |
|---------------------|--------------------------------------------------------------------------------|
| `abs(x)`            | absolute value of `x`                                                          |
| `acos(x)`           | arc cosine of `x`                                                              |
| `acosh(x)`          | inverse hyperbolic cosine of `x`                                               |
| `asin(x)`           | arc sine of `x`                                                                |
| `asinh(x)`          | inverse hyperbolic sine of `x`                                                 |
| `atan(x)`           | arc tangent of `x`                                                             |
| `atanh(x)`          | inverse hyperbolic tangent of `x`                                              |
| `cbrt(x)`           | cube root of `x`                                                               |
| `ceil(x)`           | smallest integral value not less than `x`                                      |
| `clamp(x,a,b)`      | constrain `x` to lie between `a` and `b`                                       |
| `close(a,b)`        | `1` if `a` is close to `b` within `1e-6` relative precision, `0` otherwise     |
| `close(a,b,p)`      | `1` if `a` is close to `b` within `p` relative precision, `0` otherwise        |
| `cos(x)`            | cosine of `x`                                                                  |
| `cosh(x)`           | hyperbolic cosine of `x`                                                       |
| `cube(x)`           | cube of `x`                                                                    |
| `degrees(x)`        | convert `x` from radians to degrees                                            |
| `eq(a,b)`           | `1` if `a = b`, `0` otherwise                                                  |
| `erf(x)`            | error function, `2/√π * integral from 0 to x of exp(-t²) dt`                   |
| `exp(x)`            | base-e exponential of `x` (`e^x`)                                              |
| `exp2(x)`           | base-2 exponential of `x` (`2^x`)                                              |
| `floor(x)`          | largest integral value not greater than `x`                                    |
| `fract(x)`          | fractional part of `x`                                                         |
| `gt(x)`             | `1` if `a > b`, `0` otherwise                                                  |
| `gte(x)`            | `1` if `a ≥ b`, `0` otherwise                                                  |
| `hypot(x,y)`        | euclidean distance (`√(x²+y²)`)                                                |
| `isfinite(x)`       | `1` if `x` is finite, `0` otherwise                                            |
| `isinf(x)`          | `1` if `x` is infinite, `0` otherwise                                          |
| `isnan(x)`          | `1` if `x` is not a number (`NaN`), `0` otherwise                              |
| `isnormal(x)`       | `1` if `x` is normal, `0` otherwise                                            |
| `linear(a,b,x)`     | linearly remap `x` in range `[a;b]` to `[0;1]`                                 |
| `linear2srgb(x)`    | convert `x` from linear to sRGB (see also `srgb2linear(x)`)                    |
| `linearstep(a,b,x)` | saturated version of `linear()` (equivalent to `sat(linear(a,b,x))`)           |
| `log(x)`            | natural logarithm of `x` (see `e`), also called "ln" in math terminology       |
| `log2(x)`           | base-2 logarithm of `x`                                                        |
| `lt(a,b)`           | `1` if `a < b`, `0` otherwise                                                  |
| `lte(a,b)`          | `1` if `a ≤ b`, `0` otherwise                                                  |
| `max(a,b)`          | maximum value between `a` and `b`                                              |
| `min(a,b)`          | minimum value between `a` and `b`                                              |
| `mix(a,b,x)`        | linearly remap `x` in range `[0;1]` to `[a;b]`                                 |
| `mla(a,b,c)`        | multiply-add in one operation (`a*b + c`)                                      |
| `pow(a,b)`          | `a` raised to the power of `b`                                                 |
| `print(x)`          | leave `x` unchanged but print its value on the standard output (debug purpose) |
| `radians(x)`        | convert `x` from degrees to radians                                            |
| `round(x)`          | round `x` to nearest integer, away from zero                                   |
| `sat(x)`            | saturate `x` (equivalent to `clamp(x,0,1)`)                                    |
| `sign(x)`           | `1` if `x` is positive, `-1` if is negative, `0` if zero                       |
| `sin(x)`            | sine of `x`                                                                    |
| `sinh(x)`           | hyperbolic sine of `x`                                                         |
| `smooth(a,b,x)`     | hermite interpolation (`f(t)=3t²-2t³`) using `t=linear(a,b,x)`                 |
| `smoothstep(a,b,x)` | same as `smooth()` but using `t=linearstep(a,b,x)` (only the S curve remains)  |
| `sqr(x)`            | square of x (`x²`)                                                             |
| `sqrt(x)`           | square root of `x`                                                             |
| `srgb2linear(x)`    | convert `x` from sRGB to linear (see also `linear2srgb(x)`)                    |
| `tan(x)`            | tangent of `x`                                                                 |
| `tanh(x)`           | hyperbolic tangent of `x`                                                      |
| `trunc(x)`          | round to integer, toward zero (see also `floor()`)                             |
