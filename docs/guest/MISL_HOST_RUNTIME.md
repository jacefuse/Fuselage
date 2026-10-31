# MISL host execution and device pumps

The native host owns execution slices, device snapshots and halt presentation.
VIVIAN remains independent. Initialize clocks and native subsystems first, and
serialize these operations on the host tick thread.

## Tick order

`FuselageRunGuest` supplies the combined guest tick:

1. For a new descriptor, discard the previous guest's unsaved working state,
   reset audio pages/capture bookkeeping, prepare memory, open the save and commit
   mappings. Install the graphics store watch before the first instruction.
2. Publish input, calendar and audio snapshots, then refresh Pixie and tile banks.
3. Run one slice, or decrement a pending sleep without executing instructions.
4. Attempt dirty graphics uploads and drain audio events, including after a slice
   halts or faults. Runtime faults force save abandonment independently of the UI.

The same stopped image is not re-entered. A new descriptor selects a replacement;
mutating an existing descriptor is not a reload request. Keep its bytes alive and
unchanged. Replacement discards the old working save; request a clean finish
separately when publication is intended. A failed load does not execute the image.

Do not repeat the old separate pumps/drain around this call.
`FuselageHandleGuestHalt` is an optional subsequent presentation operation.
Engine-wide CAKE polling, DICE timer processing, render preparation and window
events remain Fuselage's responsibility; the guest tick does not duplicate them.

## Slice pacing

The budget reserves the larger of 1.5 ms or one fifth of the simulation period
for host work. Its minimum is 1 ms and ceiling 250 ms. Invalid rates select 60 Hz;
tiny positive rates saturate without an overflowing float-to-integer conversion.

An estimate of execution time between voluntary yields can widen that budget.
It widens immediately, narrows by one eighth and starts at the ceiling. Budget
cuts retain accumulated work until a yield, unless the run exceeds the ceiling.
Every cut also eases the estimate down. Accumulation is bounded for long intervals.
These are guest execution measurements, not presented-frame counts. A yield does
not prove a frame was drawn; unchanged persistent graphics can still be presented.

Elapsed time is checked after accumulated chunks, including execution interrupted
by ECALLs. A pending ECALL at the deadline is serviced once before leaving the
slice. Yield/sleep completes it; faults retain their failing PC. Sleep counts
later host ticks, not milliseconds.

This is not a hard real-time deadline. Chunk execution and native service calls
can overrun the budget. An advancing DICE clock is required; no independent
instruction backstop currently handles a stalled clock. VIVIAN attempts to restore
host floating controls before each executed slice. `FuselageGetGuestStepCount`
includes faulting attempts, as VIVIAN's counter does, rather than reporting strict
architectural retirement.

## Input pumps

The page is a fresh snapshot of existing CAKE state. Held/pressed/released values
are normalized independently, so both edges can be visible between pumps.
Disconnected pads are zeroed while release edges remain visible; unused entries
are zero. Publication does not poll devices or consume their transitions.

Canvas coordinates come directly from GDMF, including valid `(0, 0)`. Conversion
truncates toward zero with signed 32-bit saturation; NaN becomes zero. The current
query also returns zero on backend failure, so that case cannot be distinguished
from top-left. The host no longer substitutes a stale nonzero position. Relative
mouse deltas and wheel movement retain their separate CAKE meanings.

## Audio pumps

Before execution, the host publishes availability, sample rate, render position
and capture metadata. Requests for one bus retain the largest requested history,
limited by SHARP's maximum. Successfully applied requests are cached to avoid
clearing history every tick; refused changes retry. Capture slots and the MMIO
control/ring/output pages reset on load.

Capture data is packed in request order. One channel selects mono; other values
select stereo. F32 selects floats; other format values select S16. Starts are
four-byte aligned. Frame counts are bounded by native capacity and remaining page
space. Returned counts are also bounded before advancing the cursor; native
callees must still honor their supplied output capacity.

After execution, unsigned index subtraction handles ring wrap. An overrun retains
only the newest capacity events and adds overwritten entries to `dropped`.
Counters retain unsigned wraparound. Native refusal does not replay consumed ring
entries; SHARP retains its own queue diagnostics. This is a sequential tick-thread
protocol, not a concurrent queue.

Native chips, timers, random streams and logical drawing resources are not all
reset by these page operations. Their ownership/cleanup remains FUNDUS work;
clearing MMIO is not subsystem shutdown.

## Halt presentation

`FuselageSetGuestHaltAction` selects escape presentation, ignore, or reserved
monitor behavior. Monitor currently falls back to escape presentation. Ignore
suppresses the panel, not fault-save abandonment.

The handler redraws a finished/fault panel each call. Faults include captured PC,
instruction, SP and return address; normal finishes omit that line. Held Escape
requests host quit. Panel rows are cleared before reuse, and displayed reasons
are shortened to one row while the halt record retains the full stored reason.
Save-load refusal uses `FuselageTickSaveFailure(reason)`, a
Fuselage-owned view independent of SRAM, positioned on the actual 80-by-45 text
grid. These views do not present a frame or run a nested event loop.

Native launcher and embedded consumers now use this combined tick; see
[native launch modes](MISL_LAUNCHER.md). Root shutdown, native entry points and
build-mode wiring are integrated; see [host builds](../engine/FUSELAGE_BUILD.md). Real
application/device validation remains outstanding.
