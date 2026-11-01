# Direct MISL math

MISL's math implementation supplies the native routines behind BOOTSPUR's math
entries. A guest can also compile the C implementation locally. The source uses
explicit binary64 constants, polynomial kernels and compensated arithmetic rather
than delegating transcendental functions to a host libm.

## Direct and ROM interfaces

`misl_math.h` declares the direct C interface, with C linkage for C++ callers.
The names follow the MISL system convention:

| Direct double function | Operation |
|---|---|
| `MISL_CalculateSine` | Sine, radians |
| `MISL_CalculateCosine` | Cosine, radians |
| `MISL_CalculateTangent` | Tangent, radians |
| `MISL_CalculateArcTangent2` | atan2 of `(y, x)`, returning radians |
| `MISL_CalculateExponential` | Exponential with base e |
| `MISL_CalculateNaturalLogarithm` | Natural logarithm |
| `MISL_CalculatePower` | Base raised to exponent |
| `MISL_RoundDown` | Round toward negative infinity |
| `MISL_RoundUp` | Round toward positive infinity |
| `MISL_Truncate` | Round toward zero |
| `MISL_Round` | Nearest integer, ties away from zero |

Each has a float counterpart with `Float` appended, such as
`MISL_CalculateSineFloat`. Float operations use the double implementation and
then convert to float; the rounding functions use the same approach.

Ordinary guest calls through `<math.h>` retain standard names such as `sin`,
`sinf` and `pow`. Their ROM indices and register contracts are unchanged by the
direct-interface renames. See [BOOTSPUR](BOOTSPUR.md#math-and-floating-abis).
`MISL_MATH_API` remains a declaration/definition attribute hook.

## Source organization

| File | Contents |
|---|---|
| `misl_math_trig.c` | Reduction, trigonometric kernels and atan2 |
| `misl_math_exponential.c` | Exponential and logarithm |
| `misl_math_power.c` | Power and integer-exponent handling |
| `misl_math_round.c` | Double rounding operations |
| `misl_math_float.c` | Float entry points |
| `misl_math_internal.h` | Private constants, representation and compensated helpers |

Compile all five C files. The private header is required source material but
does not define an application interface. Implementations compile as C; C++
applications can use the public declarations and link those objects.

## Build and numerical scope

Use `-ffp-contract=off`, no fast-math, and `FLT_EVAL_METHOD == 0`. Guest builds
also use `-fno-math-errno` for the hardware square-root primitive. The source
checks its binary64 representation assumptions. Callers must preserve the expected
round-to-nearest environment and subnormal handling; this library does not set
or restore the floating environment.

The tested direct configurations are native Windows Clang, native macOS Apple
Clang on arm64 and locally compiled RV32IMFD/ILP32D. A different guest ABI needs
matching compiler-support routines; selecting an instruction set alone does not
provide them. The ROM interface has
its own ABI-specific declarations and thunks.

The ordinary-range implementation is intended to give consistent results when
those prerequisites agree. Shared source alone does not establish identical
results on every compiler, host or floating environment. The sampled accuracy
checks compare against the installed host libm; they are not a global proof of
correct rounding or a certified maximum error.

Trigonometric range reduction becomes less accurate as argument magnitude grows.
If the computed quadrant cannot fit its signed 64-bit representation, the
trigonometric functions return a canonical quiet NaN rather than performing an
out-of-range integer cast. There is no full-binary64-range accuracy guarantee.

Power uses compensated integer powers for integral exponents of magnitude up to
1024 and exponential/logarithm composition otherwise. The integer path keeps its
compensated values within a fixed binary range and carries the excess as a
binary exponent, so results beyond the representable range become infinity or
zero, results in the subnormal range are scaled with one rounding, and results
in the normal range are unchanged from the unscaled arithmetic. The composition
path decides overflow and underflow from the logarithm times the exponent before
forming its compensated product. This implementation is still not a complete
libm replacement for extreme inputs: the composition path's accuracy near its
own range limits and for subnormal bases is that of the logarithm and
exponential it is built from.

## Special values

Transcendental functions handle selected domain, zero, infinity and NaN cases
explicitly. Sine/cosine/tangent of infinity return quiet NaN. Logarithm of zero
returns negative infinity; negative nonzero inputs return NaN. A negative power
base requires an integral exponent.

Power's zero-exponent and unit-base cases return one even with the other argument
NaN, including in the float interface. Input NaNs normally produce canonical
NaN in the transcendental paths; this is not a blanket promise about every
intermediate overflow or NaN payload. Double rounding operations can retain the
input's NaN representation. Signed zeros are retained where the operation calls
for them.

These routines do not provide a hosted `errno` or general libm exception policy.
Compiler-support conversion and division policies are documented separately in
[MISL helpers](MISL_HELPERS.md).
