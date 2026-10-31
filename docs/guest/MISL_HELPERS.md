# MISL compiler-support implementation

`misl_helpers.h` collects the C implementations used by host ROM services for
64-bit shifts, division and floating conversions. They may also be compiled into
a guest for internal comparison or a deliberately chosen runtime implementation.
They do not require another engine system.

## Interfaces

Implementation functions use private `misl_helpers_` names. The headers are split
into word/shift, division and conversion concerns. They are required source
inputs, not an application API. The compiler-emitted symbols such as `__divdi3`
and `__fixdfdi` remain the unchanged assembly thunks in `misl_rt.S`; renaming a
private helper does not change their spelling or ROM index.

The code is C and assumes the supported little-endian word representation.
Floating conversion uses binary64 arithmetic and must be built with compatible
floating support, contraction disabled and no fast-math. Shared code is not by
itself a guarantee for arbitrary hosts or rounding modes.

## Integer representation

The word helpers view a 64-bit value as two 32-bit words to avoid implementing
a variable-width shift in terms of the same compiler helper it would need.
The header rejects a compiler-reported big-endian target.

Nonpositive shift counts return the input unchanged. Counts at least 64 return
zero for logical shifts and sign fill for arithmetic right shift. These are
helper policies; they do not make an invalid C shift expression well-defined.

## Division and remainder

Unsigned division uses a bitwise quotient/remainder algorithm. Signed division
handles magnitudes without negating the most-negative signed value. Quotients
truncate toward zero and a nonzero remainder follows the numerator's sign.

The retained special policies are:

- A zero denominator returns zero for both quotient and remainder.
- Signed minimum divided by minus one returns signed minimum, with zero remainder.

The zero-denominator result differs from VIVIAN's base RISC-V DIV instruction.
These are ROM/compiler-helper results, not a claim that division by zero or signed
overflow is defined in application C.

## Floating conversions

Integer-to-floating helpers assemble the high and low words. Unsigned-to-float
uses a sticky bit when reducing a large significand, avoiding the ordinary
double-rounding problem in the tested round-to-nearest path.

Floating-to-integer helper results are explicit:

| Destination | Policy |
|---|---|
| Unsigned 64-bit | NaN and nonpositive inputs become zero; values at/above 2^64 become maximum; other values truncate toward zero |
| Signed 64-bit | NaN becomes zero; values at/below -2^63 become minimum; values at/above 2^63 become maximum; other values truncate toward zero |

Float inputs are widened before the same conversion logic. These rules describe
calls to the helpers. They do not extend the C language's permitted range for
ordinary floating-to-integer cast expressions, which a compiler may optimize
independently.
