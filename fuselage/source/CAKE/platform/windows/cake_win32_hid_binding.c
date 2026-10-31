// HID descriptor ownership and Raw Input handle binding. SEE: (CAKE.md, Windows controllers)
#include "cake_win32_controller_private.h"
#if defined(_WIN32)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static HANDLE cake_win32_hid_ignored[CAKE_WIN32_MAX_NODES];
static int cake_win32_hid_ignored_count = 0;

void cake_win32_hid_ignored_clear(void) {
    cake_win32_hid_ignored_count = 0;

    return;
}

static bool cake_win32_hid_path_equal(const char* first_path, const char* second_path) {
    for (;; first_path++, second_path++) {
        unsigned char first_character = (unsigned char)*first_path,
                      second_character = (unsigned char)*second_path;
        if (first_character >= 'A' && first_character <= 'Z') {
            first_character = (unsigned char)(first_character + 32);
        }
        if (second_character >= 'A' && second_character <= 'Z') {
            second_character = (unsigned char)(second_character + 32);
        }
        if (first_character != second_character) {
            return false;
        }
        if (first_character == 0) {
            return true;
        }
    }
}

void cake_win32_hid_unbind(cake_win32_controller* controller) {
    free(controller->hid.preparsed);
    memset(&controller->hid, 0, sizeof controller->hid);
    controller->hid.right_stick_x = controller->hid.right_stick_y = controller->hid.left_trigger_axis =
        controller->hid.right_trigger_axis = -1;

    return;
}

static void cake_win32_hid_switch_usb_wake(const cake_win32_controller* controller);

static bool cake_win32_hid_bind_handle(cake_win32_controller* controller, HANDLE raw) {
    cake_win32_hid_unbind(controller);

    UINT size = 0;
    if (GetRawInputDeviceInfoA(raw, RIDI_PREPARSEDDATA, NULL, &size) != 0 || size == 0) {
        return false;
    }

    PHIDP_PREPARSED_DATA preparsed = (PHIDP_PREPARSED_DATA)malloc(size);
    if (!preparsed) {
        return false;
    }
    if (GetRawInputDeviceInfoA(raw, RIDI_PREPARSEDDATA, preparsed, &size) == (UINT)-1) {
        free(preparsed);
        return false;
    }

    HIDP_CAPS caps;
    if (HidP_GetCaps(preparsed, &caps) != HIDP_STATUS_SUCCESS) {
        free(preparsed);
        return false;
    }

    cake_win32_hid_state* hid_state = &controller->hid;
    hid_state->raw = raw;
    hid_state->preparsed = preparsed;
    hid_state->layout =
        cake_win32_hid_layout_for(controller->identity.vendor_id, controller->identity.product_id);

    USHORT value_cap_count = caps.NumberInputValueCaps;
    HIDP_VALUE_CAPS* value_caps =
        value_cap_count ? (HIDP_VALUE_CAPS*)malloc(value_cap_count * sizeof *value_caps) : NULL;
    if (value_caps &&
        HidP_GetValueCaps(HidP_Input, value_caps, &value_cap_count, preparsed) == HIDP_STATUS_SUCCESS) {
        for (USHORT index = 0; index < value_cap_count; index++) {
            USAGE first_usage = value_caps[index].IsRange ? value_caps[index].Range.UsageMin
                                                          : value_caps[index].NotRange.Usage;
            USAGE last_usage = value_caps[index].IsRange ? value_caps[index].Range.UsageMax : first_usage;
            for (int axis_index = 0; axis_index < CAKE_WIN32_HID_AXIS_COUNT; axis_index++) {
                if (value_caps[index].UsagePage != cake_win32_hid_axis_usage[axis_index].page) {
                    continue;
                }
                USAGE usage = cake_win32_hid_axis_usage[axis_index].usage;
                if (usage < first_usage || usage > last_usage || hid_state->axis[axis_index].present) {
                    continue;
                }
                cake_win32_hid_axis* axis = &hid_state->axis[axis_index];
                axis->present = true;
                axis->logical_minimum = value_caps[index].LogicalMin;
                axis->logical_maximum = value_caps[index].LogicalMax;
                axis->bit_size = value_caps[index].BitSize;

                if (axis->logical_maximum < axis->logical_minimum && axis->logical_minimum >= 0) {
                    axis->logical_maximum =
                        (axis->bit_size >= 31) ? LONG_MAX : (LONG)((1L << axis->bit_size) - 1);
                }
            }
        }
    }
    free(value_caps);

    bool has_z_pair =
        hid_state->axis[CAKE_WIN32_HID_AXIS_Z].present && hid_state->axis[CAKE_WIN32_HID_AXIS_RZ].present;
    bool has_rx_pair =
        hid_state->axis[CAKE_WIN32_HID_AXIS_RX].present && hid_state->axis[CAKE_WIN32_HID_AXIS_RY].present;
    int remaining_axes[4];
    int remaining_axis_count = 0;
    for (int axis_index = CAKE_WIN32_HID_AXIS_Z; axis_index <= CAKE_WIN32_HID_AXIS_RZ; axis_index++) {
        if (hid_state->axis[axis_index].present) {
            remaining_axes[remaining_axis_count++] = axis_index;
        }
    }

    if (has_z_pair) {
        hid_state->right_stick_x = CAKE_WIN32_HID_AXIS_Z;
        hid_state->right_stick_y = CAKE_WIN32_HID_AXIS_RZ;
    } else if (remaining_axis_count == 2) {
        hid_state->right_stick_x = remaining_axes[0];
        hid_state->right_stick_y = remaining_axes[1];
    } else if (has_rx_pair) {
        hid_state->right_stick_x = CAKE_WIN32_HID_AXIS_RX;
        hid_state->right_stick_y = CAKE_WIN32_HID_AXIS_RY;
    }

    if (hid_state->axis[CAKE_WIN32_HID_AXIS_BRAKE].present &&
        hid_state->axis[CAKE_WIN32_HID_AXIS_ACCELERATOR].present) {
        hid_state->left_trigger_axis = CAKE_WIN32_HID_AXIS_BRAKE;
        hid_state->right_trigger_axis = CAKE_WIN32_HID_AXIS_ACCELERATOR;
    } else if (has_z_pair && has_rx_pair) {
        hid_state->left_trigger_axis = CAKE_WIN32_HID_AXIS_RX;
        hid_state->right_trigger_axis = CAKE_WIN32_HID_AXIS_RY;
    }

    printf("[CAKE] HID bound | slot %d | handle %p | layout %s | %u buttons | axes",
           (int)(controller - cake_win32_controllers), (void*)raw, hid_state->layout->name,
           (unsigned)caps.NumberInputButtonCaps);
    for (int axis_index = 0; axis_index < CAKE_WIN32_HID_AXIS_COUNT; axis_index++) {
        if (!hid_state->axis[axis_index].present) {
            continue;
        }
        printf(" %s[%ld..%ld]", cake_win32_hid_axis_name[axis_index],
               (long)hid_state->axis[axis_index].logical_minimum,
               (long)hid_state->axis[axis_index].logical_maximum);
    }
    printf("\n");

    cake_win32_hid_switch_usb_wake(controller);

    return true;
}

static void cake_win32_hid_switch_usb_wake(const cake_win32_controller* controller) {
    if (controller->identity.vendor_id != 0x057E) {
        return;
    }

    HANDLE device_handle = CreateFileA(controller->device_path, GENERIC_WRITE,
                                       FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (device_handle == INVALID_HANDLE_VALUE) {
        return;
    }

    static const uint8_t handshake[64] = {0x80, 0x02};
    static const uint8_t force_usb[64] = {0x80, 0x04};
    static const uint8_t full_mode[64] = {0x01, 0x00, 0x00, 0x01, 0x40, 0x40,
                                          0x00, 0x01, 0x40, 0x40, 0x03, 0x30};
    DWORD bytes_written;
    WriteFile(device_handle, handshake, sizeof handshake, &bytes_written, NULL);
    WriteFile(device_handle, force_usb, sizeof force_usb, &bytes_written, NULL);
    WriteFile(device_handle, full_mode, sizeof full_mode, &bytes_written, NULL);
    CloseHandle(device_handle);

    return;
}

void cake_win32_hid_bind_lookup(cake_win32_controller* controller) {
    UINT device_count = 0;
    if (GetRawInputDeviceList(NULL, &device_count, sizeof(RAWINPUTDEVICELIST)) == (UINT)-1 ||
        device_count == 0) {
        return;
    }

    RAWINPUTDEVICELIST* list = (RAWINPUTDEVICELIST*)malloc(device_count * sizeof *list);
    if (!list) {
        return;
    }

    UINT device_count_read = GetRawInputDeviceList(list, &device_count, sizeof *list);
    if (device_count_read != (UINT)-1) {
        for (UINT index = 0; index < device_count_read; index++) {
            if (list[index].dwType != RIM_TYPEHID) {
                continue;
            }
            char name[CAKE_CONTROLLER_PATH_MAX];
            UINT name_capacity = sizeof name;
            if (GetRawInputDeviceInfoA(list[index].hDevice, RIDI_DEVICENAME, name, &name_capacity) ==
                (UINT)-1) {
                continue;
            }
            if (!cake_win32_hid_path_equal(name, controller->device_path)) {
                continue;
            }
            cake_win32_hid_bind_handle(controller, list[index].hDevice);
            break;
        }
    }
    free(list);

    cake_win32_hid_ignored_clear();

    return;
}

cake_win32_controller* cake_win32_hid_slot_for(HANDLE device) {
    for (int index = 0; index < CAKE_CONTROLLER_MAX; index++) {
        cake_win32_controller* controller = &cake_win32_controllers[index];
        if (controller->identity_valid && controller->identity.backend == CAKE_BACKEND_HID &&
            controller->hid.raw == device) {
            return controller;
        }
    }
    for (int index = 0; index < cake_win32_hid_ignored_count; index++) {
        if (cake_win32_hid_ignored[index] == device) {
            return NULL;
        }
    }

    char name[CAKE_CONTROLLER_PATH_MAX];
    UINT name_capacity = sizeof name;
    if (GetRawInputDeviceInfoA(device, RIDI_DEVICENAME, name, &name_capacity) != (UINT)-1) {
        for (int index = 0; index < CAKE_CONTROLLER_MAX; index++) {
            cake_win32_controller* controller = &cake_win32_controllers[index];
            if (controller->device_path[0] == '\0' || !controller->identity_valid) {
                continue;
            }
            if (controller->identity.backend != CAKE_BACKEND_HID) {
                continue;
            }
            if (controller->lost_at != 0) {
                continue;
            }
            if (!cake_win32_hid_path_equal(name, controller->device_path)) {
                continue;
            }
            if (cake_win32_hid_bind_handle(controller, device)) {
                return controller;
            }
        }
    }

    if (cake_win32_hid_ignored_count < CAKE_WIN32_MAX_NODES) {
        cake_win32_hid_ignored[cake_win32_hid_ignored_count++] = device;
    }

    return NULL;
}
#endif
