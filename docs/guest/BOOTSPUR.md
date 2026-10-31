# BOOTSPUR guest services

BOOTSPUR is the system ROM through which a MISL guest reaches memory, text,
math and save services. The public declarations use standard C names where
compilers or shared application source expect them. See
[Guest runtime](MISL_RUNTIME.md#rom-thunks) for the assembly tail jumps and
[ROAP format](ROAP_FORMAT.md#bootspur-system-rom) for the ROM table contract.

## Headers and scope

`bootspur.h` collects four interfaces:

| Header | Contents |
|---|---|
| `misl_bootspur_memory.h` | Memory copy/fill/move, comparison and string length |
| `misl_bootspur_math.h` | Integer magnitude, ABI-specific math and inline ISA operations |
| `misl_bootspur_services.h` | Diagnostic trace and SRAM write declarations |
| `misl_format.h` | Bounded append primitives and formatting declarations |

Every external declaration has C linkage, including when a C++ source includes
it. The SRAM declaration is shared with the guest facade rather than repeated
there. These headers provide a narrow guest interface, not a complete hosted C
or C++ standard library. They require no native engine headers.

## Freestanding include path

The adjacent `include` directory provides guest `<math.h>`, `<string.h>`,
`<stdlib.h>` and `<limits.h>` headers. Use this directory before toolchain
library headers, and also supply the parent MISL directory for `bootspur.h`.
These are guest include paths; do not add them to native engine compilation.

The math, string and stdlib shims continue including the BOOTSPUR umbrella,
preserving their existing declaration surface. The limits header uses target
compiler macros, including the compiler-specific spelling for C23 long-long
width. `MB_LEN_MAX` is one under the guest's single-byte character model.

For a hermetic build, keep compiler-provided freestanding headers such as
`stddef.h`, `stdint.h` and `stdarg.h` available even when hosted library include
paths are excluded. Compiler resource directories differ; ask the selected
compiler for its own include location rather than embedding a machine path.

No allocator, filesystem, process-exit service or general libc is declared here.
For example, these headers do not supply `malloc`, `strcmp` or `hypot`.
Compiler flags and the completed link still determine which support routines
an application requires.

## Memory and strings

`memcpy`, `memset`, `memmove`, `memcmp` and `strlen` use their standard signatures.
They are ROM thunks in a guest. Byte counts use the guest's `size_t`; pointers
are guest addresses. `memcpy` requires non-overlapping ranges; use `memmove`
when ranges may overlap. Supply valid accessible ranges and a terminated string
to `strlen`. The declarations do not independently validate memory.

## Math and floating ABIs

`abs`, `labs` and `llabs` are inline builtins. Their result must fit the signed
return type; the most-negative signed input has no supported positive result.
There is no guaranteed saturation or unchanged-value result for that input.

Single-precision declarations are available under ILP32F and ILP32D;
double-precision declarations require ILP32D. The ROM-served set covers sine,
cosine, tangent, atan2, exponential, logarithm, power, floor, ceiling, truncation
and rounding. Soft-float ILP32 does not expose those floating services through
these headers; an application must supply compatible support if it needs them.

Square root and fused multiply-add execute explicit RISC-V instructions, avoiding
compiler fallback calls to functions that the ROM does not supply. The inline
assembly is volatile because these operations can affect floating status flags.
Floating absolute value uses the compiler builtin. No `errno` facility is
provided. These are the guest ISA's floating operations, not a promise of full
hosted-libm error handling or every library's edge-case behavior.

The math shim also publishes the existing `M_*` constants, infinity/NaN values
and `isnan`, `isinf` and `isfinite` builtins. It does not introduce additional
ROM entries. Applications that mix ABI modes need explicit adapters; matching
symbol names do not translate integer-register and floating-register conventions.

## Diagnostics and saves

`MISL_Trace` returns whether a host printed the line. Debug output is rate-limited;
a release host can discard it. Supply a valid guest string: an invalid pointer
can still trigger the host's bad-pointer halt path, even when output is disabled.
Trace is not a guaranteed non-failing diagnostic channel.

`MISL_WriteSram` returns true after a successful nonempty write and false for a
zero-byte request. Invalid nonempty source or destination ranges halt the guest.
Save reads use the facade's `MISL_ReadSram`. Neither declaration determines where
the host stores save files or when it commits them.

## Formatting interface

`MISL_AppendStr`, `MISL_AppendInt` and `MISL_AppendFixed` append into a buffer and
return the new stored position. Capacity includes the terminating NUL. A valid
nonempty buffer is terminated; a null buffer or zero capacity cannot receive
one. The fixed-point primitive inserts a decimal point into an integer value,
with decimal places capped at nine.

`MISL_Format` and `MISL_FormatV` return the number of characters stored, excluding
NUL. This differs from `snprintf`'s required-length return convention. Variadic
argument traversal happens in the caller's environment; a `va_list` is not sent
through an ecall. The append primitives are ROM entries, while the formatter
front end requires its separate compiled implementation.

See [Bounded MISL formatting](MISL_FORMAT.md) for the implementation files,
supported conversions, precision policy and unsupported-conversion markers.
Do not infer full printf compatibility from the function names.
