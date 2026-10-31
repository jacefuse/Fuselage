# SHARP on Linux

## ALSA output

The Linux device backend opens the ALSA PCM named "default" for blocking,
interleaved playback. That name is supplied by ALSA configuration. It can route
through direct hardware, mixing plugins or an installed sound-server compatibility
plugin; SHARP does not guarantee its presence, shared access or universal routing
through PulseAudio/PipeWire. No native PipeWire backend is implemented here.

Open first requests native-endian float32, then native-endian PCM16 if float is
unavailable. Failure of both formats is reported. It asks for two channels and
the configured rate (48000 Hz when unspecified), adopting the values returned by
the near-setting operations. The rate reported by SHARP therefore need not match
the initial request. Zero rate/channels and invalid buffer dimensions are rejected.

SHARP_ALSA_DEVICE_MS defaults to 20 and requests a total PCM buffer duration in
milliseconds, with a minimum request of 256 frames. The period request is one
quarter of that buffer, with a minimum request of 64 frames. ALSA may choose
different values; SHARP checks buffer/period readback and sizes storage from those
results. Software start threshold is the granted buffer size, and avail_min is
the granted period size. Setting failures are reported rather than ignored.

Software ring lead defaults to negotiated_rate/50 frames and is raised to at least
twice the granted PCM buffer and one 256-frame render block. Capacity rounds to a
power of two within the audio ring's bounds. This software lead is separate from
ALSA/plugin/device buffering. These defaults are tuning choices, not measured
latency or dropout guarantees.

## Stream flow and counters

Start fills the software ring synchronously through the render callback, prepares
the PCM and creates render/device workers through the private POSIX helper. The
render worker produces 256 stereo float frames at a time and briefly sleeps when
the ring lacks space for a complete block.

The device worker takes one period from the ring, pads a shortage with silence,
and converts it into the negotiated channel layout. Mono receives left only;
stereo occupies the first two channels and extra channels are zero. Float samples
are copied unchanged. PCM16 clips to -1..1, applies the retained sign-dependent
rounding and scales by 32767. NaN becomes zero and infinities saturate.

The converted period remains pending until ALSA accepts it or the stream stops
or fails. Positive short writes advance the buffer pointer by the accepted frame
count and retry the remainder. Zero/EAGAIN results sleep briefly before retry.
Recoverable negative results use snd_pcm_recover and retry the remaining period
after recovery. The [ALSA PCM reference](https://alsa-project.org/alsa-doc/alsa-lib/group___p_c_m.html)
documents that writei can return fewer frames than requested; success is not an
implicit full-period transfer.

ENODEV, ENOTTY and EBADFD mark the device failed, as do failed recovery and an
impossible accepted count larger than the request. A write interrupted by a stop
request does not itself mark device loss. The device loop clears running on exit.

The frame counter counts positive frames accepted by writei, including submitted
silence. It is not a physical playback clock; recovery can discard data already
accepted by the PCM. The private underrun counter counts short software-ring
reads and EPIPE/ESTRPIPE recovery events. EINTR/EAGAIN alone are not counted as
underruns. No portable public underrun-query API is introduced by this backend.

## Lifecycle and recovery

Repeated start while running returns success without creating duplicate workers.
Prepare failure returns an error. A partial thread-start failure clears running,
drops the prepared PCM and joins any workers before returning failure. Stop also
joins owned workers when running was already cleared by a device failure.

Stop requests a PCM drop before joining, so a blocked writer can be interrupted.
The result of drop/join and the behavior of individual drivers/plugins still
need native validation; this does not establish a bounded shutdown guarantee.
Close stops first, then closes the PCM and releases ring/conversion storage.

Wait uses an absolute accepted-frame target. Below that target it fails when
stopped or marked lost, but running write/recovery operations can still block or
retry indefinitely. Failed devices require stop/close and reopen. A normal stop
retains software-ring contents and counters; preparing again can reuse queued
samples. No automatic device/server reconnection is provided.

## Source and verification

Select sharp_alsa.c (negotiation/lifecycle), sharp_alsa_stream.c (workers),
sharp_alsa_format.c (conversion), sharp_thread_posix.c, common backend and audio
ring. Link alsa-lib and POSIX threads using the platform's build flags. Exactly
one platform supplies sharp_device_vtable. Do not also compile the original
monolith, combined sharp_thread.c or another platform's device table.

The Linux POSIX feature-test macro is set before headers unless supplied by the
build. [SHARP_MACOS.md](SHARP_MACOS.md) documents the shared POSIX helper's ownership
and interrupted-sleep behavior; its Apple QoS branch is not used on Linux.

This migration was tested on Windows with controlled ALSA declarations and calls,
ordinary/sanitized failure tests, and conversion code extracted from ENTERON.
Those fixtures are internal and do not establish native alsa-lib ABI, Linux
linkage, real blocking/drop/recovery behavior or sound-server compatibility.
Native Linux builds and playback/hotplug/load tests remain pending. No device was
opened during this pass; standalone build instructions are in SHARP.md.
