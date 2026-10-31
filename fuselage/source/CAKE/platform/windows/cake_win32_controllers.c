// Controller storage, publication and queries. SEE: (CAKE.md, Windows controllers)
#include "cake_win32_controller_private.h"
#if defined(_WIN32)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

cake_win32_controller cake_win32_controllers[CAKE_CONTROLLER_MAX];
CAKE_ControllerConnectionState cake_win32_connection_states[CAKE_CONTROLLER_MAX];
bool cake_win32_hid_trace;

static void cake_win32_poll_hid(cake_win32_controller* controller) {
    int back_buffer = 1 - controller->front_buffer;
    controller->state[back_buffer] = controller->hid.live;
    controller->front_buffer = back_buffer;

    return;
}

void cake_win32_poll_controllers(void) {
    for (int slot = 0; slot < CAKE_CONTROLLER_MAX; slot++) {
        cake_win32_controller* controller = &cake_win32_controllers[slot];

        if (controller->device_path[0] == '\0') {
            continue;
        }
        if (!controller->identity_valid) {
            continue;
        }

        switch (controller->identity.backend) {
        case CAKE_BACKEND_XINPUT:
            cake_win32_poll_xinput(controller);

            break;
        case CAKE_BACKEND_HID:
            cake_win32_poll_hid(controller);

            break;
        default:
            break;
        }
    }

    return;
}

bool CAKE_IsControllerConnected(int slot) {
    if (slot < 0 || slot >= CAKE_CONTROLLER_MAX) {
        return false;
    }
    if (cake_win32_controllers[slot].device_path[0] == '\0') {
        return false;
    }
    if (!cake_win32_controllers[slot].identity_valid) {
        return false;
    }

    return true;
}

const CAKE_ControllerState* CAKE_GetControllerState(int slot) {
    if (!CAKE_IsControllerConnected(slot)) {
        return NULL;
    }

    return &cake_win32_controllers[slot].state[cake_win32_controllers[slot].front_buffer];
}

const char* CAKE_GetControllerName(int slot) {
    if (!CAKE_IsControllerConnected(slot)) {
        return NULL;
    }

    return cake_win32_controllers[slot].name;
}

CAKE_ControllerBackend CAKE_GetControllerBackend(int slot) {
    if (!CAKE_IsControllerConnected(slot)) {
        return CAKE_BACKEND_UNKNOWN;
    }

    return cake_win32_controllers[slot].identity.backend;
}

uint16_t CAKE_GetControllerVendorID(int slot) {
    if (!CAKE_IsControllerConnected(slot)) {
        return 0;
    }

    return cake_win32_controllers[slot].identity.vendor_id;
}

uint16_t CAKE_GetControllerProductID(int slot) {
    if (!CAKE_IsControllerConnected(slot)) {
        return 0;
    }

    return cake_win32_controllers[slot].identity.product_id;
}

int CAKE_GetControllerXInputIndex(int slot) {
    if (!CAKE_IsControllerConnected(slot)) {
        return -1;
    }
    if (cake_win32_controllers[slot].identity.backend != CAKE_BACKEND_XINPUT) {
        return -1;
    }

    return cake_win32_controllers[slot].identity.xinput_index;
}

CAKE_ControllerConnectionState CAKE_GetControllerConnectionState(int slot) {
    if (slot < 0 || slot >= CAKE_CONTROLLER_MAX) {
        return CAKE_CONN_EMPTY;
    }

    return cake_win32_connection_states[slot];
}

void cake_win32_initialize_controllers(void) {
    memset(cake_win32_controllers, 0, sizeof(cake_win32_controllers));
    memset(cake_win32_connection_states, 0, sizeof(cake_win32_connection_states));
    cake_win32_hid_trace = getenv("CAKE_HID_TRACE") != NULL;
    cake_win32_hid_ignored_clear();
    cake_win32_reset_discovery();
    cake_win32_xinput_load();
    cake_win32_scan_controllers();
}
void cake_win32_shutdown_controllers(void) {
    for (int slot = 0; slot < CAKE_CONTROLLER_MAX; ++slot) {
        cake_win32_hid_unbind(&cake_win32_controllers[slot]);
    }
    memset(cake_win32_controllers, 0, sizeof(cake_win32_controllers));
    memset(cake_win32_connection_states, 0, sizeof(cake_win32_connection_states));
    cake_win32_hid_ignored_clear();
    cake_win32_reset_discovery();
}

#endif
