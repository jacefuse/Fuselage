# CAKE

## Overview

CAKE provides keyboard, relative mouse and controller input. Its public headers
use standard C types and require no graphics system or platform SDK headers.
cake.h declares the input state and backend operations; cake_help.h adds labels,
state queries and key repeat. Applications supply a native window where the
platform requires one. Fuselage coordinates that attachment when using the full
engine.

## Lifecycle

CAKE_Poll initializes the selected backend on its first call, refreshes input
and publishes transitions. Call it regularly before application input queries.
It returns no status, so a completed call does not certify successful device
discovery or platform initialization. CAKE_PollCount increments at the start of
each call; it counts calls, not independently validated input snapshots.

Keep polling and public state queries on a coordinated application thread.
The public arrays and counter are ordinary C objects, not atomic snapshots.
Internal intake locks do not make arbitrary concurrent reads and lifecycle
operations safe. Reading input does not consume it, so several readers can use
the same published state within that coordination.

Call CAKE_Shutdown before destroying the attached window or its borrowed display
connection. Coordinate it with event delivery and polling. The API returns no
cleanup status. Do not rely on shutdown to clear every public input value or on
shutdown/reinitialization to reset helper history.

CAKE_Silence discards incoming keyboard and mouse input while controller polling
continues. Existing published key/button values can remain until resume; silence
does not itself mean that every key becomes released. CAKE_Resume clears keyboard
and mouse state, their transition latches, motion and wheel values. A physically
held key may need a new observed event before it appears pressed again.

## Window attachment

CAKE_AttachWindow takes a borrowed, platform-specific pointer. It creates no
application window and must not outlive the window/display it uses.

| Platform | Attachment and delivery |
| --- | --- |
| Windows | Optional HWND, recorded for application association. Raw Input uses CAKE's message-only window on the polling thread. |
| macOS | NSWindow pointer, attached on the main thread. A local event monitor receives keyboard/mouse events through the application's event pump; attachment also starts GameController monitoring. |
| Linux | Pointer to the shared FuselageLinuxNativeHandle descriptor. Wayland uses the supplied display and a private event queue; X11 opens CAKE's own connection. |

The Linux descriptor is a small shared platform contract, independent of engine
system state. A standalone host may supply it without linking GDMF. Keep the
borrowed Wayland display alive through CAKE shutdown. Attachment returns no
status; a missing seat, protocol or display can leave some input unavailable.

## Published input

CAKE_Keys contains 256 keyboard states indexed by the CAKE_KEY_* USB HID
keyboard/keypad usage codes. These identify keys rather than translated text;
the label helpers are not keyboard-layout translation or text entry APIs.
Treat exported arrays, deltas and counters as read-only application views.

CAKE_IsKeyPressed reads the published key state directly. It does not query the
hardware or pump events. CAKE_WasKeyPressed and CAKE_WasKeyReleased read edges
published by the latest poll. Observed keyboard and mouse edges are latched
during intake, so a press and release can both be true for one poll even when
the final state is released. The next poll replaces the edge arrays; repeated
queries do not clear them.

Controller button edges are currently derived from successive published states.
A complete press/release between those states can be missed, including when a
backend receives controller reports as events. Polling more often can improve
sampling but does not guarantee delivery of every physical transition.

The modifier helpers CAKE_IsShiftPressed, CAKE_IsControlPressed and
CAKE_IsAltPressed accept either left or right key. Key queries take uint8_t, so
their whole input range indexes the 256-entry table. Mouse/controller queries
reject out-of-range indices. Button masks in controller queries match any
selected bit; a zero mask matches none.

## Mouse input

Mouse button indices are left, right, middle, X1 and X2, numbered 0..4.
CAKE_MouseDeltaX and CAKE_MouseDeltaY describe relative movement; CAKE_MouseWheel
describes the poll's scroll value. These are not absolute window or canvas
coordinates, and they must not be used as such without application conversion.
Platform scaling and precise-scroll conversion can differ. macOS and Linux carry
fractional motion/scroll remainders into later polls rather than discarding them.

Mouse state and edge helpers follow the keyboard rules. Invalid button indices
return false. CAKE_GetMouseButtonName returns UNKNOWN for an invalid index.

## Controllers

There are four controller slots, numbered 0..3. CAKE_ControllerState stores a
16-bit CAKE_BUTTON_* mask, two unsigned triggers in 0..255 and four signed stick
components in -32768..32767. Raw state has no application deadzone. Existing field
names, widths and order are retained for consumers that mirror the record.

The button mask follows the existing XInput-compatible layout. It is an engine
mapping, not a guarantee that a printed face-button label or physical position
matches across every controller family. In particular, current Sony HID mapping
follows the existing Linux mapping: Triangle maps to X and Square to Y. Controller
mapping changes must be coordinated across backends.

CAKE_BACKEND_XINPUT identifies the Windows XInput path. CAKE_BACKEND_HID is the
existing umbrella code for Windows Raw Input HID, Linux evdev and macOS
GameController; it does not promise that these use the same OS interface.

| Connection code | Meaning |
| --- | --- |
| CAKE_CONN_EMPTY | No current slot assignment, or invalid slot query |
| CAKE_CONN_ATTACHED | Controller attached or returned within its grace period |
| CAKE_CONN_LOST | Controller lost; its slot is reserved during the grace period |
| CAKE_CONN_TIMEDOUT | The grace period expired; the slot can be reused |

The timeout constant is 10000 milliseconds; changes become visible when the
backend services them. A lifecycle code persists until changed or the slot is
reused. Do not assume CAKE_IsControllerConnected has identical LOST-state behavior
on every backend: Windows currently checks retained identity, while macOS/Linux
check device presence. Consult the connection-state getter when distinguishing
attachment from a reserved slot.

## Controller queries

| Query | Unavailable or invalid result |
| --- | --- |
| GetControllerState, GetControllerName | NULL |
| GetControllerBackend | CAKE_BACKEND_UNKNOWN |
| GetControllerVendorID, GetControllerProductID | 0; also always 0 on macOS |
| GetControllerXInputIndex | -1; otherwise a Windows XInput index in 0..3 |
| GetControllerConnectionState | CAKE_CONN_EMPTY for invalid slot |
| GetControllerButtons, GetControllerAxis, GetControllerTrigger | 0 when no state is available |

Pointers refer to CAKE-owned storage. Use them before the next poll or lifecycle
operation; copy values that must be retained. Neutral input can also contain
zero, so a scalar zero is not a unique error code. An invalid axis selector
returns zero. The current trigger helper selects right only for
CAKE_TRIGGER_RIGHT and otherwise selects left, including an invalid selector.

CAKE_GetKeyName, CAKE_GetMouseButtonName and CAKE_GetControllerButtonName return
non-NULL labels or UNKNOWN. The controller label query expects a single named
button; a combination of bits returns UNKNOWN.

CAKE_SetControllerVibration takes low-frequency/left and high-frequency/right
motor speeds in 0..65535. It returns false for unavailable, unsupported or failed
operations. Windows supports XInput vibration; Linux uses evdev FF_RUMBLE when
available. Windows HID and macOS currently return false. Success reports the
backend operation, not a physical confirmation that a motor moved.

## Key repeat

CAKE_CheckKeyRepeat returns true for the latest press edge, then according to a
held-key delay and interval. Both arguments are poll counts, not milliseconds.
Zero interval reports true on each queried poll once the delay is reached.
Repeated queries in the same poll return the same answer and do not consume it.

For the existing helper, query repeat at least once each poll while using it.
The shared held-key counters advance once when any repeat query sees a new
CAKE_PollCount; skipped polls are not reconstructed. A delay of 30 and interval
of 2 approximate a half-second delay and 30 repeats per second only when polls
and repeat queries run regularly at 60 Hz. No DICE dependency is required.

## Backend state publication

The shared state implementation owns the exported arrays, motion values and
poll counter. Pending keyboard/mouse edges and previous controller button masks
remain private. Platform intake compares each event with its current or pending
state before latching an edge; repeated down events for an already held key must
not create new press edges. Latching records an edge without changing held state.

Once per poll, the backend publishes accumulated keyboard/mouse edges and clears
the pending latches. A second publication with no new events produces empty edge
arrays. When intake and publication run on different threads, both must use the
same backend lock. Shared publication adds no locks and does not pump events.

After refreshing controller state, the backend derives button edges from the
previous masks. A NULL controller state is treated as all buttons released, so
an observed disconnection can publish release edges. This depends on the backend
reporting unavailable state consistently with its connection lifecycle.

Resume's shared reset clears keyboard/mouse held state, published edges, pending
latches and motion/wheel values. Each backend also clears its own pending input.
The shared reset leaves controllers, poll count and repeat history untouched.
It is a resume operation, not a complete system shutdown/reset contract.

## Windows input

The first successful Windows poll creates CAKE's message-only input window.
Subsequent polls and shutdown must run on that window's owning thread; an
incorrect-thread call reports the problem and returns. Shutdown may also run on
another thread after the owning thread has exited, as in the root's teardown,
which joins the polling thread first. The system has then destroyed the window and
its discovery timer; shutdown releases the device notification and controller
state that remain. Window creation failure
also reports the problem and returns without draining another window's messages.
PollCount still counts the attempted poll. Registering Raw Input can fail without
a public status return; the backend reports that failure, but input availability
is not guaranteed by a completed poll.

Keyboard and mouse events are handled on the polling thread. Scan codes map to
the existing USB HID key table; out-of-range codes are ignored. Windows' deferred
left-Shift handling filters synthetic Shift sequences around extended keys.
Committed Shift changes and observed Pause changes publish edges just like other
keys. Resume also discards a deferred Shift value so it cannot restore cleared
input on the next poll.

Relative motion is accumulated for the poll; absolute-motion packets do not
contribute relative deltas. Windows wheel values retain the signed raw wheel
units rather than a count of application scroll actions. Motion and wheel sums
clamp at the int32 limits if an extreme batch exceeds the public representation.

Raw Input packets use the existing 4096-byte limit. Headers, fixed keyboard/mouse
payloads and HID report count/size bounds are checked before dispatch. Invalid,
truncated or oversized packets are ignored. Silence suppresses keyboard/mouse
dispatch while leaving controller reports active.

Device-change notifications trigger controller discovery; a two-second timer
also requests scans. These scans remain part of the polling cost. Failed device
notification removal or input-window destruction reports a cleanup failure and
retains the remaining owner for a shutdown retry on the owning thread. Shutdown
still returns no status; this is not a guarantee that every platform resource
operation or registration failure is recoverable.

## Windows controllers

Discovery keeps four controller slots and a separate cache of interface verdicts.
An already assigned interface is recognized before probing. Interfaces previously
classified as non-controllers are skipped until the cache is invalidated; pads
that found no free slot are reconsidered after a slot becomes free. The cache
holds 128 interfaces. Excess entries can be probed again, so it is not an
unconditional bound on discovery cost. Temporary probe failures can also delay
recognition until discovery is refreshed.

XInput is loaded once from System32, trying xinput1_4.dll, xinput1_3.dll and
xinput9_1_0.dll in that order. A candidate must provide state, vibration and
capability functions; battery information is optional. A missing DLL disables
that input path without preventing CAKE from loading. An identified XInput slot
without an assigned player index has neutral state and cannot vibrate. Failed
state reads publish neutral input. The selected module remains loaded for the
process lifetime, including across CAKE shutdown/reinitialization.

Other controllers use Raw Input HID reports. CAKE owns a copy of each bound
device's preparsed descriptor. A returning device can have a different raw handle;
binding compares its interface path without ASCII case sensitivity and can bind
the current handle when its first report arrives. Unknown handles are cached
until device or slot changes require another lookup.

HID descriptors identify axes, ranges and numbered buttons. The current family
tables map generic, Sony, Nintendo and NEOGEO X buttons to CAKE's existing mask.
These mappings retain the cross-platform policy described above. Axis and trigger
normalization clamps output to the public integer ranges; range calculations use
wide intermediates. Missing fields in a report leave their previous values
unchanged. Normalization does not add a deadzone.

Nintendo full reports 0x30, 0x21 and 0x31 use the existing twelve-byte button/stick
block instead of descriptor-based decoding. Stick centering uses a guarded first
sample and a nominal range, rather than factory calibration data. The USB startup
path sends the existing three output reports through a temporary write handle.
Those writes do not establish support for every Nintendo-family controller or
verify that the device accepted the startup sequence.

Device names currently retain printable ASCII and replace other characters with
question marks. Discovery, descriptor queries and startup writes still need
platform/device-specific latency and failure testing; they must not be treated
as guaranteed nonblocking operations.

## macOS input and controllers

Attach an NSWindow on the main thread before input is needed. The AppKit bridge
observes that window's events and returns them unchanged to normal dispatch.
Events without a window retain the existing keyboard-global handling. The
window's acceptsMouseMovedEvents setting is saved on attachment and restored on
detach, including a failed monitor installation. One window can be attached at
a time; attempting to replace an active attachment reports failure.

The C intake stores pending keys, mouse buttons and fractional motion under a
mutex. Poll copies held state, publishes edges and drains whole motion/wheel
units, retaining ordinary fractional remainders. Modifier and Caps Lock changes
now latch edges too. Caps Lock represents the lock's toggle state on macOS,
which differs from the physical-key press state supplied by Windows.

Non-finite motion/scroll samples are ignored. Values beyond the int32 output
range are clamped for that poll and their excess is discarded. Resume clears
pending and published keyboard/mouse input under the intake mutex. Silence is
checked under that same mutex.

GameController supplies the extended-gamepad profile and its existing button
mapping. Sticks retain the existing scaling: -1 maps to -32767 and +1 to 32767;
out-of-range finite values clamp to the signed limits. Trigger values clamp to
0..255, and non-finite controller values become zero. A failed framework state
read publishes neutral input rather than retaining the last pressed state.

The framework bridge retains only controllers accepted into a slot. A pad
rejected while all four slots are occupied can be reconsidered by the existing
two-second reconciliation timer. Return matching still uses the device name
during the lost-slot grace period; names are not unique device identifiers.

Shutdown must run on the main thread after polling and application readers stop.
It also cleans an attachment made before the first poll. C controller pointers
are invalidated before bridge references and observers are released, and late
connections are refused until monitoring is enabled again. Public keyboard/mouse
history is not globally reset by shutdown.

Notification observers retain their synchronous posting-thread delivery policy;
framework callback ordering, object lifetime and public-query concurrency still
require native validation. The intake mutex does not make every public array or
borrowed controller pointer safe for arbitrary concurrent use.

## Linux input

Pass a FuselageLinuxNativeHandle from the independent BASE/fuselage_native.h contract
to AttachWindow. Its backend_name selects Wayland or X11, and display_connection
identifies the existing display. The window and display owner must outlive CAKE's
attachment. Attach before polling starts, and shut down after polling stops.
Attachment failure leaves keyboard/mouse unavailable; controller polling does not
require a display connection.

Wayland borrows the supplied wl_display and binds its own seat on a private event
queue. The registry is created through a queue-assigned proxy wrapper so its
children inherit that queue immediately. Attachment uses two queue roundtrips;
polling dispatches pending events only. The application still needs a display
reader to deliver incoming events to the queue. CAKE never disconnects the
borrowed display.

Keyboard input uses physical Linux keycodes mapped to USB HID usages, ignoring
the text keymap and compositor repeat settings. While intake is active, focus
entry replaces held keys with the compositor's supplied list and focus exit
clears them, latching changes. Removing keyboard capability does the same; removing
pointer capability clears held buttons and latches releases. Ordinary pointer
leave preserves buttons because Wayland's implicit grab supplies their releases.

Mouse motion uses unaccelerated relative-pointer events. Without that protocol,
mouse buttons and scrolling remain available but relative motion does not.
Absolute surface coordinates are ignored. Vertical wheel steps use 120 units per
notch, positive away from the user. Continuous scrolling uses eight units per
Wayland axis unit. The discrete event suppresses its paired continuous event
within the pointer frame. Horizontal scrolling is not mapped.

X11 opens and owns a separate connection to the server named by the supplied
Display. It requests XInput 2.3 and accepts a negotiated version of at least 2.0.
Raw events are selected on the root window for all master devices; they are not
restricted to the attached application's focus. The host must manage Silence
and Resume according to its focus policy. Raw valuators zero and one supply
motion; higher valuators are ignored. Buttons four and five supply wheel steps
on press only. Polling drains the connection's event queue and can perform Xlib
socket work; it is not a strict latency guarantee.

Both paths share pending state and an intake mutex. Silence checks, resume resets,
edge publication and fractional motion draining use that mutex. Non-finite motion
is ignored; output beyond int32 limits is clamped for one poll and excess is
discarded. Ordinary fractions carry into later polls. This mutex does not make
public arrays safe for arbitrary concurrent readers.

Shutdown also releases an attachment made before the first poll. A Wayland
dispatch failure releases CAKE's protocol objects and queue; shutdown is required
before attempting another attachment. Native protocol errors, display failure
recovery and display-owner coordination still require platform validation.

## Linux controllers

Controllers use evdev independently of Wayland or X11. Discovery scans /dev/input
at initialization, after hotplug notifications and on a two-second fallback.
The watch includes permission changes so a newly accessible device can be retried.
Sysfs key capabilities identify candidates using BTN_GAMEPAD; an ioctl confirms
that capability after opening the device. This is CAKE's selection rule, rather
than a guarantee that every possible controller exposes that button cluster.

Rejected nodes and pads waiting for a free slot have separate cached verdicts.
Attached paths are skipped. Hotplug activity clears the cache, and a freed slot
makes waiting pads eligible again. The cache holds 128 node verdicts; additional
nodes are reclassified on later scans. Failed sysfs reads, device opens and
capability queries remain eligible for retry. Directory scans and device queries
still incur filesystem and driver work; the cache is not a bounded-latency promise.

CAKE retains an adopted device descriptor until disconnect or shutdown. It tries
read/write access for force feedback, then read-only access for input alone.
Polling drains complete events into running state and publishes a controller
buffer. EAGAIN and EWOULDBLOCK end the drain normally; EINTR defers the remaining
read to a later poll. Other errors, EOF or an unexpected short event read mark
the device lost and close its descriptor.

A lost slot remains reserved for a same-name return until its grace period
expires. After timeout it can accept another controller. Names are not unique
device identifiers. Connected queries on Linux require a present device; a lost
slot exposes its connection status but returns no name or input-state pointer.
This differs from Windows' retained-identity queries during the lost period.

Axes use the device's reported minimum and maximum, with wide arithmetic and
clamped output. Y axes are inverted; no deadzone is added. ABS_Z and ABS_RZ supply
triggers, while digital trigger buttons supply zero or full scale. Hat axes and
directional buttons both modify the same directional mask. The existing button
policy maps BTN_NORTH to X and BTN_WEST to Y; this is not a universal guarantee
about the printed labels or positions on every controller.

Vibration requires write access and FF_RUMBLE support. CAKE uploads one effect,
reuses its identifier on later changes, and starts or stops it according to the
requested motor strengths. The effect requests the existing zero replay length
for indefinite playback. The function returns false on upload failure or an
incomplete play/stop write; success reports those operations, not observed motor
movement. Driver support still needs validation.

Initial held buttons and current axis positions are not fully reconstructed at
adoption; event delivery supplies the running state. SYN_DROPPED recovery is also
unfinished, so an evdev queue overflow can leave stale state until later events.
Applications must serialize polling, vibration, teardown and access to borrowed
controller buffers. Double buffering and a volatile buffer index do not provide
general thread safety.

## Building CAKE

The Makefile in fuselage/source/CAKE builds libcake.a independently of other engine
systems. It requires GNU Make, a C11 compiler with GCC/Clang command-line options
and an archiver. Clang is the default. Windows builds use a MinGW-compatible
toolchain and mingw32-make; the recipes use cmd.exe.

From the repository root:

```sh
make -C fuselage/source/CAKE
make -C fuselage/source/CAKE print-libs
```

The default output directory is fuselage/build/CAKE followed by windows, linux or
macos, selected from the compiler target. PLATFORM can override that selection
when using a matching cross compiler and SDK. CC, AR, CPPFLAGS, CFLAGS and BUILD_DIR
are configurable. For example, from the CAKE directory:

```sh
make CC=clang AR=llvm-ar BUILD_DIR=out CFLAGS="-O2 -g"
```

Changing compiler, flags or platform in an existing output directory requires a
clean rebuild. The clean target removes only the selected build's objects,
dependency files, archive and generated protocol files. It retains the directory
and unrelated files. Header dependencies are tracked automatically.

Applications include cake.h or cake_help.h using the CAKE source directory as an
include path. Link libcake.a followed by the options from print-libs. An archive
does not itself link its dependencies:

| Platform | Final link dependencies |
| --- | --- |
| Windows | user32, hid and setupapi |
| macOS | Cocoa and GameController frameworks, pthread |
| Linux | Wayland client, X11, Xi, pthread and libm |

Windows XInput is loaded dynamically; no XInput import library is required.
The CAKE Windows sources require no pthread library. Other linked systems and
compiler options can introduce their own dependencies.

macOS builds select three platform C files and two Objective-C files. OBJC defaults
to CC and can be overridden; OBJCFLAGS controls Objective-C flags. The Makefile
adds ARC to both framework bridges. Build with Apple's SDK and a compiler that
supports ARC.

Linux needs the Wayland client, X11 and Xi development packages, wayland-protocols,
wayland-scanner and pkg-config. The Makefile queries installed package paths and
generates the relative-pointer header and private protocol C file into BUILD_DIR.
No protocol implementation is borrowed from GDMF. The independent sibling header
fuselage/source/BASE/fuselage_native.h supplies the native-window descriptor; retain
that relative location when copying the source tree.

PKG_CONFIG, LINUX_CFLAGS, LINUX_LIBS, WAYLAND_PROTOCOLS_DIR, WAYLAND_SCANNER and
RELATIVE_POINTER_XML can be overridden for a toolchain or sysroot. A standalone
application may use CAKE with its own window and display pump; building the library
does not supply those application services.

The four common C files and only the selected platform files enter the archive.
Public and private headers, all platform implementations and the Makefile belong
in the source package. Tests, tracking notes, generated outputs, Python and CMake
are not required library inputs.
