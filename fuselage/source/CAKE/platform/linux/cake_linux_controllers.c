// evdev slots, polling and vibration. SEE: (CAKE.md, Linux controllers)
#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "cake_linux_controller_private.h"
#if defined(__linux__)
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
const int cake_linux_axes[CAKE_LINUX_AXIS_COUNT] = {ABS_X, ABS_Y,  ABS_RX,    ABS_RY,
                                                    ABS_Z, ABS_RZ, ABS_HAT0X, ABS_HAT0Y};
cake_linux_controller cake_linux_controllers[CAKE_CONTROLLER_MAX];
CAKE_ControllerConnectionState cake_linux_connection_states[CAKE_CONTROLLER_MAX];
static uint64_t cake_linux_now_ms(void) {
    struct timespec sample = {0};
    if (clock_gettime(CLOCK_MONOTONIC, &sample) != 0 || sample.tv_sec < 0 || sample.tv_nsec < 0 ||
        sample.tv_nsec >= 1000000000 || (uint64_t)sample.tv_sec > (UINT64_MAX - 999) / 1000) {
        return 1;
    }

    uint64_t milliseconds = (uint64_t)sample.tv_sec * 1000ULL + (uint64_t)sample.tv_nsec / 1000000ULL;
    return milliseconds ? milliseconds : 1;
}
bool cake_linux_adopt_controller(int descriptor, const char* path, bool writable) {
    char name[CAKE_CONTROLLER_NAME_MAX] = "Gamepad";
    ioctl(descriptor, EVIOCGNAME(sizeof(name)), name);
    name[sizeof(name) - 1] = '\0';

    int slot = -1;
    for (int index = 0; index < CAKE_CONTROLLER_MAX; index++) {
        if (!cake_linux_controllers[index].present && cake_linux_controllers[index].lost_at != 0 &&
            strncmp(cake_linux_controllers[index].name, name, CAKE_CONTROLLER_NAME_MAX) == 0) {
            slot = index;
            break;
        }
    }
    for (int index = 0; slot < 0 && index < CAKE_CONTROLLER_MAX; index++) {
        if (!cake_linux_controllers[index].present && cake_linux_controllers[index].lost_at == 0) {
            slot = index;
        }
    }
    if (slot < 0) {
        return false;
    }

    cake_linux_controller* controller = &cake_linux_controllers[slot];
    memset(controller, 0, sizeof(*controller));
    controller->descriptor = descriptor;
    controller->present = true;
    snprintf(controller->path, sizeof(controller->path), "%s", path);
    snprintf(controller->name, sizeof(controller->name), "%s", name);

    struct input_id device_id;
    if (ioctl(descriptor, EVIOCGID, &device_id) == 0) {
        controller->vendor_id = device_id.vendor;
        controller->product_id = device_id.product;
    }

    for (int index = 0; index < CAKE_LINUX_AXIS_COUNT; index++) {
        controller->has_axis[index] =
            (ioctl(descriptor, EVIOCGABS(cake_linux_axes[index]), &controller->axis_info[index]) == 0 &&
             controller->axis_info[index].maximum > controller->axis_info[index].minimum);
    }

    unsigned long feedback_bits[(FF_MAX / (8 * sizeof(unsigned long))) + 1] = {0};
    controller->has_rumble =
        writable && (ioctl(descriptor, EVIOCGBIT(EV_FF, sizeof(feedback_bits)), feedback_bits) >= 0 &&
                     cake_linux_test_bit(feedback_bits, FF_RUMBLE));
    controller->effect_id = -1;

    cake_linux_connection_states[slot] = CAKE_CONN_ATTACHED;
    printf("[CAKE] Controller attached  | %s | slot %d%s\n", controller->name, slot,
           controller->has_rumble ? "" : " (no rumble)");

    return true;
}
void cake_linux_poll_controllers(void) {
    uint64_t now = cake_linux_now_ms();

    cake_linux_refresh_discovery(now);

    for (int index = 0; index < CAKE_CONTROLLER_MAX; index++) {
        cake_linux_controller* controller = &cake_linux_controllers[index];

        if (controller->present) {
            struct input_event event;
            ssize_t bytes_read;
            bool disconnected = false;

            while ((bytes_read = read(controller->descriptor, &event, sizeof(event))) ==
                   (ssize_t)sizeof(event)) {
                // Queue-overflow resynchronization remains pending. SEE: (CAKE.md, Linux controllers)
                cake_linux_apply_controller_event(controller, &event);
            }
            if (bytes_read != -1 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
                disconnected = true;
            }

            if (disconnected) {
                close(controller->descriptor);
                controller->descriptor = -1;
                controller->present = false;
                controller->lost_at = now;
                cake_linux_connection_states[index] = CAKE_CONN_LOST;
                printf("[CAKE] Controller detached  | %s | slot %d\n", controller->name, index);
            } else {
                int back_buffer = 1 - controller->front_buffer;
                controller->state[back_buffer] = controller->live;
                controller->front_buffer = back_buffer;
            }
        } else if (controller->lost_at != 0 && now >= controller->lost_at &&
                   now - controller->lost_at > CAKE_CONTROLLER_TIMEOUT_MS) {
            cake_linux_connection_states[index] = CAKE_CONN_TIMEDOUT;
            printf("[CAKE] Controller timed out | %s | slot %d\n", controller->name, index);
            memset(controller, 0, sizeof(*controller));
            controller->descriptor = -1;

            cake_linux_slot_freed = true;
        }
    }

    return;
}

void cake_linux_initialize_controllers(void) {
    for (int index = 0; index < CAKE_CONTROLLER_MAX; ++index) {
        cake_linux_controllers[index].descriptor = -1;
    }
    cake_linux_initialize_discovery();
}
void cake_linux_shutdown_controllers(void) {
    for (int index = 0; index < CAKE_CONTROLLER_MAX; ++index) {
        if (cake_linux_controllers[index].descriptor >= 0) {
            close(cake_linux_controllers[index].descriptor);
        }
        memset(&cake_linux_controllers[index], 0, sizeof(cake_linux_controllers[index]));
        cake_linux_controllers[index].descriptor = -1;
        cake_linux_connection_states[index] = CAKE_CONN_EMPTY;
    }
    cake_linux_shutdown_discovery();
}
bool CAKE_IsControllerConnected(int slot) {
    if (slot < 0 || slot >= CAKE_CONTROLLER_MAX) {
        return false;
    }

    return cake_linux_controllers[slot].present;
}

CAKE_ControllerConnectionState CAKE_GetControllerConnectionState(int slot) {
    if (slot < 0 || slot >= CAKE_CONTROLLER_MAX) {
        return CAKE_CONN_EMPTY;
    }

    return cake_linux_connection_states[slot];
}

const CAKE_ControllerState* CAKE_GetControllerState(int slot) {
    if (!CAKE_IsControllerConnected(slot)) {
        return NULL;
    }

    return &cake_linux_controllers[slot].state[cake_linux_controllers[slot].front_buffer];
}

const char* CAKE_GetControllerName(int slot) {
    if (!CAKE_IsControllerConnected(slot)) {
        return NULL;
    }

    return cake_linux_controllers[slot].name;
}

CAKE_ControllerBackend CAKE_GetControllerBackend(int slot) {
    if (!CAKE_IsControllerConnected(slot)) {
        return CAKE_BACKEND_UNKNOWN;
    }

    return CAKE_BACKEND_HID;
}

uint16_t CAKE_GetControllerVendorID(int slot) {
    if (!CAKE_IsControllerConnected(slot)) {
        return 0;
    }

    return cake_linux_controllers[slot].vendor_id;
}

uint16_t CAKE_GetControllerProductID(int slot) {
    if (!CAKE_IsControllerConnected(slot)) {
        return 0;
    }

    return cake_linux_controllers[slot].product_id;
}

int CAKE_GetControllerXInputIndex(int slot) {
    (void)slot;

    return -1;
}

bool CAKE_SetControllerVibration(int slot, uint16_t left_motor, uint16_t right_motor) {
    if (!CAKE_IsControllerConnected(slot)) {
        return false;
    }

    cake_linux_controller* controller = &cake_linux_controllers[slot];
    if (!controller->has_rumble || controller->descriptor < 0) {
        return false;
    }

    struct ff_effect effect;
    memset(&effect, 0, sizeof(effect));
    effect.type = FF_RUMBLE;
    effect.id = controller->effect_id;
    effect.u.rumble.strong_magnitude = left_motor;
    effect.u.rumble.weak_magnitude = right_motor;
    effect.replay.length = 0;

    if (ioctl(controller->descriptor, EVIOCSFF, &effect) < 0) {
        return false;
    }
    controller->effect_id = effect.id;

    struct input_event play_event;
    memset(&play_event, 0, sizeof(play_event));
    play_event.type = EV_FF;
    play_event.code = (uint16_t)controller->effect_id;
    play_event.value = (left_motor || right_motor) ? 1 : 0;

    return write(controller->descriptor, &play_event, sizeof(play_event)) == (ssize_t)sizeof(play_event);
}

#endif
