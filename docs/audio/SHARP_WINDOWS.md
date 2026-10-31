# SHARP on Windows

## WASAPI output

The Windows device backend uses the default console render endpoint in shared,
event-driven mode. It adopts the endpoint mix rate instead of resampling to the
requested rate. Open allocates resources but does not start playback. Start fills
the software ring synchronously through the render callback, starts the client,
then launches a render-ahead worker and a device consumer.

Supported containers are 32-bit IEEE float and 16-bit PCM, including extensible
forms with the corresponding subtype. Zero rate/channel count, mismatched block
alignment, short extensible headers and unsupported bit widths are rejected.
Nonzero extensible valid-bit counts must match the container; packed/reduced-bit
PCM is not converted. These restrictions describe the current converter, not all
formats supported by Windows. See Microsoft's [WAVEFORMATEXTENSIBLE description](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ksmedia/ns-ksmedia-waveformatextensible).

Stereo maps to the first two device channels; additional channels are zero.
A mono endpoint receives the left sample only. This preserves the current mapping;
it is not a stereo downmix or a channel-mask-aware surround renderer. Float samples
are copied unchanged. PCM16 clips to -1..1, rounds with the retained sign-dependent
half-step and scales by 32767. NaN becomes zero; infinities saturate.

The software lead defaults to negotiated_rate/50 frames, is raised to at least
twice the device buffer size and one 256-frame render block, then rounds up to the ring's power-of-two
capacity. Ring bounds can reject extreme lead requests. SHARP_WASAPI_DEVICE_MS
defaults to zero and supplies the requested shared buffer duration in milliseconds.
It is a build-time tuning option, not a latency guarantee or machine requirement.
Actual buffering depends on the endpoint and Windows audio engine.

## Stream flow and counters

The render worker produces stereo float blocks of 256 frames into the SPSC ring.
It sleeps briefly when fewer than 256 writable frames remain. The device worker
waits for the endpoint event, reads available space from buffer size minus padding,
copies ring frames and pads shortages with silence before conversion/submission.
It does not invoke the engine render callback itself.

The backend frame counter advances only after ReleaseBuffer succeeds, including
silence supplied on an underrun. It counts accepted buffer submissions, not sound
already heard or an independently measured playback position. The private
underrun counter counts short ring reads, not missing frames; it is not a public
portable diagnostics API.

Device invalidation, failed event waits, impossible padding, failed ReleaseBuffer
and device-thread COM initialization failure stop the worker loop and mark the
backend failed. Some other padding/buffer acquisition errors retain retry behavior.
Waiting below the requested frame target fails once stopped or marked lost; a
running endpoint that delivers no events still has no uniformly bounded wait.

## Lifecycle and COM ownership

Call open and close on the same thread, and serialize stream lifecycle. Successful
COM initialization during open is balanced on failure or close, including S_FALSE.
The device thread balances its own successful initialization. The inherited
RPC_E_CHANGED_MODE acceptance during open remains for compatibility and does not
acquire a COM initialization reference. Cross-apartment interface handoff still
needs review; this migration does not establish general STA-host support.
Microsoft documents the balancing rule in [CoInitializeEx](https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-coinitializeex).

Start returns success if already running, preventing duplicate thread creation.
If client start or either thread creation fails, it reports failure; partial
thread creation is stopped/joined and the started client is stopped before return.
A failed device requires stop/close and reopen rather than an in-place restart.
Start does not acknowledge successful device-thread COM initialization or actual
audio delivery before returning.

Stop clears the running flag, signals the event and joins owned threads even if
the device worker already cleared the flag. It then stops a client that was started.
Close stops first and releases interfaces, format storage, event, ring and scratch
storage. Stop/close status remains void, so do not infer successful OS cleanup from
the public return alone. A normal stop retains queued ring data; restarting that
same healthy handle can reuse it. No automatic endpoint recovery is provided.

## Platform threads

sharp_thread.h is a private backend helper interface. The Windows implementation
is sharp_thread_windows.c and uses CreateThread, WaitForSingleObject, CloseHandle
and Sleep. It has no pthread dependency. Start rejects a null function, returns a
handle or null, and borrows its user context until join. Join accepts null and
otherwise waits without a timeout, releases the handle and frees helper storage.
Do not join the same thread twice or join from that thread itself.

Above-normal priority is a scheduling hint; setting it is not a real-time deadline
guarantee. Creation/priority/join diagnostics need further hardening. POSIX thread
support is reviewed with its platform backends, separately from this Windows file.

## Source and build

Build sharp_wasapi.c (lifecycle), sharp_wasapi_stream.c (workers),
sharp_wasapi_format.c (selection/conversion), sharp_thread_windows.c, the common
backend and audio ring, with ole32. Exactly this backend supplies sharp_device_vtable
in a Windows build. Do not also compile the original monolith or old combined
sharp_thread.c. GUID values are local constants, so this path needs no uuid library.
The standalone Makefile selects these sources for PLATFORM=windows. Device playback, unplug,
latency and load testing remain to be performed after application migration.
