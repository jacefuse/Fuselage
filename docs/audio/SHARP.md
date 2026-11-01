# SHARP

## Overview

SHARP provides virtual sound chips, timestamped register writes, a stereo bus
mixer and native audio output. It operates independently of GDMF, CAKE, DICE and
the guest execution system. Fuselage coordinates it when used in the full engine.

sharp.h provides the shared-pool, all-chip interface. Components also support
independent composition: an application can own chip instances and use their
render callbacks without the façade or a device backend. The sections below document the chip, event, mixer and backend interfaces.
Music-format replay and content loading belong to the application.

The built chip kinds are Adora (sample playback), Sweeny (subtractive synthesis),
Amy (square/noise/envelope synthesis) and Jenny (FM synthesis). They are inspired
by hardware families; their presence does not promise cycle-accurate emulation.

## Lifecycle

SHARP_Init allocates the sample pool and event queue, initializes the mixer and
opens/starts the device backend. It returns true only when audio starts. Failure
does not terminate the application. SHARP_IsAvailable reports the façade's started
state; it is not continuous device-health monitoring or a device-recovery service.

Repeated init returns the existing availability state. A failed initialization
still counts as initialized, so shut down before attempting initialization again.
SHARP_Shutdown stops/closes the backend before releasing the queue and sample
pool. It returns no cleanup status; do not infer successful native thread cleanup
solely from the function returning. Coordinate lifecycle with all users.

SHARP_SetPoolMB takes a MiB count before initialization and is ignored afterward.
Nonpositive values select the default 16 MiB; positive values below four select
four MiB. Requests above 4095 MiB fail initialization before allocation because
the pool byte count is uint32. Smaller requests can still fail allocation. SHARP_GetSampleRate returns
the opened backend's frame rate, or zero without a backend; do not assume 48000 Hz.

Unavailable audio does not make every call a no-op. Chip creation and register
submission require available audio, while an allocated pool and mixer/capture
controls may still be accessible. Per-operation results below describe the current
implementation. A silent initialization may retain allocations until shutdown.

## Timing and writes

One frame contains left and right samples. SHARP_GetRenderPosition returns the
sequencer's rendered-frame position, or zero when unavailable. Rendering runs ahead
into the audio buffer, so this is not the number of frames already heard. Backend
submission counters are a separate quantity. Clock publication across threads
still requires review; a 64-bit return type alone provides no synchronization.

SHARP_WriteAt submits a 16-bit register/value pair at an absolute render-frame
position. SHARP_Write uses the current render position plus 64 frames. These units
and scheduling behavior are unchanged by the getter rename. The 64-frame lead
is a scheduling allowance, not a guarantee against late delivery. All register submissions
must use one producer thread; the event queue is single-producer/single-consumer.

Both write functions return false when audio is unavailable, the chip ID is invalid
or the event queue is full. Successful submission does not certify that a chip
recognizes that register or that sound reached the device. Register maps belong to
the individual chip headers.

Events retain submission order. A timestamp behind the preceding accepted timestamp
is clamped forward. SHARP_GetDisorderedEventCount counts those adjustments;
SHARP_GetLateEventCount counts events dispatched after their effective timestamp;
SHARP_GetDroppedEventCount counts submissions rejected by a full queue. They return
zero when unavailable. A disordered event can also be late if its clamped timestamp
is already behind the renderer. The counters are uint32 and can wrap.

## Sample pool

SHARP_GetPoolSize returns allocated bytes, or zero without an allocated pool.
SHARP_WritePool copies bytes from caller-owned data at a byte offset. It returns
false without a pool, with a null source, or when the requested range exceeds the
pool. A zero-byte write with a non-null source and valid offset succeeds. It does
not open a file or change the pool size.

The pool is shared data used by sample-playing chips. The caller's source need
only remain valid for the copy, but the pool itself must outlive its chip users.
Writing data while rendering reads that region requires coordination; this call
does not provide a synchronized live-sample update protocol.

## Chips

SHARP_CreateChip returns a zero-based ID, or -1 if unavailable, full, or unable to
add the chip to the sequencer. The current table supports sixteen chips. An
unrecognized kind currently selects Adora; it is not an invalid-kind error.
Creation routes the chip to bus zero and enables that bus. Chip IDs last until
shutdown; this interface has no individual chip-removal operation.

SHARP_GetChipCount returns the current count, including zero before initialization
or after shutdown. SHARP_GetChipName returns a borrowed static label, "(none)" for
an invalid ID, or "(unknown)" for an unrecognized stored kind. Do not free the label.
Creating/routing chips while rendering is active needs the same ownership review
as other control changes; no general concurrent-mutation guarantee is made here.

## Mixer

Bus indices are zero through SHARP_BUSES-1. Each bus carries stereo audio and has
enable, volume and balance controls. Balance attenuates the opposite channel; it
does not fold stereo into mono. LORI combines buses and applies master gain and
soft clipping.

SHARP_SetChipBus returns false for unavailable audio or an invalid chip/bus. Success
also enables the destination bus. SHARP_GetChipBus returns its index, or -1 for an
invalid chip or unavailable audio. SHARP_EnableBus, SHARP_SetBusVolume and
SHARP_SetBusBalance return false for an invalid bus and true otherwise, including
when audio is unavailable.

For finite inputs, negative bus volume clamps to zero; positive volume is not
limited to one. Balance clamps to -1..1. SHARP_SetMasterVolume stores the supplied
gain and returns no status. Supply finite values; non-finite numeric handling and
concurrent mixer updates remain under review. These are control values, not
register writes scheduled by the event queue.

## Capture

Capture retains recent rendered frames. A bus capture is taken before that bus's
volume/balance; SHARP_CAPTURE_MASTER selects the final mix after LORI. Capture
storage is statically reserved even when disabled; disabled capture skips the
history copy, rather than eliminating all storage or per-block checks.

SHARP_GetCaptureMaxFrames returns the current per-capture limit, 2048 frames.
SHARP_SetCapture returns false for an invalid bus selector. Otherwise it returns
true: false enabled or zero frames disables capture; oversized requests clamp to
the limit. Enabling resets the retained history, including when already enabled.

SHARP_ReadCapture copies the most recent available frames, oldest first, and
returns the frame count. It returns zero for an invalid selector, null destination,
zero request, no history or failure to obtain a stable sequence within four tries.
The destination must hold max_frames times the effective channel count samples
of the effective format. Only the returned frame count identifies valid output;
a zero return does not promise an untouched destination.

SHARP_FMT_F32 returns floats; SHARP_FMT_S16 returns int16 samples. Other format
values currently select S16. Channels one and two select mono and interleaved
stereo; other values select stereo. Mono averages left/right. S16 conversion clips
values to -1..1 and scales by 32767; NaN converts to zero and infinities saturate.
Float output preserves the captured values without range-clamping or NaN cleanup.

The current capture implementation has shared ordinary buffers/metadata around
an atomic sequence counter. Its earlier arbitrary-thread safety guarantee has
not been substantiated and must not be relied on. Reconfiguration, live copying
and shutdown need a supported ownership/publication design before general
concurrent use can be guaranteed. Retrying a read is not itself that guarantee.
Capture configuration and retained history currently survive shutdown and
reinitialization. Explicitly disable or reconfigure capture when starting a new
session if old history must not be reused.

## Chip callback contract

SharpChip in sharp_chip.h is an independent source description: ctx, render,
write_reg and name retain their existing field names and order. The descriptor
borrows its context and name; it does not allocate, copy or free their contents.
Keep them alive while the registered source can be called. Copying the descriptor
does not clone chip state or transfer ownership.

render adds frames of interleaved stereo to output. The sequencing layer clears
the destination before accumulation, so a chip must preserve other chips' samples.
Calls may be split into segments at event boundaries. write_reg applies its value
immediately at the current render position; the sequencer supplies timing.

The current sequencer skips a null render or write_reg callback. A null callback
does not turn a malformed context into a validated source. The scheduler invokes
these callbacks serially on its rendering path; a device backend may also render
initial buffers during startup. Do not assume every call occurs on the eventual
worker thread. Callbacks should avoid blocking or allocating on the live audio
path and must not race mutation of their context.

## Event sequencing

sharp_events.h supports application-owned SharpSeq instances without the façade,
mixer or device backend. SHARP_InitSequence initializes a fresh record and allocates
an event queue. Zero capacity selects 1024; other capacities round up to a power
of two. Requests above 2^31, or rounded allocations exceeding size_t, are rejected
before allocation. It returns one on success and zero for a rejected capacity or
allocation failure. An accepted dimension is not a promise that its allocation
will fit available memory. SHARP_FreeSequence releases the queue after both producer and renderer
stop; it does not own chip contexts or reset every field. Free before reinitializing.

SHARP_AddSequenceChip copies a valid SharpChip descriptor and returns its zero-based
ID, or -1 when SHARP_MAX_CHIPS is reached. Context/name storage remains borrowed.
Chip registration and output routing must be coordinated with rendering; the event
queue's producer/consumer protocol does not synchronize those control changes.

SHARP_WriteSequence returns one when queued and zero when full. It does not reject
an unregistered chip ID or validate a register. At dispatch, events whose chip ID
is not registered are consumed without calling a source. Submit from one producer;
render from one consumer. Timestamps are absolute render frames and out-of-order
submissions clamp to the previous accepted timestamp, as in the façade interface.

SHARP_SetSequenceOutput accepts a valid chip and a slot from zero through 255,
returning one on success or zero for a null sequence/invalid chip/slot.
SHARP_GetSequenceOutput returns that slot, or -1 for a null sequence/invalid chip.
Slots are opaque destination indices: the sequencer has no dependency on LORI's
bus definitions. New chips start at slot zero.

SHARP_RenderSequence clears one interleaved stereo destination and renders into
it. SHARP_RenderSequenceOutputs clears all output_count destinations and routes
each chip to its selected slot. Slots outside the supplied destinations produce
no output; they are not redirected to another destination. Each destination needs
frames times two float elements. Use valid initialized records and buffers; these
functions are not general pointer/extent validators.

Rendering splits buffers at event boundaries and applies due register writes
before producing the corresponding frame. Events at the end position of a block
are handled when a subsequent render reaches that position. Zero frames dispatches
no events. SHARP_GetSequencePosition reports total frames rendered by that instance.
The three SHARP_GetSequence*EventCount queries expose its uint32 diagnostics.
Position, chip records and routing fields are not automatically safe for concurrent
control access simply because queue cursors and diagnostic counters are atomic.

## Audio ring

sharp_ring.h transports interleaved float frames independently of the event
sequencer and device backend. SHARP_InitAudioRing initializes fresh storage and
returns one on success or zero for invalid dimensions/allocation failure. Zero
frames requests one frame; capacity rounds to a power of two. Current bounds are
2^24 requested frames and one through eight channels. These limits are independent
of the event queue's capacity limits.

SHARP_WriteAudioRing and SHARP_ReadAudioRing return frames transferred, limited by
free space or available data. Buffers must hold the requested frames times the
ring's channel count. Reads do not pad shortages with silence; the backend chooses
that policy. SHARP_GetAudioRingReadableFrames and SHARP_GetAudioRingWritableFrames
report frame counts, not float-element counts.

Use one producer for writing and one consumer for reading. Cursor wrap uses
uint32 arithmetic and the capacity bound; do not modify record fields directly.
The writable query belongs on the producer side and the readable query on the
consumer side. Arbitrary third-thread inspection is not a coherent snapshot.
SHARP_ResetAudioRing resets both cursors without clearing sample storage and
requires both sides to stop. SHARP_FreeAudioRing also requires stopped users and
releases only the allocated buffer. Initialize successfully before transport calls.
These functions use C atomics, but their use alone does not promise lock-free
implementation on every compiler/target; verify that property for the chosen build.

## Backend interface

sharp_backend.h keeps SharpBackend opaque to ordinary stream consumers. It knows
neither chip types nor LORI. A SharpRenderFn fills frames of interleaved stereo
float output. Supply a valid callback, configuration and context that remains alive
until backend work stops. Device backends may call rendering while priming during
start and later on their worker; do not assume exclusively one OS callback thread.

SHARP_IsBackendAvailable returns nonzero when the kind has a table with an open
callback, not when a device is known to work. SHARP_GetBackendName returns a borrowed
table label or "(unknown)" when no table or label is present. DEVICE selects the compiled
platform backend; WAV requires an application-provided implementation. SHARP does
not ship a file writer in these production sources.

SHARP_OpenBackend returns a newly allocated handle or null, recording an error on
failure. Missing configuration, render callback or table open callback is rejected
with an error before allocation or dispatch. Other pointer and extension-state
validity remains the caller's responsibility. sample_rate is desired,
lead_frames controls backend-specific render-ahead and path is for extensions
that use it. A device may choose another rate; SHARP_GetBackendSampleRate reports
the opened rate, or zero for a null handle. It does not start output.

SHARP_StartBackend and SHARP_WaitBackend return zero for a null handle or missing
operation; otherwise they forward the backend result. Existing device operations
use one for success and zero for failure. Wait takes an absolute total frame target,
not a count to add, and can block. Waiting on an unstarted/stopped or failed backend
has not been given a uniformly safe timeout policy. Do not treat it as bounded.

SHARP_StopBackend stops work; SHARP_CloseBackend calls the backend close operation
and frees the generic handle. It does not first call the stop operation.
Null handles are ignored by both. Close must release
all backend users before its storage can disappear; failure-path ownership is still
under review. The public void return does not certify successful thread cleanup.
Serialize stream lifecycle, extension registration and access to borrowed handles.
The generic layer keeps no started/stopped state: repeated start, wait and stop
calls are forwarded, and the backend defines their permitted order and results.
Close releases the handle, so it cannot be repeated with that same pointer.

SHARP_GetBackendFrameCount returns the backend's atomic frame counter, or zero for
null. Its relationship to physical playback is backend-specific; it is distinct
from sequencer-rendered frames. SHARP_GetBackendError returns a borrowed error
buffer (the shared last-error buffer for null). SHARP_GetLastBackendError returns
that shared buffer directly. Later errors can overwrite these strings; they are
not immutable snapshots or synchronized arbitrary-thread diagnostics.
Successful operations do not clear the shared last-error string. A failed open
copies the handle error to it; an extension that reports no message leaves it
empty. Error formatting retains at most 255 bytes plus the terminator. A null
format selects an empty message; other format strings and arguments must form
a valid printf-style call.

## Backend extensions

sharp_backend_extension.h replaces the externally used parts of the old
sharp_backend_impl.h. It exposes SharpBackendVTable and the existing SharpBackend
record to backend authors. The field names, order and types remain unchanged.
sharp_backend_private.h separately declares the selected device implementation's
table; ordinary applications and optional backend authors need not include it.

SHARP_RegisterBackend returns zero for an out-of-range kind or DEVICE, which cannot
be replaced. It returns one for a registrable kind, including a null table that
unregisters it. Currently WAV is the only optional kind. Register an optional
table before opening it and keep its borrowed table/name valid until all handles
using it close.
Registration does not validate callback completeness or manage table ownership.
A table without open can be registered, but availability is false and opening it
returns an error. Other callbacks may be absent: start/wait then return zero,
and stop/close skip the missing operation. An extension that owns resources
still needs callbacks that release them. Replacing or unregistering a table
affects future opens; existing handles continue borrowing their original table.
Keep that original table alive and immutable until its handles close. Registration
also persists independently of facade shutdown.

The generic layer allocates/frees SharpBackend. An extension initializes its impl
resources in open, produces output through render/user, maintains rate/channels/
frames, and releases its resources in close. Failed open must clean its own partial
resources: generic open frees the handle without invoking close on failure. No
extension callback should free the generic handle itself. A custom implementation
defines its own file/stream policy; registration does not grant broader engine
filesystem services.

SHARP_SetBackendError formats into the handle's error buffer when non-null and
also replaces the shared last-error text. A null handle updates only the shared
text. It returns no status and does not make error reporting thread-safe. Preserve
context/table lifetime and check start/stop/close failure behavior for each
backend implementation.

## LORI composition interface

LORI (Left OR Right) mixes sixteen stereo buses independently of chips, event
scheduling, threads and devices. Its public subsystem functions use names such
as InitLori and SetLoriBusVolume. lori.h needs only standard integer types; lori.c
uses the C math library for tanhf.

The caller owns a Lori record. InitLori clears it, sets master gain, initializes
each bus to volume one and centered balance, and disables all buses. It allocates
nothing. Scratch holds 8192 float elements, or 32 KiB when floats are four bytes.
Do not copy a live mixer or modify its
controls concurrently with rendering without external coordination.

EnableLoriBus converts enabled to zero or one. SetLoriBusVolume clamps negative
finite values to zero and accepts values above one. SetLoriBusBalance clamps finite
values to -1..1. Each returns one for a valid bus, zero otherwise. IsLoriBusEnabled
returns the stored enabled flag, or zero for an invalid bus. SetLoriMasterGain
stores the supplied gain without range clamping or a status return. Supply valid
records and finite control/sample values; general non-finite handling is not
established by these interfaces.

RenderLori overwrites frames of interleaved stereo output. It divides large requests
into chunks of at most LORI_BLOCK (256) frames and calls LoriFillFn once per chunk.
The callback receives all sixteen scratch buffers, even for disabled buses. It must
populate or clear the portion used by each audible bus; LORI does not clear scratch
before calling it. Disabled buses skip summing, not the callback itself. Muting a
bus therefore does not guarantee its source stops rendering or costs no CPU time.

A null fill callback reuses stored scratch; it does not request fresh silence.
Partial callback writes can likewise leave earlier samples in place. Those are
existing behaviors, so callers must not assume that an omitted fill operation
clears a previously audible bus. Callback buffers are borrowed and reused each
chunk; do not access them asynchronously.

For each enabled bus with positive volume, LORI attenuates the opposite side
according to balance and sums buses in index order. Hard left silences the right
channel; it does not sum both channels into the left. The master stage applies
tanhf(sample * master_gain) to each output sample. ProcessLori applies only that
master stage in place and ignores bus settings. The master gain can invert polarity
if negative; it is not itself a nonnegative volume constraint.

Output buffers must hold frames times two floats in a representable object, and
RenderLori's output must not overlap its scratch inputs. Zero frames does no sample
processing or fill callback. Neither processing function returns an error status or
performs general pointer/extent validation. With valid buffers, ordinary processing
preserves the original accumulation order, balance law and soft-clipping formula.

## Adora sample playback

Adora is an independently usable sample chip with sixteen mono voices, equal-power
stereo panning and four-point cubic Hermite resampling. Its source files separate
instance/helpers, register dispatch, voice setup and rendering. The render file
keeps decoding/interpolation/envelope-step helpers together on the per-sample path.
It needs a caller-owned sample pool and the C math library, not a mixer or device.

InitAdora borrows a byte pool and records the device and fallback source rates.
Keep the actual storage at least pool_size bytes long and alive while voices read
it. Supply finite positive rates. Invalid/non-finite rates or an unrepresentable
playback increment prevent a new trigger; initialization itself returns no status.
Pool mutation, register writes and rendering require coordinated ownership.

GetAdoraSource returns a SharpChip descriptor borrowing that Adora instance.
RenderAdora adds interleaved stereo into the destination; it neither clears nor
master-clips it. Hermite interpolation and multiple voices can exceed a nominal
-1..1 range. The destination must hold frames times two floats without overlapping
sample data or chip state. GetAdoraActiveVoiceCount includes voices still fading
out. These interfaces do not validate arbitrary caller pointers or mutated records.

## Adora registers

ADORA_REG(voice, field) forms a 16-bit selector with stride 32. Voices are 0..15;
fields 0..16 are defined and 17..31 are reserved. The macro does not validate its
arguments before narrowing. WriteAdoraRegister ignores selectors whose resulting
voice index is outside the table, and ignores reserved fields. There is no public
register-read operation. Existing numeric selectors are preserved.

| Field | Register | Meaning |
| --- | --- | --- |
| 0, 1 | OFFSET_HI, OFFSET_LO | Byte offset in the pool, high/low halves |
| 2, 3 | LENGTH_HI, LENGTH_LO | Sample region length in bytes |
| 4 | PITCH | Unsigned 4.12 ratio; 4096 is unity |
| 5 | VOLUME | Clamped to 0..255 on register writes |
| 6 | PAN | Clamped to 0..255; 0 left, 128 center, 255 right |
| 7 | TRIGGER | Nonzero starts/retriggers; zero enters release |
| 8, 9 | LOOP_START_HI, LOOP_START_LO | Byte offset relative to the sample region |
| 10, 11 | LOOP_LEN_HI, LOOP_LEN_LO | Loop length in bytes |
| 12 | LOOP_MODE | 0 one-shot, 1 forward, 2 ping-pong |
| 13 | FORMAT | U8, S8, S16LE or S16BE |
| 14 | ENV_AD | Attack byte in the high half, decay in the low half |
| 15 | ENV_SR | Sustain byte in the high half, release in the low half |
| 16 | BASE_RATE | Source Hz; zero uses the chip's fallback source rate |

Every register constant above carries ADORA_REG_. A trigger latches sample format,
region and loop setup. Pitch, base rate, volume, pan and envelope controls also
update active voices. Setting LOOP_MODE to zero releases an active loop; changing
it to a looping mode does not activate a new loop until retrigger. Other unknown
loop-mode values currently take the one-shot path. Unknown formats select U8.

Playback increment is (base_rate / device_rate) * (pitch / 4096). The per-voice
base-rate register reaches 65535 Hz; the chip's fallback is a double and is not
limited to that register width. Pitch zero holds a position. Unrepresentable live
increment updates retain the prior playing increment rather than introducing
infinity into loop traversal.

A trigger rejects a missing pool, zero length, offset outside the pool or a region
with no complete sample. Oversized region length clamps to the remaining pool
without wrapping. A rejected trigger preserves the prior playing sample, if any;
it is not a stop command. S16 lengths and loop positions convert from bytes using
integer division, discarding an incomplete trailing byte. Loop ranges clamp within
the sample; an out-of-range start or fewer than two loop samples disables looping.

U8 maps (byte-128)/128, S8 maps signed byte/128, and S16 maps signed word/32768
with explicit byte order. Interpolation taps clamp at one-shot edges and fold into
the loop after its first wrap. Signed decoding and interpolation-index arithmetic
use wide, defined intermediates; this does not certify that a caller's pool-size
claim matches its actual allocation.

## Adora envelopes and transitions

Attack, decay and release bytes use (value/255)^2 times four seconds; zero requests
an immediate envelope increment. Sustain is a 0..255 level. Defaults are attack
zero, decay zero, sustain 255 and release zero. Retrigger restarts the envelope.
Key-off and sample exhaustion still have the declick tail, so zero release does
not promise that every output sample becomes zero immediately.

Retrigger crossfades from the previous post-gain left/right output. Mid-note
volume/pan changes follow a one-pole gain ramp. ADORA_DECLICK defaults to 48 frames,
not a rate-independent duration; it is one millisecond at 48 kHz. It must be a
positive int. ADORA_GAIN_RAMP_MS defaults to a 0.67 millisecond time constant and
is independently overridable. Supply a finite positive gain-ramp constant and
compile all Adora files with consistent overrides. No synthesis retuning is made
by this migration.

## Adora note helper

PlayAdoraNote selects the first inactive voice, writes offset/length/pitch/volume/
pan, resets its base-rate register to zero and attempts a trigger. It returns the
selected index or -1 if no voice is free or a numeric conversion is non-finite/
outside the uint16 destination range. The returned index is not confirmation that
pool/rate validation allowed playback. Fading voices still occupy their slots.

The helper retains the selected voice's format, loop and envelope registers; it
does not reset a recycled voice completely. Configure them explicitly when needed.
It rounds pitch_ratio*4096, gain*255 and (pan/2+1/2)*255 using the existing half-unit
offset. Unlike register writes, helper values are not clamped to the musical
volume/pan ranges before conversion. Ordinary gain 0..1 and pan -1..1 are the
intended inputs; representable extended values retain the previous behavior.

Practical rates and pitch increments are still required. Extreme finite steps
can make repeated loop folding expensive, and no hard per-call latency bound is
established. General numeric-domain policy, efficient large-step folding and live
pool/control synchronization remain further hardening work.

## Sweeny synthesis

Sweeny is an independently usable subtractive chip with eight voices and four
shared stereo filters. It has no dependency on Adora, LORI, the sequencer or a
device backend. Its instance/adapters, voice control, register dispatch and render
implementation are separate files. Per-sample oscillator, noise, envelope-step
and filter arithmetic stay together in the render file.

InitSweeny initializes caller-owned storage at a device rate. Use a finite positive
rate suitable for audio. Defaults are triangle wave, square pulse width, centered
pan, full voice/master volume, dry routing and inactive voices. Filters start at
maximum cutoff, zero resonance and low-pass mode. Noise generators have distinct
initial seeds. GetSweenySource returns a SharpChip descriptor borrowing the instance.

RenderSweeny adds interleaved stereo without clearing or master-clipping. Supply
frames times two floats in a valid output object, without aliasing chip state.
GetSweenyActiveVoiceCount counts active envelopes, including silent sustain states;
it does not measure audibility. Filter tails can continue after all voices become
inactive. Register changes, rendering and object lifetime need one coordinated owner.

Invalid zero/negative/non-finite rates refuse new gate-on operations and leave
render output untouched. An unrepresentable frequency increment does not replace
the previous finite increment. These guards do not establish a complete supported
rate domain or bound CPU cost for every extreme finite input.

## Sweeny registers

SWEENY_REG(voice, field) forms a 16-bit selector at voice times 32. Voice indices
are 0..7. Register macros form/narrow numbers; they do not validate arbitrary
arguments. All existing selectors and public record layouts are retained.

| Voice field | Register | Meaning |
| --- | --- | --- |
| 0, 1 | FREQ_HI, FREQ_LO | High/low halves of unsigned 16.16 Hz |
| 2 | PULSE_WIDTH | Duty fraction divided by 65536; 32768 is square |
| 3 | WAVEFORM | Triangle 0, saw 1, pulse 2, noise 3; not a bitmask |
| 4 | GATE | Nonzero restarts attack; zero enters release |
| 5 | VOLUME | Clamped to 0..255 |
| 6 | PAN | Clamped to 0..255; 0 left, 128 center, 255 right |
| 7 | ENV_AD | High-byte attack and low-byte decay |
| 8 | ENV_SR | High-byte sustain and low-byte release |
| 9 | CONTROL | SYNC, RING, BANDLIMIT and TEST flags |
| 10 | FILTER_ROUTE | Zero dry; 1..4 select an existing filter |
| 11..31 | Reserved | Writes ignored |

Voice constants carry SWEENY_REG_. SWEENY_REG_GLOBAL is 256. Each filter has a
stride-eight block selected by SWEENY_REG_FILTER: CUTOFF at +0, RES at +1 and
MODE at +2; +3..7 are reserved. Only filters 0..3 exist. Space for filters 4..7
is reserved and ignored. SWEENY_REG_MASTER_VOLUME is 320, above those eight
reserved filter blocks, and clamps to 0..255. Other unimplemented globals are ignored.

Frequency halves update the oscillator increment immediately, so write ordering
matters if rendering occurs between the two writes. Volume, pan, envelopes and
route/control changes also affect active voices. Unknown waveform values select
triangle. Filter modes are masked to their three defined response bits; other
unknown control bits are stored but have no implemented effect.

Gate-on resets envelope level and attack state but preserves oscillator phase.
TEST is the separate phase-zero control. Gate-off does not clear filter state.
Use the sequencer when writes need specified render-frame timing; the chip itself
applies writes immediately and knows no event timestamps.

## Sweeny oscillators and envelopes

Oscillators advance phase before generating each sample. Triangle, saw, pulse and
clocked noise retain their existing formulas. BANDLIMIT applies PolyBLEP edge
correction to saw and pulse only; it is inactive for triangle and noise. This is
not a promise of alias-free output at every rate/frequency.

SYNC and RING use the preceding voice, wrapping voice zero to voice seven. Voices
are visited in ascending order, so voice zero observes its neighbor's state from
the preceding frame, while later voices see the preceding voice's current update.
This existing skew is part of the preserved behavior. RING affects triangle only.
SYNC uses the neighbor's wrap flag to reset phase. TEST holds phase at zero but
does not generally mute the voice, stop its envelope or stop the noise clock.

Noise uses a shift register clocked from oscillator increment times sixteen;
the current noise level is held between clocks. Extreme finite increments can
require excessive clock iterations, so practical audio rates remain necessary.

Envelope bytes use (value/255)^2 times four seconds. Attack is linear; decay and
release approach their targets exponentially. The exponential coefficient uses
five time constants over the encoded duration, with completion threshold 0.0005.
Zero requests an immediate increment/coefficient. Sustain is value/255. Defaults
are attack zero, decay zero, sustain 255 and release zero. Sweeny shares register
encoding conventions with Adora, not Adora's linear envelope implementation.

## Sweeny filter bank

Each filter sums its routed voices after their individual pan/gain, then processes
left and right independently. Invalid route values fall back to dry output;
unlike a missing sequencer output slot, they do not silence the voice.

Cutoff maps logarithmically from 30 to 12000 Hz and clamps to 0.49 times device
rate. Resonance clamps to 0..255 and selects the existing damping range. Filter
coefficients are recomputed per RenderSweeny call; sequencer segmentation therefore
also determines when register changes produce new coefficients.

The state-variable filter supplies low-pass, band-pass and high-pass responses.
MODE combines the selected responses; zero selects no filtered output. Every
filter's state advances each frame even without new routed voices, preserving
tails and settling behavior. Dry and filtered outputs are combined and scaled
by master volume. No new stability, fidelity or universal numeric-safety guarantee
is inferred from the topology alone; the current arithmetic is preserved.

## Amy registers and synthesis

Amy is a six-voice PSG with two shared noise generators and two shared envelope
generators. It is inspired by AY/YM-style synthesis; exact hardware emulation is
not its current contract. It uses standard C and math, with no dependency on
another chip, mixer, sequencer or audio device.

InitAmy initializes caller-owned state at the device sample rate. Volume starts
at zero, pan at 128, tone enabled, clock at 1773400 Hz, master volume at 255, and
the AY8910 volume curve selected. GetAmySource returns callbacks referring to that
same state; keep it alive until callbacks finish. Serialize initialization, register
writes and rendering. Do not modify exposed state fields behind the renderer.

RenderAmy adds to an interleaved stereo destination with room for twice the frame
count in floats. Clear it first when replacing existing audio. Normal audio rates
such as 44100 and 48000 Hz are expected. Nonpositive or nonfinite rates leave the
destination unchanged. This guard does not validate every positive rate or clock.
Pointers, buffer extent and state validity remain caller requirements.

Voice registers use AMY_REG(voice, field), voices 0..5 and stride 32:

| Field | Meaning |
| --- | --- |
| 0 TONE_PERIOD | Low 12 bits; zero acts as one |
| 1 VOLUME | Clamped 0..255; sixteen fixed levels by default |
| 2 PAN | Clamped 0..255; 0 left, 128 center, 255 right |
| 3 CONTROL | TONE enables tone; FINE_VOLUME interpolates fixed volume |
| 4 NOISE_ROUTE | Zero disables noise; 1..2 selects a shared generator |
| 5 ENV_ROUTE | Zero uses fixed volume; 1..2 selects a shared envelope |
| 6..31 | Reserved; writes ignored |

Generator indices in AMY_REG_NOISE and AMY_REG_ENV are zero-based. Routes are
one-based; AMY_ROUTE_TO(index) converts an index to a route. Invalid noise routes
disable that source. Invalid envelope routes fall back to fixed volume.

The global block starts at 192. Both generator banks reserve eight entries with
stride eight; only the first two entries in each bank are implemented. Writes to
reserved generators or fields are ignored.

| Bank | Fields |
| --- | --- |
| Noise, base 192 | 0 PERIOD (low 12 bits), 1 MODE, 2 LENGTH, 3 SOURCE |
| Envelope, base 256 | 0 PERIOD_HI, 1 PERIOD_LO, 2 SHAPE (low four bits) |
| Globals, base 320 | 0 CLOCK_HI, 1 CLOCK_LO, 2 VOLUME_TABLE, 3 MASTER_VOLUME |

The clock and each envelope period are 32-bit values made of two complete
16-bit halves, HI above LO. An AY-style 16-bit envelope period goes in
PERIOD_LO with PERIOD_HI zero, which initialization provides, and then runs at
the AY's rate; PERIOD_HI extends the range to 2^32 - 1 for envelopes slower
than the AY allows, from one step every 0.296 s at period 65536 to one every
19,370 s at the maximum with the default clock. The step rate follows
clock/(8 * period) over the whole range. Each half-write takes effect at once,
so write HI before LO when both change, or submit both at one sequencer
position.
A zero clock is treated as 1 Hz. An unknown volume-table selector resets to the
default; master volume clamps to 255. Unknown control bits are stored but ignored.

Noise modes are WHITE_AY, WHITE_SN, PERIODIC and WHITE_SEGA. Unknown modes select
WHITE_AY. Initialization seeds each generator within the 17 stages of WHITE_AY,
generator zero with all ones and generator one with a different fixed value, so
no state bit lies outside the selected width. Writing mode or length reseeds
the shift register to one, even when the value is unchanged. Periodic length
zero means 15; lengths above 31 use 31.
SOURCE zero uses the generator's period. Values 1..6 use rising edges from the
selected voice, independently of that voice's output controls. Other sources do
not advance the generator. Tone/noise period writes retain their running count.

The tone/noise mixer ANDs enabled one-bit sources; a disabled source reads high.
With both disabled, an envelope can sound directly as a buzzer. A valid envelope
route overrides fixed volume, including zero volume; FINE_VOLUME affects only
fixed volume. The unipolar mix is averaged across eight substeps and then passed
through one DC-blocking filter per channel. Master volume follows the filters.
Their stored state can produce a transient after all voices become silent.

Volume curves are five constant 32-entry tables, so chips share no mutable
state and initialization order does not matter: three generated curves
(16-level 3 dB, 32-level 1.5 dB, 16-level 2 dB), written as the exact float
results of their formulas, and two literal tables, the measured AY-3-8910 and
YM2149 levels from ayumi's AY_dac_table and YM_dac_table. Fixed levels use odd
entries of the 32-entry table; fine volume interpolates adjacent entries. Envelope
levels index the table directly. Retaining these values does not establish a new
hardware-fidelity claim or finish the upstream provenance review.

### Amy envelope timing

The four shape bits select hold, alternate, attack and continue behavior. A shape
write always restarts its envelope, even if unchanged. Alternate repeating shapes
repeat their endpoint on direction changes. Period zero acts as one, as it does
for tone and noise. Step rate is clock/(256 * period) * 32; a triangle needs two
ramps for its full cycle.

A period or clock write retimes the running step at once, as a counter that
compares against the new period on its next tick does: a step already owed
fires once at the next sample, never as a burst. Envelopes advance at output-sample boundaries, while tone and noise advance at
the eight substeps. Initialization derives step timing from period one and the
default clock, so a shape write alone runs its envelope at that timing. Until a
shape is written, each envelope runs its default shape-zero decay from full level
during the first few samples and then rests at zero, as hardware does after
reset. Period writes do not restart an envelope; only a shape write does.

A holding envelope does not advance until a shape write restarts it, and a
shape that reaches its hold stops stepping within that frame. An envelope owed
more than 256 steps in one frame, which only an extreme clock or period
produces, has whole repeating cycles removed arithmetically (32 steps, or 64 for
an alternating shape) and steps through the remainder individually, so a frame
costs at most one cycle of steps per envelope. Ordinary clocks never reach that
bound and step exactly as before.

### Amy divider timing

Each tone and noise counter accumulates clock/8 ticks per substep. Every whole
period in the count is an expiry, however many there are: a tone keeps the
parity of its expiries, a noise SOURCE receives one shift per rising edge, and
an own-clocked noise shifts on every second expiry. The remainder carries into
the next substep, so frequencies follow clock/(16 * period) at every supported
rate and 12-bit period, including ratios where several periods elapse within one
substep. A period written below the count already owed forces a single expiry at
the next substep and discards the excess, as a counter that has passed its new
period does; the ticks of that substep are kept. One noise generator shifts at
most 64 times per substep, which no realistic clock approaches. An extreme
clock still costs more than an ordinary one, because every owed expiry and
every shift up to that cap is handled, but the cost is bounded per frame.

### Amy output filtering

The unipolar tone/noise mix passes through a one-pole DC block with a 10 Hz
corner before the master volume: y = x - x_prev + c * y_prev with
c = exp(-2 * pi * 10 / rate). The exponential form keeps c inside (0, 1) at every
finite positive rate, where the earlier 1 - 2 * pi * 10 / rate left the stable
range below 31.4 Hz and made a 10 Hz rate diverge. From 8 kHz to 96 kHz the two
agree within 0.03% in corner and time constant: a level step settles below one
ten-thousandth in 146.6 ms at every supported rate, and ordinary output differs
from the earlier coefficient by about -65 dB RMS. Any finite positive rate
renders stably; below a few hundred hertz the block passes little but
differences, which is a property of the rate rather than a fault. Nonfinite
and nonpositive rates render nothing.

### Amy activity reporting

GetAmyActiveVoiceCount reports voices with a running source reaching the
output, in 0..6. A voice counts when its tone is enabled or its noise route is
valid and its level is nonzero: a nonzero fixed volume, or a routed envelope
that is moving or holding above zero. A moving envelope also counts on its own,
since the disabled sources read high and the envelope buzzes through them. A
noise or envelope route the mixer ignores is ignored here too, an envelope
resting at silence after its shape has completed does not count, and a fixed
volume with no tone, noise or moving envelope does not count, as it only
contributes a DC level. The count follows register and envelope state, not
audio output: a holding envelope at full level with a tone enabled counts
whether or not anything is heard. It must not be used to skip rendering or stop
an audio source; such skipping can also cut off DC-filter settling.

## Jenny registers and synthesis

Jenny supplies eighteen voices with four phase-modulated operators per voice,
eight connection algorithms and eight waveform choices. It combines YM-inspired
FM with additional waveforms and fixed vibrato/tremolo controls. It is not a
cycle-accurate YM2612 or OPL emulator. It uses standard C and math without another
chip, mixer, sequencer, engine clock or audio device.

InitJenny initializes silent caller-owned state. The default pan is 128, volume
255 and operator multiple one; envelope stages are idle at 96 dB attenuation.
An unprogrammed attack rate is zero, so write a patch before key-on. GetJennySource
returns callbacks referencing that same object. WriteJennyRegister and RenderJenny
retain their callback-compatible void* context signatures; the context must point
to Jenny. Serialize control and render access, and keep state alive for callbacks.

RenderJenny adds interleaved stereo frames to a caller-owned buffer holding twice
the frame count in floats. Clear it first to replace existing samples. Use an
ordinary finite positive audio rate, such as 44100 or 48000 Hz. Nonpositive and
nonfinite rates return without changing output or LFO phases. This guard does
not validate every extreme positive rate, buffer extent or directly modified state.

The register address is JENNY_REG(voice, field), voices 0..17, stride 32.
Registers 576 and above and reserved fields are ignored.

| Voice field | Meaning |
| --- | --- |
| 0 FREQ_HI, 1 FREQ_LO | Two halves of unsigned 16.16 Hz; each write applies separately |
| 2 ALG | Low three bits select algorithm 0..7 |
| 3 FEEDBACK | Low three bits select operator-one feedback depth |
| 4 PAN | Low byte: 0 left, 255 right, 128 near center |
| 5 VOLUME | Low byte: pre-DAC voice gain, 255 unity |
| 6 KEY | Zero key-off, two resume strike, other nonzero values damped strike |
| 7, 28..31 | Reserved |

PAN and VOLUME retain the low byte; they wrap rather than clamp values above
255. Operator addressing is JENNY_OP_REG(voice, operator, field), operators 0..3:

| Operator field | Bits used |
| --- | --- |
| 0 MULT_DT | 8..11 multiple, 0..2 detune |
| 1 TL_RS | 8..9 rate scaling, 0..6 total attenuation |
| 2 AR_D1R | 8..12 attack rate, 0..4 first-decay rate |
| 3 SL_D2R | 8..11 sustain attenuation, 0..4 second-decay rate |
| 4 RR_WS | 8..11 release rate, 4 tremolo enable, 3 vibrato enable, 0..2 waveform |

Raw operator writes are retained in full; rendering uses the selected bits of
the committed values. Multiple zero means one-half, otherwise 1..15. Detune is
sign-magnitude; code four is negative zero. Detune is applied before the multiple,
and negative resulting frequency clamps to zero. Total attenuation uses 0.75 dB
steps. The highest sustain-level code selects 93 dB rather than 45 dB.

### Jenny writes and key lifecycle

The r_ fields hold pending registers; l_ fields hold committed registers. Voice
frequency, algorithm, feedback, pan and volume apply live while keyed and outside
a pending voice transition. Writes during release or a pending transition remain
pending. Operator writes always remain pending until an operator commit; total
level is not a live control in the current implementation. Use voice VOLUME for
live expression.

A nonzero KEY write acts only when keyed is false. To retrigger a held note,
write zero first; writing one or two repeatedly while keyed does nothing.
KEY zero moves operators to release, or cancels their pending damped restart.

KEY one starts silent operators directly and damps sounding operators before
their phase reset and new attack. Attenuation at or above 60 dB counts as silent
for this decision. The damp increases attenuation toward 96 dB at 96000 dB/s.
KEY two commits the patch and attacks from current attenuation without damping
or resetting a sounding operator's phase; operators classified silent restart
their phase and feedback history. KEY two can therefore change timbre abruptly
when it also commits a different patch.

During a damped transition, operators can finish and commit at different samples.
The first pending operator to finish also commits voice-level settings. Some
voice-level calculations are cached for the render call, so their audible change
can wait until the next call. Buffer division can affect this transition's sound.
The current behavior is retained pending a coordinated transition fix.

### Jenny envelope and modulation

Envelope stages are idle, attack, first decay, second decay, release and forced
damp. Attenuation runs from zero (loud) to 96 dB (silent). Keycode is approximated
from Hz and drives detune and rate scaling. Ordinary envelope rates retain
ENTERON's continuous approximation anchored to 53267/3 ticks per second.
The fastest attack instead uses a fixed 0.35 coefficient per output sample;
its wall-clock duration therefore changes with the device rate.

Eight algorithms connect earlier operators to later ones, with feedback on the
first operator. Carrier lists use one-based operator numbers:

| Algorithm | Connections | Carriers |
| --- | --- | --- |
| 0 | 1 > 2 > 3 > 4 | 4 |
| 1 | (1 + 2) > 3 > 4 | 4 |
| 2 | (1 + (2 > 3)) > 4 | 4 |
| 3 | ((1 > 2) + 3) > 4 | 4 |
| 4 | 1 > 2, 3 > 4 | 2, 4 |
| 5 | 1 > (2, 3, 4) | 2, 3, 4 |
| 6 | 1 > 2 | 2, 3, 4 |
| 7 | Independent operators | 1, 2, 3, 4 |

Waveforms are sine, half sine, rectified sine, quarter pulses, doubled sine gated
to the first half-cycle, its rectified form, square and an approximate exponential
saw. Waveform phase is wrapped after modulation. No anti-aliasing guarantee is
made for high pitches, strong modulation or discontinuous waveforms.

The fixed global LFO pair supplies vibrato at 6.1 Hz and tremolo at 3.7 Hz.
Enables are per operator. Shared phases advance even when all voices are idle.
Their waveform is evaluated at each sample from the render-call phase base.
There is no configurable YM-style LFO rate/depth or SSG envelope mode yet.

### Jenny output

Carrier outputs are summed, multiplied by voice volume, clamped to -2..2 and
quantized downward in 1/128 steps. Nonnegative values then receive a positive
1/128 ladder offset. The result is multiplied by one-half and equal-power pan
gains before accumulation. These are retained synthesis choices, not newly
verified hardware measurements.

Zero DAC input becomes a positive offset. Consequently zero volume is not an
exact mute for a voice being processed. Idle voices are skipped at the beginning
of the call, but a voice that becomes idle mid-call continues through that call's
output path. DAC-offset duration can therefore depend on buffer division too.
There is no chip-wide limiter or DC blocker, and eighteen voices can exceed the
final mix's output range. Do not assume a fixed maximum of unity.

## Facade composition

The facade owns one sample pool, sequencer, LORI mixer, backend handle and table
of built-in chip instances. Component users can instead own those objects
themselves; the standalone components do not call back into facade state.

The backend asks the facade to render frames. LORI divides the request into
chunks, and each fill callback asks the sequencer for the corresponding bus
outputs. The sequencer splits those chunks at register-event boundaries. Bus
history is captured before bus gain, balance and enable affect the mix; master
history is captured after LORI's output processing.

All four built-in chips use the negotiated backend rate. Adora receives that
rate as its default sample source rate and shares the facade pool; its per-voice
BASE_RATE register can describe samples authored at another rate. Amy retains
its independent chip-clock register. Neither chip's content rate should be
inferred from the originally requested 48000 Hz backend configuration.

Implementation files separate lifecycle (sharp.c), sample writes/configuration
(sharp_pool.c), chip creation/routing/register submission (sharp_chips.c), mixer
composition (sharp_mixer.c), and capture storage/conversion (sharp_capture.c).
sharp_facade_private.h contains their shared private state. Build these five
files together with the selected backend and reviewed component sources; sharp.c
alone no longer contains the full facade. The standalone Makefile selects these sources with one platform backend.
Native Linux validation remains pending; the macOS status is in its platform note.

## Windows output

The selected Windows backend uses shared, event-driven WASAPI and native Win32
threads. [SHARP_WINDOWS.md](SHARP_WINDOWS.md) documents supported formats, channel
mapping, buffering, submission counters, lifecycle requirements and build sources.
Native header/link checks and controlled failure tests do not replace playback,
device-loss, latency or load testing.

## macOS output

The selected macOS backend uses a DefaultOutput AudioUnit and a POSIX render-ahead
worker. [SHARP_MACOS.md](SHARP_MACOS.md) describes configuration, callback buffer
checks, channel mapping, frame counters, watchdog behavior and validation limits.

## Platform threads

sharp_thread.h is a private backend helper, not a public scheduling service.
Select sharp_thread_windows.c on Windows or sharp_thread_posix.c on macOS/Linux.
The platform notes describe ownership and error limitations. Keep user/backend
state alive until workers have finished; an ignored join error cannot establish
that completion. Neither helper promises real-time deadlines.

## Linux output

The selected Linux backend uses ALSA's configured default PCM and POSIX workers.
[SHARP_LINUX.md](SHARP_LINUX.md) documents format/buffer negotiation, partial-write
handling, recovery, counters and native-validation limits. "default" does not
guarantee shared access or that every sound-server installation is configured.

## Building SHARP

The Makefile in fuselage/source/SHARP builds libsharp.a independently of Fuselage,
GDMF, CAKE, DICE and guest execution. It requires GNU Make, a C11 compiler accepting
GCC/Clang command-line options, an archiver and the selected platform's development
headers/libraries. Clang is the default compiler. Windows uses a MinGW-w64-compatible
toolchain and mingw32-make with cmd.exe recipes; MSVC/clang-cl builds are not supplied.
No Python, CMake, reference emulator or test harness is needed to build this archive.

From the repository root (use mingw32-make on native Windows):

```sh
make -C fuselage/source/SHARP
make -C fuselage/source/SHARP print-libs
make -C fuselage/source/SHARP print-sources
```

Default output is fuselage/build/SHARP followed by windows, macos or linux.
The compiler target selects the platform. PLATFORM can select it explicitly for
cross-compilation, but must match the compiler and installed target SDK; changing
this variable does not supply that SDK. Each platform includes one device table
and one thread implementation, alongside the common facade/components.

| Platform | Selected output | Application link dependencies |
| --- | --- | --- |
| Windows | WASAPI, Win32 threads | -lole32 -lm |
| macOS | CoreAudio, POSIX threads | -framework AudioToolbox -framework CoreAudio -pthread -lm |
| Linux | ALSA, POSIX threads | pkg-config --libs alsa, then -pthread -lm |

Linux obtains ALSA include/link flags with pkg-config. PKG_CONFIG, ALSA_CFLAGS and
ALSA_LIBS can override discovery for another prefix, sysroot or cross toolchain.
CC, AR, CPPFLAGS, CFLAGS and BUILD_DIR are also configurable. For example, from
the SHARP directory:

```sh
make CC=clang AR=llvm-ar BUILD_DIR=out CFLAGS="-O2 -g"
```

BUILD_DIR must be nonempty and contain no whitespace; checkout paths with spaces
work with the default relative output path. Keep separate build directories for
different targets. Changing compiler, target or flags requires a clean rebuild.
The Makefile keeps compiler-generated header dependencies, updates objects for
changed headers, and recreates the archive without stale members. The clean target
removes only this platform build's listed objects, dependency files and archive;
it leaves the directory and unrelated files intact.

Include sharp.h for the facade or the component headers for direct composition,
using fuselage/source/SHARP as the include path. Link libsharp.a before the options
reported by print-libs. The static archive does not itself absorb native libraries.
An application using only independent chip/mixer/sequence components can compile
those sources directly or let the linker extract the needed archive members; the
usual full-facade link uses the platform dependencies above.

The archive contains production code only. Native device probes, comparison
fixtures, tracking notes and generated build outputs are not archive inputs.
An application-supplied optional backend can register through
sharp_backend_extension.h; no WAV file writer is bundled. Public source/docs,
internal tests/tracking and generated artifacts remain separate ahead of the
project-wide distribution pass.

The native Windows archive and a memory-only standalone consumer were verified
during migration. macOS/Linux manifests and commands were checked, but their native
SDK/library/device builds remain pending. Platform notes retain the precise limits.
