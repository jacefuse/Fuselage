# Bounded MISL formatting

MISL supplies append primitives and a small printf-style formatter shared by
native code and ROAP guests. It reports what it stored, rather than the length
that would have been produced in an unlimited buffer.

## Source and build

| File | Responsibility |
|---|---|
| `misl_format.h` | Public declarations |
| `misl_format.c` | Variadic parsing, output layout and diagnostics |
| `misl_format_numbers.c` | Integer digits and floating conversion |
| `misl_format_append.c` | Native append primitives |
| `misl_format_internal.h` | Private conversion declarations and configuration |

Native builds compile all three C files. Guests compile the parser and number
conversion files; append primitives are supplied by the ROM thunks in
`misl_rt.S`. Compiling the append file for a guest does not define those functions.
The variadic front end runs where it is called; a `va_list` never crosses an
ecall. Guest sources use the reviewed freestanding include paths.

Compile the conversion code with `-ffp-contract=off` and without fast-math.
The floating algorithm depends on separate multiply/subtract operations and the
supported tests use round-to-nearest. The formatter does not set the caller's
rounding mode. Its representation helpers assume the supported eight-byte IEEE
binary64 `double` and 64-bit integer types.

## Append primitives

`MISL_AppendStr`, `MISL_AppendInt` and `MISL_AppendFixed` append at a supplied
position in a buffer whose capacity includes the terminating NUL. They return
the resulting stored position. For a valid nonempty buffer, a position past the
end is clamped to `capacity - 1`; the final position is NUL-terminated.
A null native buffer or zero capacity returns zero without writing.

`MISL_AppendFixed` places the decimal point into an integer value, with at most
nine decimal places. For example, value `314159` with five places prints
`3.14159`; value `5` with two places prints `0.05`. This is integer fixed-point
printing, not floating conversion.

Supply valid input strings and storage for the stated capacity. Buffer/source
overlap is not a supported contract. Guest primitive calls also follow the
host's pointer-validation policy; native null handling does not promise that an
invalid guest address will be accepted. See [BOOTSPUR](BOOTSPUR.md).

## Format strings and results

`MISL_Format(buffer, capacity, format, ...)` and its `va_list` form,
`MISL_FormatV`, support:

| Item | Accepted forms |
|---|---|
| Integers | `%d`, `%i`, `%u`, `%x`, `%X`, `%o` |
| Other values | `%c`, `%s`, `%p`, `%%` |
| Floating values when enabled | `%f`, `%F`, `%e`, `%E`, `%g`, `%G` |
| Flags | `-`, `0`, `+`, space, `#` |
| Width | Decimal digits or `*` |
| Precision | `.digits` or `.*` |
| Length | `hh`, `h`, `l`, `ll`, `z` |

Use the argument types appropriate to each conversion and length. Negative
dynamic width selects left justification; negative dynamic precision is ignored.
Oversized literal width and precision saturate to `UINT_MAX` and `INT_MAX`.
Padding writes stop when output storage is full. That bound does not limit the
length of the format string or the number of conversions it contains.

The return is the number of characters stored, excluding NUL. A null buffer or
zero capacity returns zero without formatting. Capacity one stores just NUL.
Embedded NUL from `%c` still counts as a stored character, even though ordinary
C string functions will stop there. A null format is printed as `(null)`;
a null `%s` argument uses the same text.

String precision limits how many input characters are read: `%.3s` can consume
three characters without requiring a terminator within those three characters.
Without precision, supply a terminated string. Output truncation does not mean
the formatter stops parsing subsequent conversions.

`%p` uses a lowercase `0x` prefix without a platform-specific fixed digit width;
a null pointer prints `0x0`. Integer and string formatting generally follows the
listed printf conventions, but this is not a complete `snprintf` implementation.

## Floating conversions

Native conversion is enabled by default. Guest conversion is enabled when the
selected RISC-V ISA has double-precision hardware (`__riscv_flen >= 64`). The
normal reviewed guest configuration uses ILP32D. Selecting a D-capable ISA with a
soft-float ABI does not supply the missing ABI-specific compiler/runtime helpers;
that combination needs its own link validation.

Floating precision defaults to six and is capped at seventeen. `%g` treats zero
precision as one significant digit, selects exponent notation below exponent
`-4` or at/above its significant-digit count, and removes trailing fractional
zeros unless `#` is present.

The decimal conversion has its own rounding and large-number policy. It is not
a correctly rounded general-purpose decimal library. `%f` prints an approximate
large integer part beyond the directly handled range and uses scientific form
at sufficiently large magnitudes. `%e` and `%g` scale before rounding, so final
digits can differ from a host libc. NaN payload signs are ignored; explicit `+`
or space flags still apply. Signed zero is preserved.

When floating conversion is disabled, its double argument is consumed and a
marker such as `%!f` is emitted. Subsequent arguments remain aligned.

## Unsupported conversions

`%n` never writes through its argument. It consumes the pointer of the declared
length and emits `%!n`. Hexadecimal floating `%a`/`%A` consumes a double and emits
`%!a`/`%!A`. Formatting continues after these known, unsupported conversions.

An unknown conversion stops formatting because its argument type cannot be
inferred. It emits `%!` followed by the unknown character. A trailing `%` emits
just `%!`. The stored output is terminated before returning.

Unsupported conversions also produce a diagnostic: native debug builds report
to stderr, while guests use `MISL_Trace` and its host policy. The diagnostic is
separate from the returned formatted text. Release-native builds suppress it.
