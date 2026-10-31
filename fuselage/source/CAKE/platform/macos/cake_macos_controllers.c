// Controller slots and value conversion. SEE: (CAKE.md, macOS input and controllers)
#include "cake_macos_private.h"
#if defined(__APPLE__)
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
typedef struct {
    void* controller_object;
    char name[CAKE_CONTROLLER_NAME_MAX];
    bool present;
    uint64_t lost_at;
    CAKE_ControllerState state[2];
    volatile int front_buffer;

} cake_macos_controller;

static bool cake_macos_accept_controllers;
static cake_macos_controller cake_macos_controllers[CAKE_CONTROLLER_MAX];

static CAKE_ControllerConnectionState cake_macos_connection_states[CAKE_CONTROLLER_MAX];

static uint64_t cake_macos_now_ms(void) {
    struct timespec sample = {0};
    if (clock_gettime(CLOCK_MONOTONIC, &sample) != 0 || sample.tv_sec < 0 || sample.tv_nsec < 0 ||
        sample.tv_nsec >= 1000000000 || (uint64_t)sample.tv_sec > (UINT64_MAX - 999) / 1000) {
        return 1;
    }

    uint64_t milliseconds = (uint64_t)sample.tv_sec * 1000ULL + (uint64_t)sample.tv_nsec / 1000000ULL;
    return milliseconds ? milliseconds : 1;
}

bool cake_macos_controller_connected(void* controller_object, const char* name) {
    pthread_mutex_lock(&cake_macos_intake_lock);

    if (!cake_macos_accept_controllers || !controller_object || !name) {
        pthread_mutex_unlock(&cake_macos_intake_lock);
        return false;
    }
    for (int index = 0; index < CAKE_CONTROLLER_MAX; ++index) {
        if (cake_macos_controllers[index].present &&
            cake_macos_controllers[index].controller_object == controller_object) {
            pthread_mutex_unlock(&cake_macos_intake_lock);
            return true;
        }
    }
    int slot = -1;
    for (int slot_index = 0; slot_index < CAKE_CONTROLLER_MAX; slot_index++) {
        if (!cake_macos_controllers[slot_index].present && cake_macos_controllers[slot_index].lost_at != 0 &&
            strncmp(cake_macos_controllers[slot_index].name, name, CAKE_CONTROLLER_NAME_MAX) == 0) {
            slot = slot_index;
            break;
        }
    }
    for (int slot_index = 0; slot < 0 && slot_index < CAKE_CONTROLLER_MAX; slot_index++) {
        if (!cake_macos_controllers[slot_index].present && cake_macos_controllers[slot_index].lost_at == 0) {
            slot = slot_index;
        }
    }

    if (slot >= 0) {
        cake_macos_controller* controller = &cake_macos_controllers[slot];
        memset(controller->state, 0, sizeof(controller->state));
        controller->controller_object = controller_object;
        controller->present = true;
        controller->lost_at = 0;
        controller->front_buffer = 0;
        snprintf(controller->name, sizeof(controller->name), "%s", name);
        cake_macos_connection_states[slot] = CAKE_CONN_ATTACHED;
        printf("[CAKE] Controller attached  | %s | slot %d\n", controller->name, slot);
    }

    pthread_mutex_unlock(&cake_macos_intake_lock);

    return slot >= 0;
}

void cake_macos_controller_disconnected(void* controller_object) {
    pthread_mutex_lock(&cake_macos_intake_lock);

    for (int slot_index = 0; slot_index < CAKE_CONTROLLER_MAX; slot_index++) {
        if (cake_macos_controllers[slot_index].present &&
            cake_macos_controllers[slot_index].controller_object == controller_object) {
            cake_macos_controllers[slot_index].present = false;
            cake_macos_controllers[slot_index].controller_object = NULL;
            cake_macos_controllers[slot_index].lost_at = cake_macos_now_ms();
            cake_macos_connection_states[slot_index] = CAKE_CONN_LOST;
            printf("[CAKE] Controller detached  | %s | slot %d\n", cake_macos_controllers[slot_index].name,
                   slot_index);
            break;
        }
    }

    pthread_mutex_unlock(&cake_macos_intake_lock);

    return;
}

void cake_macos_poll_controllers(void) {
    uint64_t timestamp = cake_macos_now_ms();

    pthread_mutex_lock(&cake_macos_intake_lock);

    for (int slot_index = 0; slot_index < CAKE_CONTROLLER_MAX; slot_index++) {
        cake_macos_controller* controller = &cake_macos_controllers[slot_index];

        if (controller->present) {
            int back_buffer = 1 - controller->front_buffer;
            if (!cake_macos_read_controller(controller->controller_object, &controller->state[back_buffer])) {
                memset(&controller->state[back_buffer], 0, sizeof(controller->state[back_buffer]));
            }
            controller->front_buffer = back_buffer;
        } else if (controller->lost_at != 0 && timestamp >= controller->lost_at &&
                   timestamp - controller->lost_at > CAKE_CONTROLLER_TIMEOUT_MS) {
            cake_macos_connection_states[slot_index] = CAKE_CONN_TIMEDOUT;
            printf("[CAKE] Controller timed out | %s | slot %d\n", controller->name, slot_index);
            memset(controller, 0, sizeof(*controller));
        }
    }

    pthread_mutex_unlock(&cake_macos_intake_lock);

    return;
}

bool CAKE_IsControllerConnected(int slot) {
    if (slot < 0 || slot >= CAKE_CONTROLLER_MAX) {
        return false;
    }

    return cake_macos_controllers[slot].present;
}

CAKE_ControllerConnectionState CAKE_GetControllerConnectionState(int slot) {
    if (slot < 0 || slot >= CAKE_CONTROLLER_MAX) {
        return CAKE_CONN_EMPTY;
    }

    return cake_macos_connection_states[slot];
}

const CAKE_ControllerState* CAKE_GetControllerState(int slot) {
    if (!CAKE_IsControllerConnected(slot)) {
        return NULL;
    }

    return &cake_macos_controllers[slot].state[cake_macos_controllers[slot].front_buffer];
}

const char* CAKE_GetControllerName(int slot) {
    if (!CAKE_IsControllerConnected(slot)) {
        return NULL;
    }

    return cake_macos_controllers[slot].name;
}

CAKE_ControllerBackend CAKE_GetControllerBackend(int slot) {
    if (!CAKE_IsControllerConnected(slot)) {
        return CAKE_BACKEND_UNKNOWN;
    }

    return CAKE_BACKEND_HID;
}

uint16_t CAKE_GetControllerVendorID(int slot) {
    (void)slot;

    return 0;
}

uint16_t CAKE_GetControllerProductID(int slot) {
    (void)slot;

    return 0;
}

int CAKE_GetControllerXInputIndex(int slot) {
    (void)slot;

    return -1;
}

bool CAKE_SetControllerVibration(int slot, uint16_t left_motor, uint16_t right_motor) {
    (void)slot;
    (void)left_motor;
    (void)right_motor;

    return false;
}

void cake_macos_enable_controllers(void) {
    pthread_mutex_lock(&cake_macos_intake_lock);
    cake_macos_accept_controllers = true;
    pthread_mutex_unlock(&cake_macos_intake_lock);
}
void cake_macos_clear_controllers(void) {
    pthread_mutex_lock(&cake_macos_intake_lock);
    cake_macos_accept_controllers = false;
    memset(cake_macos_controllers, 0, sizeof(cake_macos_controllers));
    memset(cake_macos_connection_states, 0, sizeof(cake_macos_connection_states));
    pthread_mutex_unlock(&cake_macos_intake_lock);
}
int16_t cake_macos_convert_axis(float value) {
    if (!isfinite(value)) {
        return 0;
    }
    float scaled = value * 32767.0f;
    if (scaled > 32767.0f) {
        scaled = 32767.0f;
    }
    if (scaled < -32768.0f) {
        scaled = -32768.0f;
    }
    return (int16_t)scaled;
}
uint8_t cake_macos_convert_trigger(float value) {
    if (!isfinite(value) || value <= 0) {
        return 0;
    }
    if (value >= 1) {
        return 255;
    }
    return (uint8_t)(value * 255.0f);
}

#endif
