// Controller discovery and interface verdict cache. SEE: (CAKE.md, Windows controllers)
#include "cake_win32_controller_private.h"
#if defined(_WIN32)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static bool cake_win32_is_controller_path(const char* path) {
    HANDLE device_handle =
        CreateFileA(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    if (device_handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    PHIDP_PREPARSED_DATA preparsed = NULL;
    if (!HidD_GetPreparsedData(device_handle, &preparsed)) {
        CloseHandle(device_handle);
        return false;
    }

    HIDP_CAPS caps;
    NTSTATUS status = HidP_GetCaps(preparsed, &caps);
    HidD_FreePreparsedData(preparsed);
    CloseHandle(device_handle);

    if (status != HIDP_STATUS_SUCCESS) {
        return false;
    }

    return caps.UsagePage == 0x01 && (caps.Usage == 0x04 || caps.Usage == 0x05);
}

typedef enum { CAKE_WIN32_NODE_NOT_CONTROLLER = 0, CAKE_WIN32_NODE_NO_SLOT } cake_win32_node_verdict;

typedef struct {
    char path[CAKE_CONTROLLER_PATH_MAX];
    cake_win32_node_verdict verdict;
    bool seen;
} cake_win32_node;

static cake_win32_node cake_win32_nodes[CAKE_WIN32_MAX_NODES];
static int cake_win32_node_count = 0;

static bool cake_win32_slot_freed = false;

static cake_win32_node* cake_win32_memo_find(const char* path) {
    for (int interface_index = 0; interface_index < cake_win32_node_count; interface_index++) {
        if (strcmp(cake_win32_nodes[interface_index].path, path) == 0) {
            return &cake_win32_nodes[interface_index];
        }
    }

    return NULL;
}

static void cake_win32_memo_set(const char* path, cake_win32_node_verdict verdict) {
    cake_win32_node* node = cake_win32_memo_find(path);

    if (!node) {
        if (cake_win32_node_count >= CAKE_WIN32_MAX_NODES) {
            return;
        }
        node = &cake_win32_nodes[cake_win32_node_count++];
        strncpy(node->path, path, CAKE_CONTROLLER_PATH_MAX - 1);
        node->path[CAKE_CONTROLLER_PATH_MAX - 1] = '\0';
    }

    node->verdict = verdict;
    node->seen = true;

    return;
}

static void cake_win32_memo_forget(const char* path) {
    for (int interface_index = 0; interface_index < cake_win32_node_count; interface_index++) {
        if (strcmp(cake_win32_nodes[interface_index].path, path) != 0) {
            continue;
        }
        cake_win32_nodes[interface_index] = cake_win32_nodes[--cake_win32_node_count];
        return;
    }

    return;
}

void cake_win32_scan_controllers(void) {
    ULONGLONG timestamp = GetTickCount64();
    BOOL seen[CAKE_CONTROLLER_MAX] = {0};

    HDEVINFO device_info =
        SetupDiGetClassDevsA(&cake_win32_hid_guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if (device_info == INVALID_HANDLE_VALUE) {
        return;
    }

    SP_DEVICE_INTERFACE_DATA device_interface = {0};
    device_interface.cbSize = sizeof(device_interface);

    union {
        SP_DEVICE_INTERFACE_DETAIL_DATA_A alignment;
        char bytes[sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A) + CAKE_CONTROLLER_PATH_MAX];
    } detail_storage;
    SP_DEVICE_INTERFACE_DETAIL_DATA_A* detail = (SP_DEVICE_INTERFACE_DETAIL_DATA_A*)detail_storage.bytes;

    for (int interface_index = 0; interface_index < cake_win32_node_count; interface_index++) {
        cake_win32_nodes[interface_index].seen = false;
    }

    for (DWORD interface_index = 0; SetupDiEnumDeviceInterfaces(device_info, NULL, &cake_win32_hid_guid,
                                                                interface_index, &device_interface);
         interface_index++) {
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

        if (!SetupDiGetDeviceInterfaceDetailA(device_info, &device_interface, detail, sizeof(detail_storage),
                                              NULL, NULL)) {
            continue;
        }

        const char* path = detail->DevicePath;

        int existing_slot = -1;
        for (int slot = 0; slot < CAKE_CONTROLLER_MAX; slot++) {
            if (cake_win32_controllers[slot].device_path[0] != '\0' &&
                strcmp(cake_win32_controllers[slot].device_path, path) == 0) {
                existing_slot = slot;
                break;
            }
        }

        if (existing_slot >= 0) {
            seen[existing_slot] = TRUE;
            cake_win32_controllers[existing_slot].last_seen_at = timestamp;

            if (cake_win32_controllers[existing_slot].lost_at != 0) {
                cake_win32_controllers[existing_slot].lost_at = 0;
                cake_win32_connection_states[existing_slot] = CAKE_CONN_ATTACHED;
                printf("[CAKE] Controller returned  | %s | slot %d\n",
                       cake_win32_controllers[existing_slot].name, existing_slot);

                if (cake_win32_controllers[existing_slot].identity.backend == CAKE_BACKEND_HID) {
                    cake_win32_hid_bind_lookup(&cake_win32_controllers[existing_slot]);
                }
            }

            continue;
        }

        cake_win32_node* cached_verdict = cake_win32_memo_find(path);
        if (cached_verdict) {
            cached_verdict->seen = true;
            if (cached_verdict->verdict == CAKE_WIN32_NODE_NOT_CONTROLLER) {
                continue;
            }
            if (cached_verdict->verdict == CAKE_WIN32_NODE_NO_SLOT && !cake_win32_slot_freed) {
                continue;
            }
        }

        if (!cake_win32_is_controller_path(path)) {
            cake_win32_memo_set(path, CAKE_WIN32_NODE_NOT_CONTROLLER);
            continue;
        }

        const char* name = cake_win32_is_xinput_path(path) ? "XInput Controller" : "Unknown Controller";

        int available_slot = -1;
        for (int slot = 0; slot < CAKE_CONTROLLER_MAX; slot++) {
            if (cake_win32_controllers[slot].device_path[0] == '\0') {
                available_slot = slot;
                break;
            }
        }

        if (available_slot < 0) {

            if (!cached_verdict) {
                printf("[CAKE] Controller found, every slot taken | %s\n", path);
            }
            cake_win32_memo_set(path, CAKE_WIN32_NODE_NO_SLOT);
            continue;
        }

        strncpy(cake_win32_controllers[available_slot].device_path, path, CAKE_CONTROLLER_PATH_MAX - 1);
        strncpy(cake_win32_controllers[available_slot].name, name, CAKE_CONTROLLER_NAME_MAX - 1);
        cake_win32_controllers[available_slot].chain_index = available_slot;
        cake_win32_controllers[available_slot].attached_at = timestamp;
        cake_win32_controllers[available_slot].last_seen_at = timestamp;
        cake_win32_controllers[available_slot].lost_at = 0;
        cake_win32_controllers[available_slot].identity_valid = false;
        cake_win32_controllers[available_slot].front_buffer = 0;
        memset(cake_win32_controllers[available_slot].state, 0,
               sizeof(cake_win32_controllers[available_slot].state));
        seen[available_slot] = TRUE;
        cake_win32_connection_states[available_slot] = CAKE_CONN_ATTACHED;

        cake_win32_identify_controller(&cake_win32_controllers[available_slot]);

        printf("[CAKE] Controller attached  | %s | slot %d\n", cake_win32_controllers[available_slot].name,
               available_slot);

        const cake_win32_controller_identity* identity = &cake_win32_controllers[available_slot].identity;
        printf("[CAKE] Controller identified | slot %d | VID %04X PID %04X | %s", available_slot,
               identity->vendor_id, identity->product_id,
               identity->backend == CAKE_BACKEND_XINPUT ? "XInput" : "HID");
        if (identity->backend == CAKE_BACKEND_XINPUT && identity->xinput_index >= 0) {
            printf(" | xinputIndex %d", identity->xinput_index);
        }
        printf("\n");

        if (identity->backend == CAKE_BACKEND_HID) {
            cake_win32_hid_bind_lookup(&cake_win32_controllers[available_slot]);
        }

        cake_win32_memo_forget(path);
    }

    SetupDiDestroyDeviceInfoList(device_info);

    for (int slot = 0; slot < cake_win32_node_count;) {
        if (cake_win32_nodes[slot].seen) {
            slot++;
            continue;
        }
        cake_win32_nodes[slot] = cake_win32_nodes[--cake_win32_node_count];
    }

    cake_win32_slot_freed = false;

    for (int slot = 0; slot < CAKE_CONTROLLER_MAX; slot++) {
        if (cake_win32_controllers[slot].device_path[0] == '\0') {
            continue;
        }
        if (seen[slot]) {
            continue;
        }

        if (cake_win32_controllers[slot].lost_at == 0) {
            cake_win32_controllers[slot].lost_at = timestamp;
            cake_win32_connection_states[slot] = CAKE_CONN_LOST;
            printf("[CAKE] Controller detached  | %s | slot %d\n", cake_win32_controllers[slot].name, slot);

            if (cake_win32_controllers[slot].identity.backend == CAKE_BACKEND_HID) {
                cake_win32_hid_unbind(&cake_win32_controllers[slot]);
                memset(cake_win32_controllers[slot].state, 0, sizeof cake_win32_controllers[slot].state);
            }
        } else if (timestamp - cake_win32_controllers[slot].lost_at > CAKE_CONTROLLER_TIMEOUT_MS) {
            cake_win32_connection_states[slot] = CAKE_CONN_TIMEDOUT;
            printf("[CAKE] Controller timed out | %s | slot %d\n", cake_win32_controllers[slot].name, slot);
            cake_win32_hid_unbind(&cake_win32_controllers[slot]);
            memset(&cake_win32_controllers[slot], 0, sizeof(cake_win32_controller));

            cake_win32_slot_freed = true;
        }
    }

    return;
}

void cake_win32_reset_discovery(void) {
    cake_win32_node_count = 0;
    cake_win32_slot_freed = false;
}
void cake_win32_refresh_controller_discovery(void) {
    cake_win32_node_count = 0;
    cake_win32_hid_ignored_clear();
    cake_win32_scan_controllers();
}
#endif
