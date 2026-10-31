# DICE

DICE provides elapsed time, calendar time, handle-based timers and random streams.
It has no dependency on Fuselage or another engine system. Public functions use
the DICE_ prefix; implementation helpers stay in private headers.

## Lifecycle

Serialize initialization and shutdown with other DICE work. Repeated DICE_Init
returns true without resetting the elapsed-time epoch. The current implementation
returns true after starting the platform clock; it does not propagate platform
clock failures.

Shutdown unlocks and releases timers before releasing RNG streams, clearing die
readiness and ending elapsed-time reporting. It also cleans components created
before DICE_Init. Configured sequence seeds survive shutdown. Repeated successful
shutdown does nothing.

Run lifecycle operations on a management thread after other callers stop. Timer
callbacks cannot initialize or release timers or shut down DICE, including other
timer handles. DICE_Shutdown has no error return: it reports failure to stderr
and retains remaining timer owners and RNG/clock lifetime for a later retry.
The private dice_shutdown_checked interface lets the orchestrator check this
result. Cleanup can be partial; it is not a transaction.

## Elapsed time

DICE_GetElapsedSeconds and DICE_GetElapsedMicroseconds measure monotonic time since
initialization. Both return zero before initialization and after shutdown.
Seconds are derived from whole microseconds, without additional sub-microsecond
precision.

Windows uses QueryPerformanceCounter, macOS uses mach_absolute_time and Linux
uses CLOCK_MONOTONIC. These measure duration rather than calendar time.
No elapsed reading is consumed. Each caller stores its own previous timestamp
to calculate a delta. DICE_GetTimerDelta instead maintains a previous reading for
an existing timer handle.

## Calendar time

DICE_GetRealTime reads calendar time independently of DICE_Init. A NULL output is
ignored. The void API has no failure status; this does not guarantee that platform
clock or timezone operations cannot fail.

| DICE_DateTime fields | Meaning |
| --- | --- |
| utcSeconds | Unsigned Unix epoch seconds, starting at 1970-01-01 UTC |
| milliseconds | Fractional part of that second, 0..999 |
| utcOffsetMinutes | Local time minus UTC, including the effective DST shift |
| isDst | One when daylight saving time is reported locally, otherwise zero |
| year, month, day | Local civil date; month and day start at one |
| weekday | Sunday is zero, Saturday is six |
| hour, minute, second | Local time of day |
| yearDay | Local day of year, starting at one |
| _pad | Reserved padding field, written as zero |

Store UTC values for timestamps and use local fields for display. UTC and local
readings are currently separate operations. The offset calculation rounds their
whole-second difference to the nearest minute. This absorbs a small read gap,
but does not make the reads one atomic snapshot.

Civil-date arithmetic follows Howard Hinnant's calendar algorithm. The record
is 56 bytes in the checked Windows ABI. Guest mirrors must retain field order,
widths and padding.

## Companion interfaces

dice.h includes dice_timers.h, dice_rng.h and dice_help.h. Timers expose handle
allocation/ownership flags, start/stop/pause/resume, interval consumption and
optional worker-thread callbacks. A handle ownership flag is distinct from a
thread synchronization primitive. Callers synchronize callbacks with application
state.

RNG streams support deterministic, entropy-seeded, continuous-mixing and explicit
accumulation modes. Their entropy guarantees must be assessed separately from
clock behavior. Dice helpers consume streams; positional sequences use seeds
and positions without consuming a stream. Internal reset declarations are not
part of the public interface.


## Random streams

RNG handles are 0..31. Each owns PCG32 state and a stream selector derived from
its handle. A deterministic replay therefore needs both the handle and caller
seed. Seeding, raw draws, explicit entropy mixing and float conversion share
one implementation across platforms. The original golden vectors remain intact.
The algorithm attribution is Melissa O'Neill's PCG32.

| Mode | Initialization and later draws |
| --- | --- |
| DICE_RNG_DETERMINISTIC | Seed directly from the caller; no automatic entropy |
| DICE_RNG_ENTROPY_SEEDED | XOR caller seed with one entropy reading; then draw normally |
| DICE_RNG_CONTINUOUS | Same initial seeding; mix fresh entropy before each raw draw |
| DICE_RNG_ACCUMULATE | Seed directly from the caller; only explicit mixing adds entropy |

InitRNG replaces a valid handle's existing stream. An invalid handle or mode
returns false without changing it. ReleaseRNG returns false for an invalid or
uninitialized stream. GetRNGSeed returns the original caller seed, including in
entropy modes; it does not recover the OS entropy used for initialization.
An invalid stream returns seed zero.

MixRNGEntropy hashes the supplied bytes into the existing state and advances it
once. It returns false without changing state for an invalid stream, NULL data
or zero bytes. Explicit mixing works in every mode and affects deterministic
replays, so those inputs must also be reproduced.

RandUint returns a full-width unsigned draw. RandInt includes both bounds and
exchanges reversed bounds for a valid stream. Rejection sampling avoids modulo
bias and may consume several raw draws. Integer range calculations require a
32-bit int. The full signed-range case preserves the existing raw-bit mapping:
unsigned values above INT_MAX become their negative signed counterparts.
RandFloat uses 24 random bits and returns a value in [0,1).

For an invalid stream, RandUint/RandFloat return zero and RandInt returns the
supplied minimum without exchanging bounds. These values can also be legitimate
results, so they are not unique error codes.

RandChance returns false for probabilities at or below zero and true at or above
one, without consuming a draw or checking the handle. Otherwise it compares a
float draw to the probability. A NaN probability returns false and consumes a
draw on a valid stream. Callers should still supply a valid handle; an invalid
handle with an ordinary positive probability can return true because its float
fallback is zero. These existing edge semantics are preserved.

Serialize access to each stream, including initialization, release and mixing,
and coordinate system shutdown with all users. RNG operations do not add their
own locks.

### Entropy and fallback

Windows uses BCryptGenRandom and needs the native bcrypt library. macOS/Linux
use getentropy. OS failure falls back to platform counter/time values; if the
fallback clock also fails, its initialized zero value is used. These failures
are not reported through InitRNG. The fallback can be predictable or repeat, so
these gameplay streams provide no cryptographic unpredictability guarantee.
The Windows implementation and controlled failure paths have been exercised;
native macOS/Linux entropy execution remains to be verified.


## Dice helpers

A die is a convenience wrapper over the RNG stream with the same handle: die N
and RNG N share state. DICE_SeedDie selects deterministic mode with the supplied
seed. DICE_RollDieRange, DICE_RollDieFloat and DICE_RollDieChance use the RNG range,
float and probability rules described above.

On the first roll without SeedDie, the helper initializes an entropy-seeded RNG
with caller seed zero. A directly initialized RNG at that handle is replaced if
the die helper has not been seeded yet. Once the helper is ready, direct RNG
calls and die rolls consume the same stream. Reinitializing that live stream
changes subsequent die rolls too. After direct RNG release, the next die roll
auto-seeds again; call SeedDie again when deterministic replay is required.

SeedDie returns false for an out-of-range handle. Invalid die rolls preserve
RNG fallback behavior: range returns the supplied minimum, float returns zero,
and chance follows the probability shortcuts and zero-float comparison. These
fallback results are not distinct error codes. A first chance call can initialize
the die even when its probability means no draw is needed.

System shutdown clears the helper's seeded flags after RNG release. Synchronize
both APIs together when they share a handle; the readiness check is not a lock.

## Positional sequences

Sequence handles are 0..31. DICE_SeedSequence sets a handle's seed; until then its
seed is its handle number. Sequences do not allocate or consume RNG streams and
need no DICE_Init. Their configured seeds currently survive shutdown and
reinitialization. Explicitly set seeds when selecting a new world or replay.

DICE_GetSequenceUint returns the same value for the same seed and 64-bit position,
regardless of earlier queries. Positions can be visited repeatedly or in any
order. DICE_GetSequenceFloat maps 24 bits to [0,1); DICE_GetSequenceChance compares
that value against a probability without advancing anything. Hashing retains the
existing SplitMix64 mixing steps attributed to Steele/Vigna.

DICE_GetSequenceInt includes both bounds and exchanges reversed bounds. Its
multiply-and-scale mapping uses one position query. It is not exactly uniform
unless the interval width divides 2^32; this mapping is preserved for existing
content. The full signed range retains the original raw-bit-to-signed mapping.
Wide additions use int64 arithmetic and the implementation requires 32-bit int.

SeedSequence returns false for an invalid handle. Invalid getters return unsigned
zero, float zero or the supplied integer minimum. Chance preserves existing RNG-
style shortcuts: probabilities at or above one return true, and an ordinary
positive probability can also return true for an invalid handle because the float
fallback is zero. NaN returns false. Supply valid handles even when a fallback
looks usable. Serialize seed changes with queries; a stable seed makes queries
independent of order, not synchronized with concurrent writes.

## Timers and callbacks

Timer handles are 0..31. AllocTimer finds an unused, unlocked handle but does not
reserve it. LockTimer is an advisory ownership flag that prevents initialization,
release and allocation; it does not prevent start/stop, polling or private retuning.
Serialize allocation, ownership changes, initialization, release and shutdown with
all other handle users. Per-timer locks do not make concurrent release and lookup
safe.

InitTimer takes a frequency in Hz; InitTimerInterval takes microseconds. Zero,
non-finite frequencies and intervals outside the platform's representable range
return false. A period must occupy at least one native clock unit. A valid
replacement releases the previous timer before creating its worker. Failed
construction or cleanup can retain a terminal slot; retry ReleaseTimer from the
management thread. Windows uses a waitable timer, a wake event and a worker;
Linux/macOS use a condition variable and a worker.

Start begins a fresh period. Pause preserves the remaining duration; Resume
continues it. Stop clears pending intervals. These calls do not join an active
callback: its user data must remain valid until successful ReleaseTimer, which
waits for the worker to finish. ReleaseTimer returns false for invalid,
uninitialized or locked handles, callback callers, and failed platform cleanup.
A cleanup failure retains remaining owners for retry rather than discarding them.

Callbacks run on their timer worker without the timer state lock. They may
control their timer and consume intervals, but must not perform blocking lifetime
operations on DICE timers. A slow callback does not cause one callback for every
missed interval. Missed intervals contribute to the pending count; scheduling can
allow an immediate follow-up callback. ConsumeTimer returns and clears the pending
32-bit count, which can wrap. GetTimerDelta measures time between its own reads,
not callback spacing; its first reading returns zero.

A failed worker clock, wait or timer-arm operation makes the slot terminal.
IsTimerRunning then returns false, further starts fail, and management cleanup is
still required. The public API does not expose a separate worker error code.
Private retuning changes period/phase without recreating a worker, ignores the
ownership flag and rejects unrepresentable arithmetic. A stopped timer ignores
the phase shift; a paused timer clamps a negative remaining duration to zero.

## Building DICE

The Makefile in fuselage/source/DICE builds libdice.a without another engine
system. It requires GNU Make and a C11 compiler with GCC/Clang command-line
options, plus an archiver. The default compiler is Clang. On Windows, use a
MinGW-compatible toolchain and mingw32-make; the recipes use cmd.exe.

From the repository root:

```sh
make -C fuselage/source/DICE
make -C fuselage/source/DICE print-libs
```

The default output directory is fuselage/build/DICE followed by the platform
name. CC, AR, CPPFLAGS, CFLAGS and BUILD_DIR can be supplied to make. For example,
from the DICE directory:

```sh
make CC=clang AR=llvm-ar BUILD_DIR=out CFLAGS="-O2 -g"
```

The compiler target selects windows, linux or macos. PLATFORM can select one
explicitly when using a cross compiler; it must match that compiler and its SDK.
Changing compiler or flags requires a clean rebuild. The clean target removes
only the listed objects, dependency files and archive in the selected build
directory. It does not remove the directory or unrelated files.

Include dice.h, or a companion header, using the DICE source directory as an
include path. Link the application with libdice.a followed by the options from
print-libs: -lbcrypt on Windows, or -pthread -lm on Linux/macOS. These options
belong on the final application link; an archive does not link its dependencies.
The Makefile also supplies -pthread while compiling POSIX workers. Windows Clang
builds use -fno-emulated-tls for the callback-context flag, avoiding a winpthreads
dependency from compiler-emulated thread-local storage.

Both platform worker files belong in a source distribution. Each build selects
one of them alongside the eight common implementation files. Private headers
are needed to build DICE; applications use the four public headers. Tests,
comparison tools, tracking notes and generated output are not library inputs.
