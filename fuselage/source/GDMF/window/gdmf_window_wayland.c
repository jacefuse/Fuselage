// Wayland window lifecycle, protocol listeners, input presentation, and event reader.
// SEE: (GDMF_LINUX.md, Wayland event ownership).
#define _POSIX_C_SOURCE 200809L

#include "../gdmf.h"
#include "gdmf_window.h"
#include "gdmf_window_linux.h"
#include "../../BASE/fuselage_native.h"

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

static struct wl_display*    gdmf_wayland_display_connection = NULL;
static struct wl_registry*   gdmf_wayland_registry = NULL;
static struct wl_compositor* gdmf_wayland_compositor = NULL;
static struct xdg_wm_base*   gdmf_wayland_shell = NULL;
static struct wl_seat*       gdmf_wayland_seat = NULL;
static struct wl_shm*        gdmf_wayland_shared_memory = NULL;

static struct zxdg_decoration_manager_v1*  gdmf_wayland_decoration_manager = NULL;
static struct zwp_pointer_constraints_v1*  gdmf_wayland_pointer_constraints = NULL;
static struct zwp_idle_inhibit_manager_v1* gdmf_wayland_idle_inhibit_manager = NULL;

static struct wl_surface*    gdmf_wayland_surface = NULL;
static struct xdg_surface*   gdmf_wayland_shell_surface = NULL;
static struct xdg_toplevel*  gdmf_wayland_toplevel = NULL;

static struct zxdg_toplevel_decoration_v1* gdmf_wayland_decoration = NULL;
static struct zwp_idle_inhibitor_v1*       gdmf_wayland_idle_inhibitor = NULL;

static bool gdmf_wayland_initial_configure_seen = false;

// Toplevel events stage dimensions and flags; surface configure acknowledges them.
static int  gdmf_wayland_pending_width = 0;
static int  gdmf_wayland_pending_height = 0;
static bool gdmf_wayland_pending_resizing = false;
static bool gdmf_wayland_pending_fullscreen = false;
static bool gdmf_wayland_pending_suspended = false;

static int gdmf_wayland_saved_windowed_width = 0;
static int gdmf_wayland_saved_windowed_height = 0;

static bool gdmf_wayland_fullscreen_confirmed = false;

// Both fullscreen requests share one compositor state; retain the requested label.
static GDMF_DisplayMode gdmf_wayland_requested_mode = GDMF_MODE_WINDOWED;

static struct wl_keyboard* gdmf_wayland_keyboard = NULL;
static struct wl_pointer*  gdmf_wayland_pointer = NULL;

static struct wl_cursor_theme* gdmf_wayland_cursor_theme = NULL;
static struct wl_surface*      gdmf_wayland_cursor_surface = NULL;
static struct wl_cursor*       gdmf_wayland_arrow_cursor = NULL;

static uint32_t gdmf_wayland_pointer_enter_serial = 0;
static bool     gdmf_wayland_pointer_inside = false;
static int      gdmf_wayland_last_mouse_x = 0;
static int      gdmf_wayland_last_mouse_y = 0;

static struct zwp_confined_pointer_v1* gdmf_wayland_confined_pointer = NULL;

// Protects confinement object updates; it does not synchronize all backend state.
static pthread_mutex_t gdmf_wayland_input_state_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_t gdmf_wayland_reader_thread;
static bool      gdmf_wayland_reader_started = false;
static int       gdmf_wayland_reader_stop_pipe[2] = { -1, -1 };

static void gdmf_wayland_apply_input_state(void);
static void gdmf_wayland_destroy_window(void);

static void gdmf_wayland_handle_registry_global(void* listener_data, struct wl_registry* registry,
                            uint32_t global_name, const char* interface_name, uint32_t interface_version) {
    (void)listener_data;

    if (strcmp(interface_name, wl_compositor_interface.name) == 0) {
        gdmf_wayland_compositor = wl_registry_bind(registry, global_name, &wl_compositor_interface,
                                        interface_version < 4 ? interface_version : 4);
    } else if (strcmp(interface_name, xdg_wm_base_interface.name) == 0) {

        gdmf_wayland_shell = wl_registry_bind(registry, global_name, &xdg_wm_base_interface,
                                    interface_version < 6 ? interface_version : 6);
    } else if (strcmp(interface_name, wl_seat_interface.name) == 0) {
        gdmf_wayland_seat = wl_registry_bind(registry, global_name, &wl_seat_interface,
                                  interface_version < 5 ? interface_version : 5);
    } else if (strcmp(interface_name, wl_shm_interface.name) == 0) {
        gdmf_wayland_shared_memory = wl_registry_bind(registry, global_name, &wl_shm_interface, 1);
    } else if (strcmp(interface_name, zxdg_decoration_manager_v1_interface.name) == 0) {
        gdmf_wayland_decoration_manager = wl_registry_bind(registry, global_name,
                                               &zxdg_decoration_manager_v1_interface, 1);
    } else if (strcmp(interface_name, zwp_pointer_constraints_v1_interface.name) == 0) {
        gdmf_wayland_pointer_constraints = wl_registry_bind(registry, global_name,
                                                &zwp_pointer_constraints_v1_interface, 1);
    } else if (strcmp(interface_name, zwp_idle_inhibit_manager_v1_interface.name) == 0) {
        gdmf_wayland_idle_inhibit_manager = wl_registry_bind(registry, global_name,
                                                &zwp_idle_inhibit_manager_v1_interface, 1);
    }

    return;
}

// Global removal is currently ignored; dynamic registry handling needs hardening.
static void gdmf_wayland_handle_registry_remove(void* listener_data, struct wl_registry* registry, uint32_t global_name) {
    (void)listener_data; (void)registry; (void)global_name;

    return;
}

static const struct wl_registry_listener gdmf_wayland_registry_listener = {
    .global = gdmf_wayland_handle_registry_global,
    .global_remove = gdmf_wayland_handle_registry_remove,
};

static void gdmf_wayland_handle_shell_ping(void* listener_data, struct xdg_wm_base* shell, uint32_t event_serial) {
    (void)listener_data;
    xdg_wm_base_pong(shell, event_serial);

    return;
}

static const struct xdg_wm_base_listener gdmf_wayland_shell_listener = {
    .ping = gdmf_wayland_handle_shell_ping,
};

static void gdmf_wayland_fit_aspect_ratio(int* window_width, int* window_height) {
    if (*window_width <= 0 || *window_height <= 0) { return; }

    int height_for_width = (*window_width * gdmf_window_config_aspect_denominator) / gdmf_window_config_aspect_numerator;
    if (height_for_width <= *window_height) { *window_height = height_for_width; }
    else { *window_width = (*window_height * gdmf_window_config_aspect_numerator) / gdmf_window_config_aspect_denominator; }

    return;
}

static void gdmf_wayland_handle_toplevel_configure(void* listener_data, struct xdg_toplevel* toplevel,
                               int32_t width, int32_t height, struct wl_array* states) {
    (void)listener_data; (void)toplevel;

    gdmf_wayland_pending_width = width;
    gdmf_wayland_pending_height = height;
    gdmf_wayland_pending_resizing = false;
    gdmf_wayland_pending_fullscreen = false;
    gdmf_wayland_pending_suspended = false;

    uint32_t* window_state;
    wl_array_for_each(window_state, states) {
        switch (*window_state) {
        case XDG_TOPLEVEL_STATE_RESIZING:   gdmf_wayland_pending_resizing = true; break;
        case XDG_TOPLEVEL_STATE_FULLSCREEN: gdmf_wayland_pending_fullscreen = true; break;
#ifdef XDG_TOPLEVEL_STATE_SUSPENDED_SINCE_VERSION
        case XDG_TOPLEVEL_STATE_SUSPENDED:  gdmf_wayland_pending_suspended = true; break;
#endif
        default: break;
        }
    }

    return;
}

static void gdmf_wayland_handle_toplevel_close(void* listener_data, struct xdg_toplevel* toplevel) {
    (void)listener_data; (void)toplevel;
    gdmf_window_close_requested = true;

    return;
}

// Optional protocol advice is currently ignored; preserve listener entries.
static void gdmf_wayland_handle_configure_bounds(void* listener_data, struct xdg_toplevel* toplevel,
                                      int32_t width, int32_t height) {
    (void)listener_data; (void)toplevel; (void)width; (void)height;

    return;
}

// Optional capability advice is currently ignored; preserve listener entries.
static void gdmf_wayland_handle_window_capabilities(void* listener_data, struct xdg_toplevel* toplevel,
                                     struct wl_array* capabilities) {
    (void)listener_data; (void)toplevel; (void)capabilities;

    return;
}

static const struct xdg_toplevel_listener gdmf_wayland_toplevel_listener = {
    .configure = gdmf_wayland_handle_toplevel_configure,
    .close = gdmf_wayland_handle_toplevel_close,
    .configure_bounds = gdmf_wayland_handle_configure_bounds,
    .wm_capabilities = gdmf_wayland_handle_window_capabilities,
};

// Acknowledge the configure cycle before publishing its staged dimensions.
static void gdmf_wayland_handle_surface_configure(void* listener_data, struct xdg_surface* shell_surface, uint32_t event_serial) {
    (void)listener_data;

    xdg_surface_ack_configure(shell_surface, event_serial);

    int window_width = gdmf_wayland_pending_width;
    int window_height = gdmf_wayland_pending_height;

    if (window_width == 0 || window_height == 0) {

        window_width = gdmf_window_width;
        window_height = gdmf_window_height;
    } else if (!gdmf_wayland_pending_fullscreen) {

        (void)gdmf_wayland_pending_resizing;
        gdmf_wayland_fit_aspect_ratio(&window_width, &window_height);
    }

    if (gdmf_wayland_fullscreen_confirmed && !gdmf_wayland_pending_fullscreen) {

        if (gdmf_wayland_pending_width == 0 && gdmf_wayland_saved_windowed_width > 0) {
            window_width = gdmf_wayland_saved_windowed_width;
            window_height = gdmf_wayland_saved_windowed_height;
        }
        gdmf_wayland_requested_mode = GDMF_MODE_WINDOWED;
    }
    if (!gdmf_wayland_fullscreen_confirmed && gdmf_wayland_pending_fullscreen) {

        gdmf_wayland_saved_windowed_width = gdmf_window_width;
        gdmf_wayland_saved_windowed_height = gdmf_window_height;
    }
    gdmf_wayland_fullscreen_confirmed = gdmf_wayland_pending_fullscreen;

    GDMF_DisplayMode confirmed_mode =
        gdmf_wayland_fullscreen_confirmed ? (gdmf_wayland_requested_mode == GDMF_MODE_WINDOWED
                        ? GDMF_MODE_BORDERLESS : gdmf_wayland_requested_mode)
                     : GDMF_MODE_WINDOWED;
    if (confirmed_mode != gdmf_window_display_mode) {
        gdmf_window_display_mode = confirmed_mode;
        printf("[GDMF] Display mode: %s\n",
               confirmed_mode == GDMF_MODE_WINDOWED ? "WINDOWED" :
               confirmed_mode == GDMF_MODE_BORDERLESS ? "BORDERLESS" : "FULLSCREEN EXCLUSIVE");
    }

    bool was_minimized = gdmf_window_minimized;
    gdmf_window_minimized = gdmf_wayland_pending_suspended;

    if (window_width > 0 && window_height > 0 && (was_minimized || window_width != gdmf_window_width || window_height != gdmf_window_height)) {
        gdmf_window_width = window_width;
        gdmf_window_height = window_height;
        gdmf_window_resize_pending = true;
    }

    gdmf_wayland_initial_configure_seen = true;

    return;
}

static const struct xdg_surface_listener gdmf_wayland_surface_listener = {
    .configure = gdmf_wayland_handle_surface_configure,
};

// GDMF tracks focus; key interpretation belongs to the input system.
static void gdmf_wayland_handle_keyboard_keymap(void* listener_data, struct wl_keyboard* keyboard, uint32_t keymap_format, int32_t keymap_descriptor, uint32_t keymap_bytes) {
    (void)listener_data; (void)keyboard; (void)keymap_format; (void)keymap_bytes;
    close(keymap_descriptor);

    return;
}

static void gdmf_wayland_handle_keyboard_enter(void* listener_data, struct wl_keyboard* keyboard, uint32_t event_serial,
                           struct wl_surface* surface, struct wl_array* keys) {
    (void)listener_data; (void)keyboard; (void)event_serial; (void)surface; (void)keys;

    gdmf_window_has_focus = true;
    gdmf_wayland_apply_input_state();

    return;
}

static void gdmf_wayland_handle_keyboard_leave(void* listener_data, struct wl_keyboard* keyboard, uint32_t event_serial,
                           struct wl_surface* surface) {
    (void)listener_data; (void)keyboard; (void)event_serial; (void)surface;

    gdmf_window_has_focus = false;
    gdmf_wayland_apply_input_state();

    return;
}

static void gdmf_wayland_handle_keyboard_key(void* listener_data, struct wl_keyboard* keyboard, uint32_t event_serial,
                         uint32_t event_time, uint32_t key, uint32_t state) {
    (void)listener_data; (void)keyboard; (void)event_serial; (void)event_time; (void)key; (void)state;
    return;
}

static void gdmf_wayland_handle_keyboard_modifiers(void* listener_data, struct wl_keyboard* keyboard, uint32_t event_serial,
                               uint32_t depressed_modifiers, uint32_t latched_modifiers, uint32_t locked_modifiers, uint32_t keyboard_group) {
    (void)listener_data; (void)keyboard; (void)event_serial; (void)depressed_modifiers; (void)latched_modifiers; (void)locked_modifiers; (void)keyboard_group;
    return;
}

static void gdmf_wayland_handle_keyboard_repeat(void* listener_data, struct wl_keyboard* keyboard, int32_t repeat_rate, int32_t repeat_delay) {
    (void)listener_data; (void)keyboard; (void)repeat_rate; (void)repeat_delay;
    return;
}

static const struct wl_keyboard_listener gdmf_wayland_keyboard_listener = {
    .keymap = gdmf_wayland_handle_keyboard_keymap,
    .enter = gdmf_wayland_handle_keyboard_enter,
    .leave = gdmf_wayland_handle_keyboard_leave,
    .key = gdmf_wayland_handle_keyboard_key,
    .modifiers = gdmf_wayland_handle_keyboard_modifiers,
    .repeat_info = gdmf_wayland_handle_keyboard_repeat,
};

// Cursor selection uses the serial from the latest surface entry.
static void gdmf_wayland_handle_pointer_enter(void* listener_data, struct wl_pointer* pointer, uint32_t event_serial,
                          struct wl_surface* surface, wl_fixed_t surface_x, wl_fixed_t surface_y) {
    (void)listener_data; (void)pointer; (void)surface;

    gdmf_wayland_pointer_enter_serial = event_serial;
    gdmf_wayland_pointer_inside = true;
    gdmf_wayland_last_mouse_x = wl_fixed_to_int(surface_x);
    gdmf_wayland_last_mouse_y = wl_fixed_to_int(surface_y);

    gdmf_wayland_apply_input_state();

    return;
}

static void gdmf_wayland_handle_pointer_leave(void* listener_data, struct wl_pointer* pointer, uint32_t event_serial,
                          struct wl_surface* surface) {
    (void)listener_data; (void)pointer; (void)event_serial; (void)surface;
    gdmf_wayland_pointer_inside = false;

    return;
}

static void gdmf_wayland_handle_pointer_motion(void* listener_data, struct wl_pointer* pointer, uint32_t event_time,
                           wl_fixed_t surface_x, wl_fixed_t surface_y) {
    (void)listener_data; (void)pointer; (void)event_time;
    gdmf_wayland_last_mouse_x = wl_fixed_to_int(surface_x);
    gdmf_wayland_last_mouse_y = wl_fixed_to_int(surface_y);

    return;
}

static void gdmf_wayland_handle_pointer_button(void* listener_data, struct wl_pointer* pointer, uint32_t event_serial,
                           uint32_t event_time, uint32_t button, uint32_t state) {
    (void)listener_data; (void)pointer; (void)event_serial; (void)event_time; (void)button; (void)state;
    return;
}

static void gdmf_wayland_handle_pointer_axis(void* listener_data, struct wl_pointer* pointer, uint32_t event_time,
                         uint32_t axis, wl_fixed_t value) {
    (void)listener_data; (void)pointer; (void)event_time; (void)axis; (void)value;
    return;
}

static void gdmf_wayland_handle_pointer_frame(void* listener_data, struct wl_pointer* pointer) {
    (void)listener_data; (void)pointer;
    return;
}

static void gdmf_wayland_handle_axis_source(void* listener_data, struct wl_pointer* pointer, uint32_t source) {
    (void)listener_data; (void)pointer; (void)source;
    return;
}

static void gdmf_wayland_handle_axis_stop(void* listener_data, struct wl_pointer* pointer, uint32_t event_time, uint32_t axis) {
    (void)listener_data; (void)pointer; (void)event_time; (void)axis;
    return;
}

static void gdmf_wayland_handle_axis_discrete(void* listener_data, struct wl_pointer* pointer, uint32_t axis, int32_t discrete) {
    (void)listener_data; (void)pointer; (void)axis; (void)discrete;
    return;
}

static const struct wl_pointer_listener gdmf_wayland_pointer_listener = {
    .enter = gdmf_wayland_handle_pointer_enter,
    .leave = gdmf_wayland_handle_pointer_leave,
    .motion = gdmf_wayland_handle_pointer_motion,
    .button = gdmf_wayland_handle_pointer_button,
    .axis = gdmf_wayland_handle_pointer_axis,
    .frame = gdmf_wayland_handle_pointer_frame,
    .axis_source = gdmf_wayland_handle_axis_source,
    .axis_stop = gdmf_wayland_handle_axis_stop,
    .axis_discrete = gdmf_wayland_handle_axis_discrete,
};

// Apply cursor appearance and optional confinement according to requested focus policy.
static void gdmf_wayland_apply_input_state(void) {
    if (!gdmf_wayland_surface || !gdmf_wayland_pointer) { return; }

    pthread_mutex_lock(&gdmf_wayland_input_state_mutex);

    if (gdmf_wayland_pointer_inside) {
        bool hide_cursor = gdmf_window_has_focus && !gdmf_window_cursor_visible_requested;
        if (hide_cursor) {
            wl_pointer_set_cursor(gdmf_wayland_pointer, gdmf_wayland_pointer_enter_serial, NULL, 0, 0);
        } else if (gdmf_wayland_arrow_cursor && gdmf_wayland_cursor_surface) {
            struct wl_cursor_image* cursor_image = gdmf_wayland_arrow_cursor->images[0];
            struct wl_buffer* cursor_buffer = wl_cursor_image_get_buffer(cursor_image);
            wl_surface_attach(gdmf_wayland_cursor_surface, cursor_buffer, 0, 0);
            wl_surface_damage(gdmf_wayland_cursor_surface, 0, 0, (int32_t)cursor_image->width, (int32_t)cursor_image->height);
            wl_surface_commit(gdmf_wayland_cursor_surface);
            wl_pointer_set_cursor(gdmf_wayland_pointer, gdmf_wayland_pointer_enter_serial, gdmf_wayland_cursor_surface,
                                  (int32_t)cursor_image->hotspot_x, (int32_t)cursor_image->hotspot_y);
        }
    }

    bool confine_requested = gdmf_window_has_focus && gdmf_window_mouse_capture_requested;
    if (confine_requested && !gdmf_wayland_confined_pointer && gdmf_wayland_pointer_constraints) {
        gdmf_wayland_confined_pointer = zwp_pointer_constraints_v1_confine_pointer(
            gdmf_wayland_pointer_constraints, gdmf_wayland_surface, gdmf_wayland_pointer, NULL,
            ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
    } else if (!confine_requested && gdmf_wayland_confined_pointer) {
        zwp_confined_pointer_v1_destroy(gdmf_wayland_confined_pointer);
        gdmf_wayland_confined_pointer = NULL;
    }

    pthread_mutex_unlock(&gdmf_wayland_input_state_mutex);

    return;
}

static void gdmf_wayland_handle_seat_capabilities(void* listener_data, struct wl_seat* seat, uint32_t seat_capabilities) {
    (void)listener_data;

    if ((seat_capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && !gdmf_wayland_keyboard) {
        gdmf_wayland_keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(gdmf_wayland_keyboard, &gdmf_wayland_keyboard_listener, NULL);
    }
    if (!(seat_capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && gdmf_wayland_keyboard) {
        wl_keyboard_destroy(gdmf_wayland_keyboard);
        gdmf_wayland_keyboard = NULL;
    }

    if ((seat_capabilities & WL_SEAT_CAPABILITY_POINTER) && !gdmf_wayland_pointer) {
        gdmf_wayland_pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(gdmf_wayland_pointer, &gdmf_wayland_pointer_listener, NULL);
    }
    if (!(seat_capabilities & WL_SEAT_CAPABILITY_POINTER) && gdmf_wayland_pointer) {
        if (gdmf_wayland_confined_pointer) {
            zwp_confined_pointer_v1_destroy(gdmf_wayland_confined_pointer);
            gdmf_wayland_confined_pointer = NULL;
        }
        wl_pointer_destroy(gdmf_wayland_pointer);
        gdmf_wayland_pointer = NULL;
    }

    return;
}

static void gdmf_wayland_handle_seat_name(void* listener_data, struct wl_seat* seat, const char* seat_name) {
    (void)listener_data; (void)seat; (void)seat_name;
    return;
}

static const struct wl_seat_listener gdmf_wayland_seat_listener = {
    .capabilities = gdmf_wayland_handle_seat_capabilities,
    .name = gdmf_wayland_handle_seat_name,
};

// Owns default-queue dispatch after creation; the stop pipe wakes its poll.
static void* gdmf_wayland_read_events(void* thread_parameter) {
    (void)thread_parameter;

    for (;;) {

        while (wl_display_prepare_read(gdmf_wayland_display_connection) != 0) {
            if (wl_display_dispatch_pending(gdmf_wayland_display_connection) < 0) { goto connection_lost; }
        }

        wl_display_flush(gdmf_wayland_display_connection);

        struct pollfd poll_descriptors[2] = {
            { .fd = wl_display_get_fd(gdmf_wayland_display_connection), .events = POLLIN },
            { .fd = gdmf_wayland_reader_stop_pipe[0],          .events = POLLIN },
        };
        if (poll(poll_descriptors, 2, -1) < 0) {
            wl_display_cancel_read(gdmf_wayland_display_connection);
            continue;
        }

        if (poll_descriptors[1].revents) {
            wl_display_cancel_read(gdmf_wayland_display_connection);
            return NULL;
        }
        if (poll_descriptors[0].revents & (POLLERR | POLLHUP)) {
            wl_display_cancel_read(gdmf_wayland_display_connection);
            goto connection_lost;
        }

        if (poll_descriptors[0].revents & POLLIN) { wl_display_read_events(gdmf_wayland_display_connection); }
        else                          { wl_display_cancel_read(gdmf_wayland_display_connection); }

        if (wl_display_dispatch_pending(gdmf_wayland_display_connection) < 0) { goto connection_lost; }
    }

connection_lost:

    if (!gdmf_window_close_requested) {
        printf("[GDMF] Wayland connection lost (compositor gone or "
               "protocol error); shutting down\n");
        gdmf_window_close_requested = true;
    }

    return NULL;
}

// Initial configure is dispatched here before starting the sole default-queue reader.
static int gdmf_wayland_create_window(void) {
    gdmf_wayland_display_connection = wl_display_connect(NULL);
    if (!gdmf_wayland_display_connection) {
        printf("[GDMF] wl_display_connect failed "
               "(WAYLAND_DISPLAY=%s)\n", getenv("WAYLAND_DISPLAY"));

        return -1;
    }

    gdmf_wayland_registry = wl_display_get_registry(gdmf_wayland_display_connection);
    wl_registry_add_listener(gdmf_wayland_registry, &gdmf_wayland_registry_listener, NULL);
    wl_display_roundtrip(gdmf_wayland_display_connection);

    if (!gdmf_wayland_compositor || !gdmf_wayland_shell) {
        printf("[GDMF] Compositor is missing wl_compositor/xdg_wm_base\n");
        gdmf_wayland_destroy_window();

        return -1;
    }

    xdg_wm_base_add_listener(gdmf_wayland_shell, &gdmf_wayland_shell_listener, NULL);
    if (gdmf_wayland_seat) { wl_seat_add_listener(gdmf_wayland_seat, &gdmf_wayland_seat_listener, NULL); }

    if (gdmf_wayland_shared_memory) {
        gdmf_wayland_cursor_theme = wl_cursor_theme_load(NULL, 24, gdmf_wayland_shared_memory);
        if (gdmf_wayland_cursor_theme) {
            gdmf_wayland_arrow_cursor = wl_cursor_theme_get_cursor(gdmf_wayland_cursor_theme, "default");
            if (!gdmf_wayland_arrow_cursor) { gdmf_wayland_arrow_cursor = wl_cursor_theme_get_cursor(gdmf_wayland_cursor_theme, "left_ptr"); }
        }
        if (gdmf_wayland_arrow_cursor) { gdmf_wayland_cursor_surface = wl_compositor_create_surface(gdmf_wayland_compositor); }
    }

    gdmf_wayland_surface = wl_compositor_create_surface(gdmf_wayland_compositor);
    gdmf_wayland_shell_surface = xdg_wm_base_get_xdg_surface(gdmf_wayland_shell, gdmf_wayland_surface);
    xdg_surface_add_listener(gdmf_wayland_shell_surface, &gdmf_wayland_surface_listener, NULL);

    gdmf_wayland_toplevel = xdg_surface_get_toplevel(gdmf_wayland_shell_surface);
    xdg_toplevel_add_listener(gdmf_wayland_toplevel, &gdmf_wayland_toplevel_listener, NULL);
    xdg_toplevel_set_title(gdmf_wayland_toplevel, gdmf_window_config_title);
    xdg_toplevel_set_app_id(gdmf_wayland_toplevel, "fuselage");
    xdg_toplevel_set_min_size(gdmf_wayland_toplevel, 320, 180);

    if (gdmf_wayland_decoration_manager) {
        gdmf_wayland_decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(
            gdmf_wayland_decoration_manager, gdmf_wayland_toplevel);
        zxdg_toplevel_decoration_v1_set_mode(gdmf_wayland_decoration,
            ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    }

    if (gdmf_wayland_idle_inhibit_manager) {
        gdmf_wayland_idle_inhibitor = zwp_idle_inhibit_manager_v1_create_inhibitor(
            gdmf_wayland_idle_inhibit_manager, gdmf_wayland_surface);
    } else {
        printf("[GDMF] Warning: no idle-inhibit protocol; display may sleep during controller play\n");
    }

    if (gdmf_window_config_icon_rgba) {
        printf("[GDMF] Note: window icons aren't portable Wayland yet; icon ignored\n");
    }

    gdmf_window_width = gdmf_window_config_width;
    gdmf_window_height = gdmf_window_config_height;

    wl_surface_commit(gdmf_wayland_surface);
    while (!gdmf_wayland_initial_configure_seen && wl_display_dispatch(gdmf_wayland_display_connection) != -1) { }

    if (pipe(gdmf_wayland_reader_stop_pipe) != 0 ||
        pthread_create(&gdmf_wayland_reader_thread, NULL, gdmf_wayland_read_events, NULL) != 0) {
        printf("[GDMF] Failed to start the Wayland event reader thread\n");
        gdmf_wayland_destroy_window();

        return -1;
    }
    gdmf_wayland_reader_started = true;

    gdmf_window_has_focus = true;
    gdmf_wayland_apply_input_state();

    printf("[GDMF] Wayland window ready\n");

    return 0;
}

// Stop dispatch before destroying the objects used by its callbacks.
static void gdmf_wayland_destroy_window(void) {

    if (gdmf_wayland_reader_started) {
        char stop_signal = 1;
        ssize_t bytes_written = write(gdmf_wayland_reader_stop_pipe[1], &stop_signal, 1);
        (void)bytes_written;
        pthread_join(gdmf_wayland_reader_thread, NULL);
        gdmf_wayland_reader_started = false;
    }
    if (gdmf_wayland_reader_stop_pipe[0] >= 0) { close(gdmf_wayland_reader_stop_pipe[0]); gdmf_wayland_reader_stop_pipe[0] = -1; }
    if (gdmf_wayland_reader_stop_pipe[1] >= 0) { close(gdmf_wayland_reader_stop_pipe[1]); gdmf_wayland_reader_stop_pipe[1] = -1; }

    if (gdmf_wayland_idle_inhibitor)     { zwp_idle_inhibitor_v1_destroy(gdmf_wayland_idle_inhibitor);         gdmf_wayland_idle_inhibitor = NULL; }
    if (gdmf_wayland_confined_pointer)   { zwp_confined_pointer_v1_destroy(gdmf_wayland_confined_pointer);     gdmf_wayland_confined_pointer = NULL; }
    if (gdmf_wayland_decoration)        { zxdg_toplevel_decoration_v1_destroy(gdmf_wayland_decoration);      gdmf_wayland_decoration = NULL; }
    if (gdmf_wayland_keyboard)          { wl_keyboard_destroy(gdmf_wayland_keyboard);                        gdmf_wayland_keyboard = NULL; }
    if (gdmf_wayland_pointer)           { wl_pointer_destroy(gdmf_wayland_pointer);                          gdmf_wayland_pointer = NULL; }
    if (gdmf_wayland_toplevel)          { xdg_toplevel_destroy(gdmf_wayland_toplevel);                       gdmf_wayland_toplevel = NULL; }
    if (gdmf_wayland_shell_surface)        { xdg_surface_destroy(gdmf_wayland_shell_surface);                      gdmf_wayland_shell_surface = NULL; }
    if (gdmf_wayland_surface)           { wl_surface_destroy(gdmf_wayland_surface);                          gdmf_wayland_surface = NULL; }
    if (gdmf_wayland_cursor_surface)     { wl_surface_destroy(gdmf_wayland_cursor_surface);                    gdmf_wayland_cursor_surface = NULL; }
    if (gdmf_wayland_cursor_theme)       { wl_cursor_theme_destroy(gdmf_wayland_cursor_theme);                 gdmf_wayland_cursor_theme = NULL; }
    if (gdmf_wayland_idle_inhibit_manager){ zwp_idle_inhibit_manager_v1_destroy(gdmf_wayland_idle_inhibit_manager); gdmf_wayland_idle_inhibit_manager = NULL; }
    if (gdmf_wayland_pointer_constraints){ zwp_pointer_constraints_v1_destroy(gdmf_wayland_pointer_constraints);  gdmf_wayland_pointer_constraints = NULL; }
    if (gdmf_wayland_decoration_manager) { zxdg_decoration_manager_v1_destroy(gdmf_wayland_decoration_manager);   gdmf_wayland_decoration_manager = NULL; }
    if (gdmf_wayland_seat)              { wl_seat_destroy(gdmf_wayland_seat);                                gdmf_wayland_seat = NULL; }
    if (gdmf_wayland_shared_memory)               { wl_shm_destroy(gdmf_wayland_shared_memory);                                  gdmf_wayland_shared_memory = NULL; }
    if (gdmf_wayland_shell)            { xdg_wm_base_destroy(gdmf_wayland_shell);                          gdmf_wayland_shell = NULL; }
    if (gdmf_wayland_compositor)        { wl_compositor_destroy(gdmf_wayland_compositor);                    gdmf_wayland_compositor = NULL; }
    if (gdmf_wayland_registry)          { wl_registry_destroy(gdmf_wayland_registry);                        gdmf_wayland_registry = NULL; }

    if (gdmf_wayland_display_connection) {
        wl_display_flush(gdmf_wayland_display_connection);
        wl_display_disconnect(gdmf_wayland_display_connection);
        gdmf_wayland_display_connection = NULL;
    }

    gdmf_wayland_initial_configure_seen = false;

    return;
}

// Flush outgoing requests and check connection health; do not dispatch here.
static void gdmf_wayland_pump(void) {
    if (!gdmf_wayland_display_connection) { return; }

    if (wl_display_get_error(gdmf_wayland_display_connection) != 0) {
        if (!gdmf_window_close_requested) {
            printf("[GDMF] Wayland connection lost (compositor gone or "
                   "protocol error); shutting down\n");
            gdmf_window_close_requested = true;
        }
        return;
    }

    wl_display_flush(gdmf_wayland_display_connection);

    return;
}

static void gdmf_wayland_request_display_mode(GDMF_DisplayMode mode) {
    if (!gdmf_wayland_toplevel) { return; }

    gdmf_wayland_requested_mode = mode;

    if (mode == GDMF_MODE_WINDOWED) {
        xdg_toplevel_unset_fullscreen(gdmf_wayland_toplevel);
    } else {

        xdg_toplevel_set_fullscreen(gdmf_wayland_toplevel, NULL);
    }

    return;
}

static void gdmf_wayland_input_state_changed(void) {
    gdmf_wayland_apply_input_state();

    return;
}

// Returns the last surface-local position, even after the pointer leaves.
static bool gdmf_wayland_get_mouse_client(int* mouse_x, int* mouse_y) {
    if (!gdmf_wayland_surface) { return false; }

    *mouse_x = gdmf_wayland_last_mouse_x;
    *mouse_y = gdmf_wayland_last_mouse_y;

    return true;
}

void* GDMF_GetWaylandDisplay(void) { return (void*)gdmf_wayland_display_connection; }
void* GDMF_GetWaylandSurface(void) { return (void*)gdmf_wayland_surface; }

static FuselageLinuxNativeHandle gdmf_wayland_native_handle;
static void* gdmf_wayland_get_native_handle(void) {
    gdmf_wayland_native_handle.backend_name = "wayland";
    gdmf_wayland_native_handle.display_connection = (void*)gdmf_wayland_display_connection;
    gdmf_wayland_native_handle.window_id = NULL;

    return (void*)&gdmf_wayland_native_handle;
}

const gdmf_linux_window_backend gdmf_linux_window_backend_wayland = {
    .name = "wayland",
    .create = gdmf_wayland_create_window,
    .destroy = gdmf_wayland_destroy_window,
    .pump = gdmf_wayland_pump,
    .request_display_mode = gdmf_wayland_request_display_mode,
    .input_state_changed = gdmf_wayland_input_state_changed,
    .get_mouse_client = gdmf_wayland_get_mouse_client,
    .get_native_handle = gdmf_wayland_get_native_handle,
};
