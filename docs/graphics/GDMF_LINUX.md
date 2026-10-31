# GDMF on Linux

## Backend selection

One Linux executable supports both Wayland and X11. Window creation tries
Wayland first and falls back to X11 if Wayland creation fails. The first
successful backend remains selected until window destruction. The dispatcher
does not switch display servers while the window is running.

Set `FUSELAGE_BACKEND=wayland` or `FUSELAGE_BACKEND=x11` to request only that
backend. If the requested backend fails, window creation fails without trying
the other one. Values are case-sensitive. An unrecognized value, including an
empty string, produces a diagnostic and uses the normal fallback order.

A backend's creation function performs the probe and must release partial
resources on failure. The dispatcher returns `0` for successful creation and
`-1` when none of its candidates succeeds.

## Dispatch and ownership

The dispatcher forwards window operations to the selected backend. With no
backend selected, event pumping and setting requests do nothing, mouse queries
return false without supplying coordinates, and the native-handle query returns
NULL. Destruction releases the selected backend and clears the selection.

The native handle points to a backend-owned descriptor identifying Wayland or
X11. Consumers borrow that descriptor and must not retain it after destruction.
The Vulkan surface dispatcher uses the selected backend's name to choose its
matching surface implementation.

Monotonic timing is supplied by the dispatcher independently of display-server
selection. Thread-safety, failed-probe cleanup, and repeated lifecycle behavior
still require verification in the backend implementations.

## Native window descriptor

`BASE/fuselage_native.h` defines the shared `FuselageLinuxNativeHandle` structure.
GDMF fills it and passes it through the opaque native-window interface. CAKE
reads it without depending on GDMF's implementation headers.

| Field | Wayland | X11 |
|---|---|---|
| `backend_name` | Static `"wayland"` string | Static `"x11"` string |
| `display_connection` | `wl_display*` | Xlib `Display*` |
| `window_id` | NULL | X11 `Window` identifier converted through `uintptr_t` |

The X11 identifier is an integer carried in a pointer-sized field; it is not
an address to dereference. The descriptor owns none of these resources. Its
storage and display connection belong to the active window backend, and users
must stop using them before that backend is destroyed. A descriptor pointer
must not be reused as a handle for a later window lifetime.

## Wayland event ownership

Window creation connects to the compositor, binds registry objects, and waits
for the first surface configure before starting the event reader. The reader
then owns dispatch of the default event queue. The per-frame pump flushes
outgoing requests and checks connection health; it does not dispatch that queue.
A stop pipe wakes the reader for shutdown, and teardown joins it before
destroying the protocol objects used by its callbacks.

Toplevel configure events stage dimensions and state flags. The surface
configure callback acknowledges the cycle and publishes the resulting window
state. Non-fullscreen dimensions are fitted to the configured aspect ratio;
fullscreen dimensions are left to the compositor. Both fullscreen API modes
request the same compositor fullscreen state, retaining the requested label
when confirmation arrives.

GDMF uses keyboard events for focus and pointer events for position and cursor
appearance. Application input belongs to CAKE, which uses its own event queue
on the shared connection. GDMF's confinement mutex protects some input-object
updates; it does not establish thread safety for the entire backend.

## Wayland behavior and limitations

- Mouse queries return the last known surface-local position. Leaving the
  surface does not supply an updated global cursor position. Before the first
  pointer event, the stored position is zero.
- Coordinates and window dimensions currently assume buffer scale one.
  Fractional scaling, high-density displays, and output changes need verification.
- Where supported, the toplevel's suspended flag is used as the minimized
  indicator. Suspension does not establish that the user minimized the window;
  without that protocol state, this backend does not report minimization.
- Confinement requires the pointer-constraints protocol. Cursor appearance also
  depends on a usable cursor theme. Requested settings are not proof that the
  compositor has applied the requested effect.
- This implementation does not set window icons. It requests server-side
  decorations when the decoration manager is available and draws no client-side
  decorations itself.
- Idle inhibition is requested when the compositor advertises the protocol.
  Its absence is reported, and controller activity may not prevent display sleep.

Creation failure handling and repeated lifecycles remain under review. In
particular, the current initial-configure loop can exit on a dispatch error
without making creation fail immediately. General safety guarantees must wait
for verification of those error paths and shared-state synchronization.

## X11 event ownership

The X11 backend opens its own display connection and drains events from the
main-thread pump. It has no event-reader thread. The backend calls
`XInitThreads()` before opening its connection because setting requests can
also make Xlib calls from another thread. This does not by itself synchronize
the engine's shared state or protect teardown.

CAKE opens a separate connection for input. GDMF handles window geometry,
focus, minimization properties, close requests, and fullscreen property changes.
Creation waits for the window to be mapped before returning.

## X11 display and input behavior

Windowed mode restores saved client geometry. Borderless mode requests
window-manager fullscreen using `_NET_WM_STATE_FULLSCREEN`. The exclusive
path also uses XRandR, but currently reapplies the selected display controller's
existing mode rather than choosing a different resolution or refresh rate.
Leaving that path or destroying the window attempts to restore the saved mode.

Fullscreen property notifications supply the window manager's reported state.
Switches between the two fullscreen labels may update the engine's mode after
an Xlib round trip without a property change. That round trip is not proof of
window-manager approval or successful exclusive presentation.

Capture uses a pointer grab confined to the window. If the grab is refused,
the backend retries on a later input-state update rather than every frame.
Cursor hiding selects a transparent cursor. Mouse position is queried relative
to the window, but the current implementation does not check `XQueryPointer`'s
result before returning its coordinates.

The backend supplies a UTF-8 title and window icon property, requests minimum
size and aspect hints, and applies corrective resizing for window managers that
do not enforce the aspect hint. Minimized state is inferred from mapping events
and the window manager's iconic-state property.

The current backend does not provide graceful recovery from loss of the X
server. Error handling, extension availability, display-mode confirmation,
shutdown synchronization, and blocking creation waits still need hardening.
