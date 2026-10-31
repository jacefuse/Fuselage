# MISL input, audio, timers and ROM services

These native adapters translate guest registers and addresses into calls owned by
CAKE, SHARP, DICE and the shared MISL runtime. The subsystems do not include host
adapter headers. Numeric opcodes and wire layouts retain the existing ABI.

## Input

Keyboard and mouse-button queries check the index before reading CAKE tables.
Mouse values are signed deltas and wheel movement, carried unchanged in a 32-bit
register. Controller metadata and vibration calls use CAKE's normal slot rules.
The adapter reads existing state; it does not poll devices or consume transitions.
Polling belongs to the orchestrator; mapped publication is supplied by the
[host device pumps](MISL_HOST_RUNTIME.md#input-pumps).

Controller-name copying returns the stored byte count excluding NUL. A nonempty
admitted output is terminated when CAKE supplies a name; capacity one stores only
NUL. Zero capacity, invalid output or a missing native name returns zero. When
the name is missing, the output remains unchanged. The complete output capacity
must be writable, even if the name would occupy fewer bytes.

## Audio

Calls use the SHARP facade for availability, sample rate, render position, chip
creation, pool writes, register events and mixer settings. The render position
uses a low/high result pair. Scheduled writes preserve the full 64-bit position.
The master-volume setter is void and does not replace the result register.

Pool uploads require a complete readable guest source span. Unmapped input returns
false without calling SHARP; native destination bounds and availability checks
still apply. A true register-write result means queue acceptance, not that the
audio has reached a device. Mixer numeric validation remains SHARP's policy.
Mapped control/ring processing and capture publication are supplied by the
[audio pumps](MISL_HOST_RUNTIME.md#audio-pumps). This dispatcher does not operate
the event ring itself.

## Dice

Random streams, dice, positional sequences and timers use the canonical DICE
operations. Seeds and sequence positions retain all 64 bits. Handles retain their
existing eight-bit narrowing; callers should pass valid handles. Timer creation
uses an interval in microseconds and supplies no native callback or user pointer.
Timer allocation preserves signed minus one as `0xFFFFFFFF` when none is free.

Entropy mixing requires the complete input span; native stream and zero-length
rules still determine acceptance. The real-time call requires a complete writable
`MislClock` destination. It fills an aligned native temporary and copies the bytes,
so an unaligned guest destination is permitted. The same field-by-field DICE
calendar conversion supplies mapped clock publication; its reserved field is zero.
Success reports a copied snapshot, not a separate calendar-query error status.
Timers still require host processing; invoking the adapter does not advance them.

## ROM

BOOTSPUR numerical entries call the shared compiler-support, math and formatter
implementations. Floating values travel as bits in integer registers, with doubles
in low/high pairs. Results retain the previously reviewed numerical behavior;
the dispatcher does not substitute host-libc math.

Memory operations resolve entire source/destination spans before writing. Zero-byte
copy, move, set and compare operations resolve no pointers. Copy/move/set return
the destination address; comparison returns exactly minus one, zero or one. The
host copy entry tolerates overlap through `memmove`. This does not change the C
contract of any locally compiled guest `memcpy` implementation.

String length reads mapped bytes until NUL. Address arithmetic cannot wrap back
to address zero. Append-string reads pieces of at most 127 bytes into native
storage before calling the shared formatter. Invalid input faults; a fault after
earlier pieces were appended can leave that prefix in the destination. It may
inspect more source bytes than fit in the remaining output. Native/guest aliasing
does not provide an immutable snapshot of the entire source string.

Append results are stored positions, limited to capacity minus one. Unlike the
direct formatter primitive, the ROM adapter faults for zero output capacity.
Invalid nonempty memory ranges and string walks halt the guest at the serviced
ECALL. Diagnostics include the entry and offending range when available. Unknown
entries also halt rather than silently succeeding.

Debug tracing copies a safe prefix of at most 511 bytes. It faults on an unmapped
byte encountered during that bounded read. Debug hosts print at most 120 lines
per one-second window and report dropped counts when the next window opens.
The limiter is process-wide. Release hosts validate the same string range and
discard the line, returning zero. Debug results indicate whether a line was printed.

## Save admission and integration

The ROM save-write entry resolves its complete source before calling a private
persistence interface. Zero bytes return zero without touching pointers or save
storage; success returns one. Refusal faults. The persistence implementation must
check the full offset/count range before mutation and consume the input during
the call. [SRAM](../storage/SRAM.md) now supplies this implementation and the load/finish
binding. The scheduler and root exit paths use it; see
[native lifecycle](../engine/FUSELAGE.md#lifecycle).

The [scheduler and device pumps](MISL_HOST_RUNTIME.md) and
[native build](../engine/FUSELAGE_BUILD.md) connect these dispatchers to the engine. Resource
reset policy still needs broader hardening. Long string walks and large native
calls need an explicit service-work budget before claiming bounded slice latency. See [the native host boundary](MISL_HOST.md) for mapping limits.
