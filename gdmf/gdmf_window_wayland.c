// GDMF -- Linux/Wayland window backend.
// The OS window itself: the wl_display connection, the xdg_toplevel and its
// configure/ack dance, keyboard-focus and pointer tracking for the shared
// state, cursor capture/visibility, aspect ratio enforcement, and idle
// inhibition. Platform-neutral state and the public API live in gdmf.c; the
// interface between the two is gdmf_window.h.
//
// Threading: the Win32 pattern after all, not the macOS one -- a dedicated
// EVENT thread, for a reason Wayland shares with no other platform: a
// compositor DISCONNECTS a client that stops reading its socket while
// events pour in (a high-polling mouse during one long engine tick is
// enough -- weston logs "error in client communication" and executes the
// client). Win32 window messages and macOS event queues just wait; Wayland
// kills. So the window is created inside gdmf_window_create() (main
// thread), and then a reader thread drains and dispatches the default
// queue continuously, keeping the engine responsive to the compositor
// (pings, configures, floods) no matter how long a tick runs.
// gdmf_window_pump() only flushes outgoing requests and watches for a dead
// connection. CAKE rides the same connection through its own
// wl_event_queue dispatched from CAKE_Poll -- the capture-on-poll-thread
// pattern (see the ratified contract in cake.h).
//
// Display modes: Wayland has no client-side mode setting -- fullscreen is
// xdg_toplevel_set_fullscreen and the compositor owns the actual output
// mode. BORDERLESS and FULLSCREEN_EXCLUSIVE therefore both map to
// set_fullscreen; the mode the caller asked for is what gdmf_st_displayMode
// reports (the distinction the two names promise -- covering the monitor --
// is delivered either way; changing the output's resolution is not a thing
// a Wayland client can do, by design). Win32's DEVMODE machinery has no
// counterpart here and none is needed: the renderer letterboxes to the
// canvas regardless.
//
// Aspect ratio: a Wayland compositor doesn't negotiate a size, it hands
// one over -- a tiling compositor most bluntly of all. What the client
// controls is what it commits, so the correction below runs on every
// configure and the committed size is always aspect-correct. Confirmed
// against sway, which tiled the window to 956 wide: the backend committed
// 956x537, the shrink-to-fit result exactly.
//
// Known parity gaps, all deliberate for now:
//   * Window icon: no stable Wayland protocol reaches weston/GNOME/KDE
//     uniformly (xdg-toplevel-icon-v1 is not yet everywhere); the icon
//     config is accepted and ignored. Revisit when the protocol lands
//     broadly.
//   * gdmf_st_minimized: xdg-shell only reports the 'suspended' state from
//     protocol v6 -- where offered it's used, otherwise minimize simply
//     never reads back as true (rendering continues; harmless).
//   * gdmf_window_get_mouse_client returns the last position the pointer
//     had while over the surface -- Wayland tells a client nothing about
//     the cursor once it leaves.

// -std=c11 is strict ISO; clock_gettime/CLOCK_MONOTONIC and poll() are
// POSIX and must be asked for by name before any header is pulled in.
#define _POSIX_C_SOURCE 200809L

#include "gdmf.h"
#include "gdmf_window.h"
#include "gdmf_window_linux.h"
#include "../fuselage_native.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <poll.h>
#include <unistd.h>
#include <pthread.h>

#include <wayland-client.h>
#include <wayland-cursor.h>
#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "pointer-constraints-unstable-v1-client-protocol.h"
#include "idle-inhibit-unstable-v1-client-protocol.h"

// --- Connection and globals (bound from the registry) ------------------------

static struct wl_display*    g_display    = NULL;
static struct wl_registry*   g_registry   = NULL;
static struct wl_compositor* g_compositor = NULL;
static struct xdg_wm_base*   g_wmBase     = NULL;
static struct wl_seat*       g_seat       = NULL;
static struct wl_shm*        g_shm        = NULL;

static struct zxdg_decoration_manager_v1*  g_decorationManager = NULL;
static struct zwp_pointer_constraints_v1*  g_pointerConstraints = NULL;
static struct zwp_idle_inhibit_manager_v1* g_idleInhibitManager = NULL;

// --- The window --------------------------------------------------------------

static struct wl_surface*    g_surface    = NULL;
static struct xdg_surface*   g_xdgSurface = NULL;
static struct xdg_toplevel*  g_toplevel   = NULL;

static struct zxdg_toplevel_decoration_v1* g_decoration    = NULL;
static struct zwp_idle_inhibitor_v1*       g_idleInhibitor = NULL;

static bool g_initialConfigureSeen = false;

// Pending state from the last xdg_surface.configure cycle: xdg_toplevel
// events arrive first and stage here; xdg_surface.configure is the commit
// point where they become real (that's the protocol's atomicity rule).
static int  g_pendingWidth      = 0;      // 0 = compositor leaves it to us
static int  g_pendingHeight     = 0;
static bool g_pendingResizing   = false;  // interactive resize drag in progress
static bool g_pendingFullscreen = false;
static bool g_pendingSuspended  = false;

// Windowed geometry, saved when leaving WINDOWED so coming back restores it
// (parity with the Win32 backend's g_savedWindowRect).
static int g_savedFloatingW = 0;
static int g_savedFloatingH = 0;

static bool g_fullscreen = false;   // what the compositor last confirmed
// The mode the caller asked for -- BORDERLESS and FULLSCREEN_EXCLUSIVE both
// arrive at the same compositor state (see the header comment), so the
// requested mode is what the confirmed fullscreen state reports back as.
static GDMF_DisplayMode g_requestedMode = GDMF_MODE_WINDOWED;

// --- Input-adjacent state (seat, cursor, capture) ----------------------------

static struct wl_keyboard* g_keyboard = NULL;
static struct wl_pointer*  g_pointer  = NULL;

static struct wl_cursor_theme* g_cursorTheme   = NULL;
static struct wl_surface*      g_cursorSurface = NULL;
static struct wl_cursor*       g_cursorArrow   = NULL;

static uint32_t g_pointerEnterSerial = 0;     // latest enter, for set_cursor
static bool     g_pointerInside      = false;
static int      g_mouseX             = 0;     // last known surface-local px
static int      g_mouseY             = 0;

static struct zwp_confined_pointer_v1* g_confinedPointer = NULL;

// Serializes gdmf_wl_apply_input_state: it runs from the reader thread
// (focus/enter events) and from whatever thread calls the public
// capture/visibility setters, and it holds create-or-destroy state
// (g_confinedPointer) that must not race.
static pthread_mutex_t g_inputStateLock = PTHREAD_MUTEX_INITIALIZER;

// --- The event reader thread -------------------------------------------------
// Owns all dispatching of the default queue. Started once the window
// exists; stopped (via the self-pipe) in gdmf_window_destroy. On a dead
// connection it flags closeRequested and exits -- the same disposition the
// pump applies, whichever notices first.

static pthread_t g_readerThread;
static bool      g_readerRunning = false;
static int       g_readerStopPipe[2] = { -1, -1 };

static void gdmf_wl_apply_input_state(void);
static void gdmf_window_wl_destroy(void);   // create()'s failure paths use it

// --- wl_registry -------------------------------------------------------------

static void registry_global(void* data, struct wl_registry* registry,
                            uint32_t name, const char* interface, uint32_t version) {
    (void)data;

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        g_compositor = wl_registry_bind(registry, name, &wl_compositor_interface,
                                        version < 4 ? version : 4);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        // Take up to v6 for the 'suspended' toplevel state; anything older
        // still gives everything else.
        g_wmBase = wl_registry_bind(registry, name, &xdg_wm_base_interface,
                                    version < 6 ? version : 6);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        g_seat = wl_registry_bind(registry, name, &wl_seat_interface,
                                  version < 5 ? version : 5);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        g_shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0) {
        g_decorationManager = wl_registry_bind(registry, name,
                                               &zxdg_decoration_manager_v1_interface, 1);
    } else if (strcmp(interface, zwp_pointer_constraints_v1_interface.name) == 0) {
        g_pointerConstraints = wl_registry_bind(registry, name,
                                                &zwp_pointer_constraints_v1_interface, 1);
    } else if (strcmp(interface, zwp_idle_inhibit_manager_v1_interface.name) == 0) {
        g_idleInhibitManager = wl_registry_bind(registry, name,
                                                &zwp_idle_inhibit_manager_v1_interface, 1);
    }

    return;
}

static void registry_global_remove(void* data, struct wl_registry* registry, uint32_t name) {
    (void)data; (void)registry; (void)name;
    // Globals GDMF binds (compositor, wm_base, seat) don't come and go on
    // any compositor worth running under; ignore.
    return;
}

static const struct wl_registry_listener g_registryListener = {
    .global        = registry_global,
    .global_remove = registry_global_remove,
};

// --- xdg_wm_base -------------------------------------------------------------

static void wm_base_ping(void* data, struct xdg_wm_base* wmBase, uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(wmBase, serial);

    return;
}

static const struct xdg_wm_base_listener g_wmBaseListener = {
    .ping = wm_base_ping,
};

// --- Aspect ratio enforcement ------------------------------------------------

// Same correction as the Win32 backend's gdmf_compute_aspect_corrected_size:
// anchors on whichever dimension is already smaller relative to the target
// ratio and shrinks the other to match, so the corrected size never exceeds
// what the compositor proposed (a compositor proposal is a boundary -- tiled
// or snapped windows must not grow past it). Wayland's configure carries no
// "which edge is being dragged" information, so the Win32 WM_SIZING
// edge-anchored variant has no counterpart here; shrink-to-fit covers both.
static void gdmf_wl_aspect_corrected(int* w, int* h) {
    if (*w <= 0 || *h <= 0) { return; }

    int heightForWidth = (*w * gdmf_cfg_aspectDen) / gdmf_cfg_aspectNum;
    if (heightForWidth <= *h) { *h = heightForWidth; }
    else { *w = (*h * gdmf_cfg_aspectNum) / gdmf_cfg_aspectDen; }

    return;
}

// --- xdg_surface / xdg_toplevel ----------------------------------------------

static void toplevel_configure(void* data, struct xdg_toplevel* toplevel,
                               int32_t width, int32_t height, struct wl_array* states) {
    (void)data; (void)toplevel;

    g_pendingWidth      = width;
    g_pendingHeight     = height;
    g_pendingResizing   = false;
    g_pendingFullscreen = false;
    g_pendingSuspended  = false;

    uint32_t* state;
    wl_array_for_each(state, states) {
        switch (*state) {
        case XDG_TOPLEVEL_STATE_RESIZING:   g_pendingResizing   = true; break;
        case XDG_TOPLEVEL_STATE_FULLSCREEN: g_pendingFullscreen = true; break;
#ifdef XDG_TOPLEVEL_STATE_SUSPENDED_SINCE_VERSION
        case XDG_TOPLEVEL_STATE_SUSPENDED:  g_pendingSuspended  = true; break;
#endif
        default: break;
        }
    }

    return;
}

static void toplevel_close(void* data, struct xdg_toplevel* toplevel) {
    (void)data; (void)toplevel;
    gdmf_st_closeRequested = true;

    return;
}

static void toplevel_configure_bounds(void* data, struct xdg_toplevel* toplevel,
                                      int32_t width, int32_t height) {
    (void)data; (void)toplevel; (void)width; (void)height;
    // Advisory maximum only; the aspect correction in xdg_surface_configure
    // already keeps committed sizes inside whatever the compositor proposes.
    return;
}

static void toplevel_wm_capabilities(void* data, struct xdg_toplevel* toplevel,
                                     struct wl_array* capabilities) {
    (void)data; (void)toplevel; (void)capabilities;
    // Which optional requests (minimize, fullscreen, ...) the compositor
    // honors; GDMF sends its requests regardless and unhonored ones are
    // defined no-ops. NOTE: every member of a wl listener must be non-NULL
    // -- libwayland aborts the process on a NULL entry the moment the event
    // fires, which is why these two "ignore" stubs exist at all.
    return;
}

static const struct xdg_toplevel_listener g_toplevelListener = {
    .configure        = toplevel_configure,
    .close            = toplevel_close,
    .configure_bounds = toplevel_configure_bounds,
    .wm_capabilities  = toplevel_wm_capabilities,
};

static void xdg_surface_configure(void* data, struct xdg_surface* xdgSurface, uint32_t serial) {
    (void)data;

    // The commit point: everything the toplevel staged becomes real here.
    xdg_surface_ack_configure(xdgSurface, serial);

    int w = g_pendingWidth;
    int h = g_pendingHeight;

    if (w == 0 || h == 0) {
        // Compositor leaves the size to us: keep what we have (first
        // configure: the configured startup size).
        w = gdmf_st_width;
        h = gdmf_st_height;
    } else if (!g_pendingFullscreen) {
        // Enforce the design aspect ratio on floating sizes -- the parity
        // rule with Win32's WM_SIZING/WM_WINDOWPOSCHANGING pair. Fullscreen
        // sizes are the monitor's and deliberately left alone (letterboxing
        // is the renderer's job, not the window's). During an interactive
        // resize the correction still applies -- the compositor draws its
        // drag feedback wherever it likes, but what we commit is
        // aspect-correct, which is all the window ever shows.
        (void)g_pendingResizing;
        gdmf_wl_aspect_corrected(&w, &h);
    }

    if (g_fullscreen && !g_pendingFullscreen) {
        // Leaving fullscreen: restore the saved floating size when the
        // compositor didn't dictate one (parity with Win32's saved rect).
        if (g_pendingWidth == 0 && g_savedFloatingW > 0) {
            w = g_savedFloatingW;
            h = g_savedFloatingH;
        }
        g_requestedMode = GDMF_MODE_WINDOWED;
    }
    if (!g_fullscreen && g_pendingFullscreen) {
        // Entering fullscreen: remember the floating size we're leaving.
        g_savedFloatingW = gdmf_st_width;
        g_savedFloatingH = gdmf_st_height;
    }
    g_fullscreen = g_pendingFullscreen;

    // The confirmed mode: fullscreen reports whichever flavor was asked
    // for; not-fullscreen is WINDOWED (see the header comment).
    GDMF_DisplayMode confirmed =
        g_fullscreen ? (g_requestedMode == GDMF_MODE_WINDOWED
                        ? GDMF_MODE_BORDERLESS : g_requestedMode)
                     : GDMF_MODE_WINDOWED;
    if (confirmed != gdmf_st_displayMode) {
        gdmf_st_displayMode = confirmed;
        printf("[GDMF] Display mode: %s\n",
               confirmed == GDMF_MODE_WINDOWED ? "WINDOWED" :
               confirmed == GDMF_MODE_BORDERLESS ? "BORDERLESS" : "FULLSCREEN EXCLUSIVE");
    }

    bool wasMinimized = gdmf_st_minimized;
    gdmf_st_minimized = g_pendingSuspended;

    if (w > 0 && h > 0 && (wasMinimized || w != gdmf_st_width || h != gdmf_st_height)) {
        gdmf_st_width          = w;
        gdmf_st_height         = h;
        gdmf_st_resizeOccurred = true;
    }

    g_initialConfigureSeen = true;

    return;
}

static const struct xdg_surface_listener g_xdgSurfaceListener = {
    .configure = xdg_surface_configure,
};

// --- wl_keyboard: focus only -------------------------------------------------
// GDMF's interest in the keyboard begins and ends with focus -- actual key
// events are CAKE's business (its own seat listeners, same connection, same
// pump; see cake.c's Wayland section).

static void keyboard_keymap(void* data, struct wl_keyboard* kb, uint32_t format, int32_t fd, uint32_t size) {
    (void)data; (void)kb; (void)format; (void)size;
    close(fd);   // not our concern -- CAKE owns key interpretation

    return;
}

static void keyboard_enter(void* data, struct wl_keyboard* kb, uint32_t serial,
                           struct wl_surface* surface, struct wl_array* keys) {
    (void)data; (void)kb; (void)serial; (void)surface; (void)keys;

    gdmf_st_hasFocus = true;
    gdmf_wl_apply_input_state();

    return;
}

static void keyboard_leave(void* data, struct wl_keyboard* kb, uint32_t serial,
                           struct wl_surface* surface) {
    (void)data; (void)kb; (void)serial; (void)surface;

    // Release capture/hidden-cursor while unfocused so switching away never
    // leaves the user's mouse stuck -- same rule as WM_KILLFOCUS.
    gdmf_st_hasFocus = false;
    gdmf_wl_apply_input_state();

    return;
}

static void keyboard_key(void* data, struct wl_keyboard* kb, uint32_t serial,
                         uint32_t time, uint32_t key, uint32_t state) {
    (void)data; (void)kb; (void)serial; (void)time; (void)key; (void)state;
    return;   // CAKE's business
}

static void keyboard_modifiers(void* data, struct wl_keyboard* kb, uint32_t serial,
                               uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group) {
    (void)data; (void)kb; (void)serial; (void)depressed; (void)latched; (void)locked; (void)group;
    return;   // CAKE's business
}

static void keyboard_repeat_info(void* data, struct wl_keyboard* kb, int32_t rate, int32_t delay) {
    (void)data; (void)kb; (void)rate; (void)delay;
    return;   // auto-repeat is a text affordance; CAKE ignores it too
}

static const struct wl_keyboard_listener g_keyboardListener = {
    .keymap      = keyboard_keymap,
    .enter       = keyboard_enter,
    .leave       = keyboard_leave,
    .key         = keyboard_key,
    .modifiers   = keyboard_modifiers,
    .repeat_info = keyboard_repeat_info,
};

// --- wl_pointer: position and cursor only ------------------------------------

static void pointer_enter(void* data, struct wl_pointer* pointer, uint32_t serial,
                          struct wl_surface* surface, wl_fixed_t sx, wl_fixed_t sy) {
    (void)data; (void)pointer; (void)surface;

    g_pointerEnterSerial = serial;
    g_pointerInside      = true;
    g_mouseX             = wl_fixed_to_int(sx);
    g_mouseY             = wl_fixed_to_int(sy);

    // A Wayland client owns its cursor image and must set it on every
    // enter -- there is no compositor default to fall back on.
    gdmf_wl_apply_input_state();

    return;
}

static void pointer_leave(void* data, struct wl_pointer* pointer, uint32_t serial,
                          struct wl_surface* surface) {
    (void)data; (void)pointer; (void)serial; (void)surface;
    g_pointerInside = false;

    return;
}

static void pointer_motion(void* data, struct wl_pointer* pointer, uint32_t time,
                           wl_fixed_t sx, wl_fixed_t sy) {
    (void)data; (void)pointer; (void)time;
    g_mouseX = wl_fixed_to_int(sx);
    g_mouseY = wl_fixed_to_int(sy);

    return;
}

static void pointer_button(void* data, struct wl_pointer* pointer, uint32_t serial,
                           uint32_t time, uint32_t button, uint32_t state) {
    (void)data; (void)pointer; (void)serial; (void)time; (void)button; (void)state;
    return;   // CAKE's business
}

static void pointer_axis(void* data, struct wl_pointer* pointer, uint32_t time,
                         uint32_t axis, wl_fixed_t value) {
    (void)data; (void)pointer; (void)time; (void)axis; (void)value;
    return;   // CAKE's business
}

static void pointer_frame(void* data, struct wl_pointer* pointer) {
    (void)data; (void)pointer;
    return;
}

static void pointer_axis_source(void* data, struct wl_pointer* pointer, uint32_t source) {
    (void)data; (void)pointer; (void)source;
    return;
}

static void pointer_axis_stop(void* data, struct wl_pointer* pointer, uint32_t time, uint32_t axis) {
    (void)data; (void)pointer; (void)time; (void)axis;
    return;
}

static void pointer_axis_discrete(void* data, struct wl_pointer* pointer, uint32_t axis, int32_t discrete) {
    (void)data; (void)pointer; (void)axis; (void)discrete;
    return;
}

static const struct wl_pointer_listener g_pointerListener = {
    .enter         = pointer_enter,
    .leave         = pointer_leave,
    .motion        = pointer_motion,
    .button        = pointer_button,
    .axis          = pointer_axis,
    .frame         = pointer_frame,
    .axis_source   = pointer_axis_source,
    .axis_stop     = pointer_axis_stop,
    .axis_discrete = pointer_axis_discrete,
};

// --- Cursor visibility / mouse capture ---------------------------------------

// Re-derives both effects from scratch every time, exactly like the Win32
// backend's gdmf_apply_input_state: cheap, and only runs on focus/enter/
// toggle events, never per-frame.
static void gdmf_wl_apply_input_state(void) {
    if (!g_surface || !g_pointer) { return; }

    pthread_mutex_lock(&g_inputStateLock);

    // Cursor image: hidden only while focused-and-hidden-desired; the
    // set_cursor call needs the latest enter serial and only means anything
    // while the pointer is actually over the surface.
    if (g_pointerInside) {
        bool shouldHide = gdmf_st_hasFocus && !gdmf_st_cursorVisibleDesired;
        if (shouldHide) {
            wl_pointer_set_cursor(g_pointer, g_pointerEnterSerial, NULL, 0, 0);
        } else if (g_cursorArrow && g_cursorSurface) {
            struct wl_cursor_image* image = g_cursorArrow->images[0];
            struct wl_buffer* buffer = wl_cursor_image_get_buffer(image);
            wl_surface_attach(g_cursorSurface, buffer, 0, 0);
            wl_surface_damage(g_cursorSurface, 0, 0, (int32_t)image->width, (int32_t)image->height);
            wl_surface_commit(g_cursorSurface);
            wl_pointer_set_cursor(g_pointer, g_pointerEnterSerial, g_cursorSurface,
                                  (int32_t)image->hotspot_x, (int32_t)image->hotspot_y);
        }
    }

    // Mouse capture: confine the pointer to the surface while focused and
    // desired (ClipCursor's counterpart). NULL region = the whole surface,
    // and the compositor keeps the confinement region current across
    // resizes on its own -- no WM_SIZE/WM_MOVE re-clip dance needed.
    bool wantConfine = gdmf_st_hasFocus && gdmf_st_mouseCaptureDesired;
    if (wantConfine && !g_confinedPointer && g_pointerConstraints) {
        g_confinedPointer = zwp_pointer_constraints_v1_confine_pointer(
            g_pointerConstraints, g_surface, g_pointer, NULL,
            ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
    } else if (!wantConfine && g_confinedPointer) {
        zwp_confined_pointer_v1_destroy(g_confinedPointer);
        g_confinedPointer = NULL;
    }

    pthread_mutex_unlock(&g_inputStateLock);

    return;
}

// --- wl_seat -----------------------------------------------------------------

static void seat_capabilities(void* data, struct wl_seat* seat, uint32_t caps) {
    (void)data;

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !g_keyboard) {
        g_keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(g_keyboard, &g_keyboardListener, NULL);
    }
    if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && g_keyboard) {
        wl_keyboard_destroy(g_keyboard);
        g_keyboard = NULL;
    }

    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !g_pointer) {
        g_pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(g_pointer, &g_pointerListener, NULL);
    }
    if (!(caps & WL_SEAT_CAPABILITY_POINTER) && g_pointer) {
        if (g_confinedPointer) {
            zwp_confined_pointer_v1_destroy(g_confinedPointer);
            g_confinedPointer = NULL;
        }
        wl_pointer_destroy(g_pointer);
        g_pointer = NULL;
    }

    return;
}

static void seat_name(void* data, struct wl_seat* seat, const char* name) {
    (void)data; (void)seat; (void)name;
    return;
}

static const struct wl_seat_listener g_seatListener = {
    .capabilities = seat_capabilities,
    .name         = seat_name,
};

static void* gdmf_wl_reader_main(void* param) {
    (void)param;

    for (;;) {
        // libwayland's multi-thread read protocol: claim reader status,
        // draining anything already queued first.
        while (wl_display_prepare_read(g_display) != 0) {
            if (wl_display_dispatch_pending(g_display) < 0) { goto dead; }
        }
        // Listeners run inside dispatch (pong, set_cursor, ack_configure)
        // and their requests must not sit in the buffer until the next
        // tick's pump -- a ping answered late is this thread's whole job.
        wl_display_flush(g_display);

        struct pollfd pfds[2] = {
            { .fd = wl_display_get_fd(g_display), .events = POLLIN },
            { .fd = g_readerStopPipe[0],          .events = POLLIN },
        };
        if (poll(pfds, 2, -1) < 0) {
            wl_display_cancel_read(g_display);
            continue;   // EINTR
        }

        if (pfds[1].revents) {                      // shutdown request
            wl_display_cancel_read(g_display);
            return NULL;
        }
        if (pfds[0].revents & (POLLERR | POLLHUP)) {
            wl_display_cancel_read(g_display);
            goto dead;
        }

        if (pfds[0].revents & POLLIN) { wl_display_read_events(g_display); }
        else                          { wl_display_cancel_read(g_display); }

        if (wl_display_dispatch_pending(g_display) < 0) { goto dead; }
    }

dead:
    // Compositor gone or protocol error: same disposition as the pump's
    // check -- stop cleanly, whichever of us notices first.
    if (!gdmf_st_closeRequested) {
        printf("[GDMF] Wayland connection lost (compositor gone or "
               "protocol error) -- shutting down\n");
        gdmf_st_closeRequested = true;
    }

    return NULL;
}

// --- Backend interface (see gdmf_window.h) -----------------------------------

static int gdmf_window_wl_create(void) {
    g_display = wl_display_connect(NULL);
    if (!g_display) {
        printf("[GDMF] wl_display_connect failed -- no Wayland compositor? "
               "(WAYLAND_DISPLAY=%s)\n", getenv("WAYLAND_DISPLAY"));

        return -1;
    }

    g_registry = wl_display_get_registry(g_display);
    wl_registry_add_listener(g_registry, &g_registryListener, NULL);
    wl_display_roundtrip(g_display);   // collect globals

    if (!g_compositor || !g_wmBase) {
        printf("[GDMF] Compositor is missing wl_compositor/xdg_wm_base\n");
        gdmf_window_wl_destroy();

        return -1;
    }

    xdg_wm_base_add_listener(g_wmBase, &g_wmBaseListener, NULL);
    if (g_seat) { wl_seat_add_listener(g_seat, &g_seatListener, NULL); }

    // Cursor theme, for showing the ordinary arrow (a Wayland client must
    // provide its own cursor image -- see pointer_enter).
    if (g_shm) {
        g_cursorTheme = wl_cursor_theme_load(NULL, 24, g_shm);
        if (g_cursorTheme) {
            g_cursorArrow = wl_cursor_theme_get_cursor(g_cursorTheme, "default");
            if (!g_cursorArrow) { g_cursorArrow = wl_cursor_theme_get_cursor(g_cursorTheme, "left_ptr"); }
        }
        if (g_cursorArrow) { g_cursorSurface = wl_compositor_create_surface(g_compositor); }
    }

    // The window: wl_surface -> xdg_surface -> xdg_toplevel.
    g_surface    = wl_compositor_create_surface(g_compositor);
    g_xdgSurface = xdg_wm_base_get_xdg_surface(g_wmBase, g_surface);
    xdg_surface_add_listener(g_xdgSurface, &g_xdgSurfaceListener, NULL);

    g_toplevel = xdg_surface_get_toplevel(g_xdgSurface);
    xdg_toplevel_add_listener(g_toplevel, &g_toplevelListener, NULL);
    xdg_toplevel_set_title(g_toplevel, gdmf_cfg_title);
    xdg_toplevel_set_app_id(g_toplevel, "fuselage");
    xdg_toplevel_set_min_size(g_toplevel, 320, 180);   // parity: WM_GETMINMAXINFO

    // Server-side decorations where the compositor offers them (KDE,
    // wlroots; GNOME doesn't). The engine draws no client-side decorations
    // -- without the protocol the window is simply borderless, and that's
    // an accepted look on the one desktop that lacks it. Confirmed on a
    // native sway session: the request is honored and sway draws its title
    // bar. A game that comes up fullscreen never sees any of this -- a
    // fullscreen toplevel has no decorations by definition, whoever owns
    // them.
    if (g_decorationManager) {
        g_decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(
            g_decorationManager, g_toplevel);
        zxdg_toplevel_decoration_v1_set_mode(g_decoration,
            ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    }

    // Keep the display awake while the engine runs -- the counterpart of
    // Win32's SetThreadExecutionState and macOS's IOPM assertion, and for
    // the same reason: a controller-only game looks idle to the OS.
    if (g_idleInhibitManager) {
        g_idleInhibitor = zwp_idle_inhibit_manager_v1_create_inhibitor(
            g_idleInhibitManager, g_surface);
    } else {
        printf("[GDMF] Warning: no idle-inhibit protocol -- display may sleep during controller play\n");
    }

    if (gdmf_cfg_iconRGBA) {
        printf("[GDMF] Note: window icons aren't portable Wayland yet; icon ignored\n");
    }

    gdmf_st_width  = gdmf_cfg_width;
    gdmf_st_height = gdmf_cfg_height;

    // Commit the bare surface and wait for the first configure -- the
    // protocol forbids attaching a buffer before acking it, and Vulkan's
    // swapchain will be attaching buffers the moment the renderer starts.
    // This blocking dispatch is safe: the reader thread doesn't exist yet.
    wl_surface_commit(g_surface);
    while (!g_initialConfigureSeen && wl_display_dispatch(g_display) != -1) { }

    // From here on, the reader thread owns all default-queue dispatching
    // (see its comment above) -- no blocking Wayland call may touch the
    // default queue on any other thread past this point.
    if (pipe(g_readerStopPipe) != 0 ||
        pthread_create(&g_readerThread, NULL, gdmf_wl_reader_main, NULL) != 0) {
        printf("[GDMF] Failed to start the Wayland event reader thread\n");
        gdmf_window_wl_destroy();

        return -1;
    }
    g_readerRunning = true;

    // Apply whatever mouse capture / cursor visibility was requested before
    // the window existed -- same sequencing as the other backends. Focus
    // arrives via wl_keyboard.enter moments later; starting from "focused"
    // matches Win32/macOS and is corrected by the first enter/leave.
    gdmf_st_hasFocus = true;
    gdmf_wl_apply_input_state();

    printf("[GDMF] Wayland window ready\n");

    return 0;
}

static void gdmf_window_wl_destroy(void) {
    // Stop the reader thread first -- object teardown below must not race
    // its dispatching. A byte on the self-pipe wakes its poll; if the
    // thread already exited on a dead connection, the join returns at once.
    if (g_readerRunning) {
        char stop = 1;
        ssize_t w = write(g_readerStopPipe[1], &stop, 1);
        (void)w;   // best effort; a full pipe means it's already waking
        pthread_join(g_readerThread, NULL);
        g_readerRunning = false;
    }
    if (g_readerStopPipe[0] >= 0) { close(g_readerStopPipe[0]); g_readerStopPipe[0] = -1; }
    if (g_readerStopPipe[1] >= 0) { close(g_readerStopPipe[1]); g_readerStopPipe[1] = -1; }

    if (g_idleInhibitor)     { zwp_idle_inhibitor_v1_destroy(g_idleInhibitor);         g_idleInhibitor = NULL; }
    if (g_confinedPointer)   { zwp_confined_pointer_v1_destroy(g_confinedPointer);     g_confinedPointer = NULL; }
    if (g_decoration)        { zxdg_toplevel_decoration_v1_destroy(g_decoration);      g_decoration = NULL; }
    if (g_keyboard)          { wl_keyboard_destroy(g_keyboard);                        g_keyboard = NULL; }
    if (g_pointer)           { wl_pointer_destroy(g_pointer);                          g_pointer = NULL; }
    if (g_toplevel)          { xdg_toplevel_destroy(g_toplevel);                       g_toplevel = NULL; }
    if (g_xdgSurface)        { xdg_surface_destroy(g_xdgSurface);                      g_xdgSurface = NULL; }
    if (g_surface)           { wl_surface_destroy(g_surface);                          g_surface = NULL; }
    if (g_cursorSurface)     { wl_surface_destroy(g_cursorSurface);                    g_cursorSurface = NULL; }
    if (g_cursorTheme)       { wl_cursor_theme_destroy(g_cursorTheme);                 g_cursorTheme = NULL; }
    if (g_idleInhibitManager){ zwp_idle_inhibit_manager_v1_destroy(g_idleInhibitManager); g_idleInhibitManager = NULL; }
    if (g_pointerConstraints){ zwp_pointer_constraints_v1_destroy(g_pointerConstraints);  g_pointerConstraints = NULL; }
    if (g_decorationManager) { zxdg_decoration_manager_v1_destroy(g_decorationManager);   g_decorationManager = NULL; }
    if (g_seat)              { wl_seat_destroy(g_seat);                                g_seat = NULL; }
    if (g_shm)               { wl_shm_destroy(g_shm);                                  g_shm = NULL; }
    if (g_wmBase)            { xdg_wm_base_destroy(g_wmBase);                          g_wmBase = NULL; }
    if (g_compositor)        { wl_compositor_destroy(g_compositor);                    g_compositor = NULL; }
    if (g_registry)          { wl_registry_destroy(g_registry);                        g_registry = NULL; }

    if (g_display) {
        wl_display_flush(g_display);
        wl_display_disconnect(g_display);
        g_display = NULL;
    }

    g_initialConfigureSeen = false;

    return;
}

static void gdmf_window_wl_pump(void) {
    if (!g_display) { return; }

    // Dispatching belongs to the reader thread alone; the per-tick pump
    // just pushes this thread's accumulated requests out and turns a dead
    // connection into "window closed" (same disposition as
    // gdmf_vulkan_device_lost -- stop cleanly, don't thrash).
    if (wl_display_get_error(g_display) != 0) {
        if (!gdmf_st_closeRequested) {
            printf("[GDMF] Wayland connection lost (compositor gone or "
                   "protocol error) -- shutting down\n");
            gdmf_st_closeRequested = true;
        }
        return;
    }

    wl_display_flush(g_display);

    return;
}

static void gdmf_window_wl_request_display_mode(GDMF_DisplayMode mode) {
    if (!g_toplevel) { return; }

    // Called from the game/render side; xdg requests are queued on the
    // connection and go out with the pump's flush. The compositor's
    // configure confirms the switch -- gdmf_st_displayMode updates there
    // (xdg_surface_configure), matching the "backend updates on actual
    // switch" contract.
    g_requestedMode = mode;

    if (mode == GDMF_MODE_WINDOWED) {
        xdg_toplevel_unset_fullscreen(g_toplevel);
    } else {
        // BORDERLESS and FULLSCREEN_EXCLUSIVE both: cover the monitor.
        // NULL output = the compositor picks (the one the window is on).
        xdg_toplevel_set_fullscreen(g_toplevel, NULL);
    }

    return;
}

static void gdmf_window_wl_input_state_changed(void) {
    gdmf_wl_apply_input_state();

    return;
}

static bool gdmf_window_wl_get_mouse_client(int* x, int* y) {
    if (!g_surface) { return false; }

    // Last known surface-local position -- surface coordinates are the same
    // space as gdmf_st_width/height at buffer scale 1. Wayland reports
    // nothing once the pointer leaves the surface, so "last known" is the
    // honest best available (see the header's parity notes).
    *x = g_mouseX;
    *y = g_mouseY;

    return true;
}

// Native handles for gdmf_surface_wayland.c
void* GDMF_GetWaylandDisplay(void) { return (void*)g_display; }
void* GDMF_GetWaylandSurface(void) { return (void*)g_surface; }

// The tagged handle for CAKE_AttachWindow (see fuselage_native.h).
// Wayland has no single "window" object a stranger could use; the
// connection is what a subsystem starts from (binding its own seat, its
// own listeners, riding the same pump) -- see cake.c's Linux section for
// the taker's side. Filled fresh on each call: g_display changes across
// create/destroy cycles.
static FuselageLinuxNativeHandle g_nativeHandle;
static void* gdmf_window_wl_native_handle(void) {
    g_nativeHandle.kind    = "wayland";
    g_nativeHandle.display = (void*)g_display;
    g_nativeHandle.window  = NULL;

    return (void*)&g_nativeHandle;
}

// --- The dispatch table (see gdmf_window_linux.h) ----------------------------

const gdmf_linux_backend gdmf_backend_wayland = {
    .name                 = "wayland",
    .create               = gdmf_window_wl_create,
    .destroy              = gdmf_window_wl_destroy,
    .pump                 = gdmf_window_wl_pump,
    .request_display_mode = gdmf_window_wl_request_display_mode,
    .input_state_changed  = gdmf_window_wl_input_state_changed,
    .get_mouse_client     = gdmf_window_wl_get_mouse_client,
    .native_handle        = gdmf_window_wl_native_handle,
};
