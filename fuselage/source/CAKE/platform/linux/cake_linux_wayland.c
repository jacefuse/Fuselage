// Wayland seat intake. SEE: (CAKE.md, Linux input)
#include "cake_linux_private.h"
#if defined(__linux__)
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/input.h>
#include <wayland-client.h>
#include "relative-pointer-unstable-v1-client-protocol.h"
static struct wl_display* cake_linux_wayland_display;
static struct wl_event_queue* cake_linux_wayland_queue;
static struct wl_registry* cake_linux_wayland_registry;
static struct wl_seat* cake_linux_wayland_seat;
static struct wl_keyboard* cake_linux_wayland_keyboard;
static struct wl_pointer* cake_linux_wayland_pointer;
static struct zwp_relative_pointer_manager_v1* cake_linux_wayland_relative_manager;
static struct zwp_relative_pointer_v1* cake_linux_wayland_relative_pointer;
static bool cake_linux_wayland_wheel_discrete_seen;
static uint32_t cake_linux_wayland_seat_global_name;
static uint32_t cake_linux_wayland_manager_name;
static void cake_linux_wayland_keyboard_keymap(void* data, struct wl_keyboard* keyboard, uint32_t format,
                                               int32_t keymap_descriptor, uint32_t size) {
    (void)data;
    (void)keyboard;
    (void)format;
    (void)size;

    close(keymap_descriptor);

    return;
}

static void cake_linux_wayland_keyboard_enter(void* data, struct wl_keyboard* keyboard, uint32_t serial,
                                              struct wl_surface* surface, struct wl_array* keys) {
    (void)data;
    (void)keyboard;
    (void)serial;
    (void)surface;
    if (!keys || keys->size % sizeof(uint32_t) || (keys->size && !keys->data)) {
        return;
    }
    cake_linux_resync_keys(keys->data, keys->size / sizeof(uint32_t));
}

static void cake_linux_wayland_keyboard_leave(void* data, struct wl_keyboard* keyboard, uint32_t serial,
                                              struct wl_surface* surface) {
    (void)data;
    (void)keyboard;
    (void)serial;
    (void)surface;
    cake_linux_resync_keys(NULL, 0);
}

static void cake_linux_wayland_keyboard_key(void* data, struct wl_keyboard* keyboard, uint32_t serial,
                                            uint32_t time, uint32_t key_code, uint32_t state) {
    (void)data;
    (void)keyboard;
    (void)serial;
    (void)time;
    cake_linux_intake_key(key_code, state == WL_KEYBOARD_KEY_STATE_PRESSED);
}

static void cake_linux_wayland_keyboard_modifiers(void* data, struct wl_keyboard* keyboard, uint32_t serial,
                                                  uint32_t depressed, uint32_t latched, uint32_t locked,
                                                  uint32_t group) {
    (void)data;
    (void)keyboard;
    (void)serial;
    (void)depressed;
    (void)latched;
    (void)locked;
    (void)group;

    return;
}

static void cake_linux_wayland_keyboard_repeat_info(void* data, struct wl_keyboard* keyboard, int32_t rate,
                                                    int32_t delay) {
    (void)data;
    (void)keyboard;
    (void)rate;
    (void)delay;
    return;
}

static const struct wl_keyboard_listener cake_linux_wayland_keyboard_listener = {
    .keymap = cake_linux_wayland_keyboard_keymap,
    .enter = cake_linux_wayland_keyboard_enter,
    .leave = cake_linux_wayland_keyboard_leave,
    .key = cake_linux_wayland_keyboard_key,
    .modifiers = cake_linux_wayland_keyboard_modifiers,
    .repeat_info = cake_linux_wayland_keyboard_repeat_info,
};

static void cake_linux_wayland_pointer_enter(void* data, struct wl_pointer* pointer, uint32_t serial,
                                             struct wl_surface* surface, wl_fixed_t surface_x,
                                             wl_fixed_t surface_y) {
    (void)data;
    (void)pointer;
    (void)serial;
    (void)surface;
    (void)surface_x;
    (void)surface_y;
    return;
}

static void cake_linux_wayland_pointer_leave(void* data, struct wl_pointer* pointer, uint32_t serial,
                                             struct wl_surface* surface) {
    // Preserve held buttons through the implicit grab. SEE: (CAKE.md, Linux input)
    (void)data;
    (void)pointer;
    (void)serial;
    (void)surface;

    return;
}

static void cake_linux_wayland_pointer_motion(void* data, struct wl_pointer* pointer, uint32_t time,
                                              wl_fixed_t surface_x, wl_fixed_t surface_y) {
    // Absolute surface positions are not raw deltas. SEE: (CAKE.md, Linux input)
    (void)data;
    (void)pointer;
    (void)time;
    (void)surface_x;
    (void)surface_y;

    return;
}

static void cake_linux_wayland_relative_motion(void* data, struct zwp_relative_pointer_v1* relative_pointer,
                                               uint32_t time_high, uint32_t time_low, wl_fixed_t delta_x,
                                               wl_fixed_t delta_y, wl_fixed_t unaccelerated_x,
                                               wl_fixed_t unaccelerated_y) {
    (void)data;
    (void)relative_pointer;
    (void)time_high;
    (void)time_low;
    (void)delta_x;
    (void)delta_y;
    cake_linux_intake_motion(wl_fixed_to_double(unaccelerated_x), wl_fixed_to_double(unaccelerated_y));
}

static const struct zwp_relative_pointer_v1_listener cake_linux_wayland_relative_listener = {
    .relative_motion = cake_linux_wayland_relative_motion,
};

static void cake_linux_wayland_pointer_button(void* data, struct wl_pointer* pointer, uint32_t serial,
                                              uint32_t time, uint32_t button, uint32_t state) {
    (void)data;
    (void)pointer;
    (void)serial;
    (void)time;
    int button_index;
    switch (button) {
    case BTN_LEFT:
        button_index = CAKE_MOUSE_LEFT;
        break;
    case BTN_RIGHT:
        button_index = CAKE_MOUSE_RIGHT;
        break;
    case BTN_MIDDLE:
        button_index = CAKE_MOUSE_MIDDLE;
        break;
    case BTN_SIDE:
        button_index = CAKE_MOUSE_X1;
        break;
    case BTN_EXTRA:
        button_index = CAKE_MOUSE_X2;
        break;
    default:
        return;
    }
    cake_linux_intake_button(button_index, state == WL_POINTER_BUTTON_STATE_PRESSED);
}

static void cake_linux_wayland_pointer_axis(void* data, struct wl_pointer* pointer, uint32_t time,
                                            uint32_t axis, wl_fixed_t value) {
    (void)data;
    (void)pointer;
    (void)time;
    if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL || cake_linux_wayland_wheel_discrete_seen) {
        return;
    }
    cake_linux_intake_scroll(-wl_fixed_to_double(value) * 8.0);
}

static void cake_linux_wayland_pointer_axis_discrete(void* data, struct wl_pointer* pointer, uint32_t axis,
                                                     int32_t discrete) {
    (void)data;
    (void)pointer;
    if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL) {
        return;
    }
    cake_linux_wayland_wheel_discrete_seen = true;
    cake_linux_intake_scroll(-(double)discrete * 120.0);
}

static void cake_linux_wayland_pointer_frame(void* data, struct wl_pointer* pointer) {
    (void)data;
    (void)pointer;
    cake_linux_wayland_wheel_discrete_seen = false;

    return;
}

static void cake_linux_wayland_pointer_axis_source(void* data, struct wl_pointer* pointer, uint32_t source) {
    (void)data;
    (void)pointer;
    (void)source;
    return;
}

static void cake_linux_wayland_pointer_axis_stop(void* data, struct wl_pointer* pointer, uint32_t time,
                                                 uint32_t axis) {
    (void)data;
    (void)pointer;
    (void)time;
    (void)axis;
    return;
}

static const struct wl_pointer_listener cake_linux_wayland_pointer_listener = {
    .enter = cake_linux_wayland_pointer_enter,
    .leave = cake_linux_wayland_pointer_leave,
    .motion = cake_linux_wayland_pointer_motion,
    .button = cake_linux_wayland_pointer_button,
    .axis = cake_linux_wayland_pointer_axis,
    .frame = cake_linux_wayland_pointer_frame,
    .axis_source = cake_linux_wayland_pointer_axis_source,
    .axis_stop = cake_linux_wayland_pointer_axis_stop,
    .axis_discrete = cake_linux_wayland_pointer_axis_discrete,
};

static void cake_linux_wayland_ensure_relative_pointer(void) {
    if (cake_linux_wayland_relative_manager && cake_linux_wayland_pointer &&
        !cake_linux_wayland_relative_pointer) {
        cake_linux_wayland_relative_pointer = zwp_relative_pointer_manager_v1_get_relative_pointer(
            cake_linux_wayland_relative_manager, cake_linux_wayland_pointer);
        if (cake_linux_wayland_relative_pointer &&
            zwp_relative_pointer_v1_add_listener(cake_linux_wayland_relative_pointer,
                                                 &cake_linux_wayland_relative_listener, NULL) < 0) {
            zwp_relative_pointer_v1_destroy(cake_linux_wayland_relative_pointer);
            cake_linux_wayland_relative_pointer = NULL;
        }
    }
}
static void cake_linux_wayland_drop_keyboard(void) {
    if (cake_linux_wayland_keyboard) {
        wl_keyboard_destroy(cake_linux_wayland_keyboard);
        cake_linux_wayland_keyboard = NULL;
    }
    cake_linux_resync_keys(NULL, 0);
}
static void cake_linux_wayland_drop_pointer(void) {
    if (cake_linux_wayland_relative_pointer) {
        zwp_relative_pointer_v1_destroy(cake_linux_wayland_relative_pointer);
        cake_linux_wayland_relative_pointer = NULL;
    }
    if (cake_linux_wayland_pointer) {
        wl_pointer_destroy(cake_linux_wayland_pointer);
        cake_linux_wayland_pointer = NULL;
    }
    cake_linux_wayland_wheel_discrete_seen = false;
    cake_linux_release_buttons();
}
static void cake_linux_wayland_seat_capabilities(void* data, struct wl_seat* seat, uint32_t capabilities) {
    (void)data;
    if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && !cake_linux_wayland_keyboard) {
        cake_linux_wayland_keyboard = wl_seat_get_keyboard(seat);
        if (cake_linux_wayland_keyboard &&
            wl_keyboard_add_listener(cake_linux_wayland_keyboard, &cake_linux_wayland_keyboard_listener,
                                     NULL) < 0) {
            cake_linux_wayland_drop_keyboard();
        }
    }
    if (!(capabilities & WL_SEAT_CAPABILITY_KEYBOARD)) {
        cake_linux_wayland_drop_keyboard();
    }
    if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !cake_linux_wayland_pointer) {
        cake_linux_wayland_pointer = wl_seat_get_pointer(seat);
        if (cake_linux_wayland_pointer &&
            wl_pointer_add_listener(cake_linux_wayland_pointer, &cake_linux_wayland_pointer_listener, NULL) <
                0) {
            cake_linux_wayland_drop_pointer();
        }
    }
    if (!(capabilities & WL_SEAT_CAPABILITY_POINTER)) {
        cake_linux_wayland_drop_pointer();
    }
    cake_linux_wayland_ensure_relative_pointer();
}

static void cake_linux_wayland_seat_name(void* data, struct wl_seat* seat, const char* name) {
    (void)data;
    (void)seat;
    (void)name;
    return;
}

static const struct wl_seat_listener cake_linux_wayland_seat_listener = {
    .capabilities = cake_linux_wayland_seat_capabilities,
    .name = cake_linux_wayland_seat_name,
};

static void cake_linux_wayland_registry_global(void* data, struct wl_registry* registry, uint32_t name,
                                               const char* interface_name, uint32_t version) {
    (void)data;
    if (!version) {
        return;
    }
    if (strcmp(interface_name, wl_seat_interface.name) == 0 && !cake_linux_wayland_seat) {
        cake_linux_wayland_seat =
            wl_registry_bind(registry, name, &wl_seat_interface, version < 5 ? version : 5);
        if (cake_linux_wayland_seat) {
            cake_linux_wayland_seat_global_name = name;
            if (wl_seat_add_listener(cake_linux_wayland_seat, &cake_linux_wayland_seat_listener, NULL) < 0) {
                wl_seat_destroy(cake_linux_wayland_seat);
                cake_linux_wayland_seat = NULL;
                cake_linux_wayland_seat_global_name = 0;
            }
        }
    } else if (strcmp(interface_name, zwp_relative_pointer_manager_v1_interface.name) == 0 &&
               !cake_linux_wayland_relative_manager) {
        cake_linux_wayland_relative_manager =
            wl_registry_bind(registry, name, &zwp_relative_pointer_manager_v1_interface, 1);
        if (cake_linux_wayland_relative_manager) {
            cake_linux_wayland_manager_name = name;
            cake_linux_wayland_ensure_relative_pointer();
        }
    }
}

static void cake_linux_wayland_registry_global_remove(void* data, struct wl_registry* registry,
                                                      uint32_t name) {
    (void)data;
    (void)registry;
    if (cake_linux_wayland_seat && name == cake_linux_wayland_seat_global_name) {
        cake_linux_wayland_drop_keyboard();
        cake_linux_wayland_drop_pointer();
        wl_seat_destroy(cake_linux_wayland_seat);
        cake_linux_wayland_seat = NULL;
        cake_linux_wayland_seat_global_name = 0;
    }
    if (cake_linux_wayland_relative_manager && name == cake_linux_wayland_manager_name) {
        if (cake_linux_wayland_relative_pointer) {
            zwp_relative_pointer_v1_destroy(cake_linux_wayland_relative_pointer);
            cake_linux_wayland_relative_pointer = NULL;
        }
        zwp_relative_pointer_manager_v1_destroy(cake_linux_wayland_relative_manager);
        cake_linux_wayland_relative_manager = NULL;
        cake_linux_wayland_manager_name = 0;
    }
}

static const struct wl_registry_listener cake_linux_wayland_registry_listener = {
    .global = cake_linux_wayland_registry_global,
    .global_remove = cake_linux_wayland_registry_global_remove,
};

void cake_linux_wayland_detach(void) {
    if (!cake_linux_wayland_display) {
        return;
    }
    cake_linux_wayland_drop_keyboard();
    cake_linux_wayland_drop_pointer();
    if (cake_linux_wayland_relative_manager) {
        zwp_relative_pointer_manager_v1_destroy(cake_linux_wayland_relative_manager);
        cake_linux_wayland_relative_manager = NULL;
    }
    if (cake_linux_wayland_seat) {
        wl_seat_destroy(cake_linux_wayland_seat);
        cake_linux_wayland_seat = NULL;
    }
    if (cake_linux_wayland_registry) {
        wl_registry_destroy(cake_linux_wayland_registry);
        cake_linux_wayland_registry = NULL;
    }
    if (cake_linux_wayland_queue) {
        wl_event_queue_destroy(cake_linux_wayland_queue);
        cake_linux_wayland_queue = NULL;
    }
    cake_linux_wayland_display = NULL; // Borrowed; the display owner disconnects it after CAKE stops.
    cake_linux_wayland_seat_global_name = cake_linux_wayland_manager_name = 0;
}
bool cake_linux_wayland_attach(void* display_connection) {
    if (!display_connection || cake_linux_wayland_display) {
        return false;
    }
    cake_linux_wayland_display = display_connection;
    cake_linux_wayland_queue = wl_display_create_queue(cake_linux_wayland_display);
    if (!cake_linux_wayland_queue) {
        goto failure;
    }
    // Children inherit this private queue at creation, before another reader can see them.
    struct wl_display* wrapper = wl_proxy_create_wrapper(cake_linux_wayland_display);
    if (!wrapper) {
        goto failure;
    }
    wl_proxy_set_queue((struct wl_proxy*)wrapper, cake_linux_wayland_queue);
    cake_linux_wayland_registry = wl_display_get_registry(wrapper);
    wl_proxy_wrapper_destroy(wrapper);
    if (!cake_linux_wayland_registry ||
        wl_registry_add_listener(cake_linux_wayland_registry, &cake_linux_wayland_registry_listener, NULL) <
            0) {
        goto failure;
    }
    if (wl_display_roundtrip_queue(cake_linux_wayland_display, cake_linux_wayland_queue) < 0 ||
        wl_display_roundtrip_queue(cake_linux_wayland_display, cake_linux_wayland_queue) < 0) {
        goto failure;
    }
    if (!cake_linux_wayland_seat) {
        fprintf(stderr, "[CAKE] No Wayland seat; keyboard/mouse unavailable\n");
    }
    if (!cake_linux_wayland_relative_manager) {
        fprintf(stderr, "[CAKE] No relative-pointer protocol; mouse deltas unavailable\n");
    }
    return true;
failure:
    cake_linux_wayland_detach();
    return false;
}
void cake_linux_wayland_drain(void) {
    if (cake_linux_wayland_queue &&
        wl_display_dispatch_queue_pending(cake_linux_wayland_display, cake_linux_wayland_queue) < 0) {
        fprintf(stderr, "[CAKE] Wayland input dispatch failed\n");
        cake_linux_wayland_detach();
    }
}
#endif
