# SHARP on macOS

## CoreAudio output

The macOS backend uses a DefaultOutput AudioUnit from plain C. It reads the unit's
output-side stream rate and channel count, then requests interleaved float32 on
its input side. A nonpositive reported rate falls back to the requested rate or
48000 Hz, and zero reported channels fall back to stereo. Nonfinite rates, rates
that cannot round to a positive uint32 value, and overflowing channel/byte extents
are rejected. The backend reports the rounded rate; CoreAudio's client description
retains its floating-point rate.

The software ring lead defaults to reported_rate/50 frames and is raised to at
least twice the default output device's buffer size and one 256-frame render
block. Capacity rounds to a power of two within the audio ring's bounds. A failed
device-buffer query uses 512 frames. Scratch capacity is the largest of the
reported maximum slice, device-buffer size and 4096 frames. These defaults provide
headroom; they are not measured latency or dropout guarantees.

Open installs the callback and initializes the unit. Start primes the ring through
the engine render callback, creates a POSIX render-ahead worker, and starts the
unit. CoreAudio owns the device callback thread; SHARP creates no second audio
consumer thread. The worker renders blocks of 256 stereo float frames and briefly
sleeps when the ring cannot accept a complete block.

## Callback buffers

The callback expects one interleaved float buffer with the configured channel
count. Stereo goes to its first two channels, mono receives the left sample only,
and additional channels receive zero. This retains the existing mapping; it is
not a stereo downmix or speaker-layout remapping.

The callback checks buffer count, data pointer, channel count and mDataByteSize
before writing the requested frames. Invalid layouts or short storage are cleared
only within the supplied byte extents and return kAudio_ParamError without consuming
the ring or counting the request as rendered. A zero-frame callback returns
success without touching storage. Apple's [AudioBuffer documentation](https://developer.apple.com/documentation/coreaudiotypes/audiobuffer)
describes the buffer size and channel fields used for these checks.

A valid request larger than scratch capacity returns silence within the verified
buffer, marks output silent, increments the diagnostic count and advances the
callback frame counter. An ordinary short ring read pads the missing stereo frames
with silence before channel mapping. The counter describes frames returned by
successful callbacks, including silence; it is not an independently measured
physical-playback clock. The private underrun query currently also includes
invalid/oversized callback requests and is not a portable public diagnostics API.

## Lifecycle and waits

Repeated start while running returns success without creating another worker.
Worker creation and unit-start failures clear running state and release any
created worker. Stop requests that the AudioUnit stop, clears running, and joins
an owned worker even if its running flag was already cleared. Close then releases
the unit, ring and scratch resources. The underlying stop/uninitialize/dispose
results are not fully surfaced; successful callback quiescence on native failure
paths still requires validation. Apple's [AudioOutputUnitStop description](https://developer.apple.com/documentation/audiotoolbox/audiooutputunitstop(_:))
does not replace that backend ownership audit.

Wait uses an absolute callback-frame target. Below that target it fails if stopped
or already marked lost. Its current watchdog declares loss after 2000 accumulated
milliseconds of sleep requests without counter progress. That is a heuristic,
not measured elapsed time or proof of unplugging. Loss also clears the worker's
running flag; stop/close still joins its thread. A lost handle requires reopening.
A normal stop retains queued samples and counter state for a later restart.

Default-device, rate and channel changes require native testing. The selected
AudioUnit's behavior does not establish a complete SHARP recovery/reconfiguration
policy, and the backend does not currently publish a new negotiated configuration
after opening.

## POSIX threads

sharp_thread_posix.c implements the private helper interface for macOS and Linux.
It uses pthreads and nanosleep. The Linux POSIX feature-test macro is set before
headers unless the build supplied one. Creation returns a handle or null and
borrows user context until successful join. Null functions are rejected; null join
is ignored. Join failure retains the helper allocation, but the void API cannot
report completion to its owner; this is not complete failure recovery.

On macOS, creation attributes request USER_INITIATED QoS, retaining the existing
scheduling intent. Attribute/QoS failure can fall back to inherited behavior; no
deadline, audio-workgroup participation or FIFO policy is requested. See Apple's
[quality-of-service guidance](https://developer.apple.com/library/archive/documentation/Performance/Conceptual/EnergyGuide-iOS/PrioritizeWorkWithQoS.html).
The worker renders ahead rather than participating in the current device cycle.
Sleep converts milliseconds to seconds/nanoseconds and resumes the remaining
interval after EINTR. Other sleep errors are not reported by the current helper.

## Build and verification

Select sharp_coreaudio.c, sharp_coreaudio_stream.c, sharp_thread_posix.c, the
common backend and audio ring, linking AudioToolbox and CoreAudio frameworks.
Do not also build Windows threads or the old combined sharp_thread.c. macOS
pthreads are supplied by the system; Linux thread builds use their pthread flags.
The standalone Makefile selects these sources for PLATFORM=macos.

This migration was checked on Windows with test-only AudioUnit/CoreAudio and
pthread/time substitutes, original-source callback comparisons and sanitizers.
Those substitutes are internal test fixtures, not Apple headers, an ABI model,
or native platform verification. Compile/link against a real macOS SDK and run
device/lifecycle/load tests before declaring this backend validated. Native Linux
thread validation is likewise outstanding. No device was opened during this pass.
