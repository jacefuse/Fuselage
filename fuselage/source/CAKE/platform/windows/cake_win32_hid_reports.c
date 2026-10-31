// HID report decoding and normalization. SEE: (CAKE.md, Windows controllers)
#include "cake_win32_controller_private.h"
#if defined(_WIN32)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static bool cake_win32_hid_read_axis(const cake_win32_hid_state* hid_state, int axis_index, PCHAR report,
                                     ULONG length, int32_t* output) {
    const cake_win32_hid_axis* axis = &hid_state->axis[axis_index];
    if (!axis->present) {
        return false;
    }

    ULONG axis_value = 0;
    if (HidP_GetUsageValue(HidP_Input, cake_win32_hid_axis_usage[axis_index].page, 0,
                           cake_win32_hid_axis_usage[axis_index].usage, &axis_value, hid_state->preparsed,
                           report, length) != HIDP_STATUS_SUCCESS) {
        return false;
    }

    int64_t signed_value = axis_value > INT32_MAX ? (int64_t)axis_value - 0x100000000LL : axis_value;
    if (axis->logical_minimum < 0 && axis->bit_size > 0 && axis->bit_size < 32) {
        uint32_t sign = 1u << (axis->bit_size - 1);
        if (axis_value & sign) {
            signed_value = (int64_t)(axis_value & (sign - 1u)) - sign;
        }
    }
    *output = (int32_t)signed_value;

    return true;
}

static int16_t cake_win32_hid_norm_stick(const cake_win32_hid_axis* axis, int32_t value, bool invert) {
    if (axis->logical_maximum <= axis->logical_minimum) {
        return 0;
    }

    double fraction = (double)((int64_t)value - axis->logical_minimum) /
                      (double)((int64_t)axis->logical_maximum - axis->logical_minimum);
    double scaled = fraction * 65535.0 - 32768.0;
    if (invert) {
        scaled = -scaled - 1.0;
    }
    if (scaled > 32767.0) {
        scaled = 32767.0;
    }
    if (scaled < -32768.0) {
        scaled = -32768.0;
    }

    return (int16_t)scaled;
}

static uint8_t cake_win32_hid_norm_trigger(const cake_win32_hid_axis* axis, int32_t value) {
    if (axis->logical_maximum <= axis->logical_minimum) {
        return 0;
    }

    double fraction = (double)((int64_t)value - axis->logical_minimum) /
                      (double)((int64_t)axis->logical_maximum - axis->logical_minimum);
    if (fraction < 0.0) {
        fraction = 0.0;
    }
    if (fraction > 1.0) {
        fraction = 1.0;
    }

    return (uint8_t)(fraction * 255.0);
}

static void cake_win32_hid_trace_print(const cake_win32_controller* controller, const uint8_t* bytes,
                                       ULONG length) {
    const cake_win32_hid_state* hid_state = &controller->hid;

    printf("[CAKE] HID trace | slot %d |", (int)(controller - cake_win32_controllers));
    for (ULONG index = 0; index < length && index < 16; index++) {
        printf(" %02X", bytes[index]);
    }
    printf(" | buttons");
    for (int b = 0; b < 32; b++) {
        if (hid_state->raw_buttons & (1u << b)) {
            printf(" %d", b + 1);
        }
    }
    printf(" |");
    for (int axis_index = 0; axis_index < CAKE_WIN32_HID_AXIS_COUNT; axis_index++) {
        if (hid_state->axis[axis_index].present) {
            printf(" %s %ld", cake_win32_hid_axis_name[axis_index], (long)hid_state->raw_axes[axis_index]);
        }
    }
    printf(" | -> %04X LT %3u RT %3u LX %6d LY %6d RX %6d RY %6d\n", hid_state->live.buttons,
           hid_state->live.leftTrigger, hid_state->live.rightTrigger, hid_state->live.thumbLeftX,
           hid_state->live.thumbLeftY, hid_state->live.thumbRightX, hid_state->live.thumbRightY);

    return;
}

static void cake_win32_hid_apply_switch_full(cake_win32_controller* controller, const uint8_t* bytes,
                                             ULONG length) {
    cake_win32_hid_state* hid_state = &controller->hid;
    uint8_t right_buttons = bytes[3], shared_buttons = bytes[4], left_buttons = bytes[5];
    uint16_t button_mask = 0;

    if (right_buttons & 0x01) {
        button_mask |= CAKE_BUTTON_Y;
    }
    if (right_buttons & 0x02) {
        button_mask |= CAKE_BUTTON_X;
    }
    if (right_buttons & 0x04) {
        button_mask |= CAKE_BUTTON_A;
    }
    if (right_buttons & 0x08) {
        button_mask |= CAKE_BUTTON_B;
    }
    if (right_buttons & 0x40) {
        button_mask |= CAKE_BUTTON_RIGHT_SHOULDER;
    }
    if (shared_buttons & 0x01) {
        button_mask |= CAKE_BUTTON_BACK;
    }
    if (shared_buttons & 0x02) {
        button_mask |= CAKE_BUTTON_START;
    }
    if (shared_buttons & 0x04) {
        button_mask |= CAKE_BUTTON_RIGHT_THUMB;
    }
    if (shared_buttons & 0x08) {
        button_mask |= CAKE_BUTTON_LEFT_THUMB;
    }
    if (left_buttons & 0x01) {
        button_mask |= CAKE_BUTTON_DPAD_DOWN;
    }
    if (left_buttons & 0x02) {
        button_mask |= CAKE_BUTTON_DPAD_UP;
    }
    if (left_buttons & 0x04) {
        button_mask |= CAKE_BUTTON_DPAD_RIGHT;
    }
    if (left_buttons & 0x08) {
        button_mask |= CAKE_BUTTON_DPAD_LEFT;
    }
    if (left_buttons & 0x40) {
        button_mask |= CAKE_BUTTON_LEFT_SHOULDER;
    }

    hid_state->live.buttons = button_mask;
    hid_state->live.rightTrigger = (right_buttons & 0x80) ? 255 : 0;
    hid_state->live.leftTrigger = (left_buttons & 0x80) ? 255 : 0;

    int32_t stick[4] = {
        bytes[6] | ((bytes[7] & 0x0F) << 8),
        (bytes[7] >> 4) | (bytes[8] << 4),
        bytes[9] | ((bytes[10] & 0x0F) << 8),
        (bytes[10] >> 4) | (bytes[11] << 4),
    };

    if (!hid_state->switch_centered) {
        for (int index = 0; index < 4; index++) {
            hid_state->switch_center[index] =
                (stick[index] > 2048 - 700 && stick[index] < 2048 + 700) ? stick[index] : 2048;
        }
        hid_state->switch_centered = true;
    }

    int16_t output[4];
    for (int index = 0; index < 4; index++) {
        int32_t scaled_value = (stick[index] - hid_state->switch_center[index]) * 32767 / 1700;
        if (scaled_value > 32767) {
            scaled_value = 32767;
        }
        if (scaled_value < -32768) {
            scaled_value = -32768;
        }
        output[index] = (int16_t)scaled_value;
    }
    hid_state->live.thumbLeftX = output[0];
    hid_state->live.thumbLeftY = output[1];
    hid_state->live.thumbRightX = output[2];
    hid_state->live.thumbRightY = output[3];
    int32_t left_x = stick[0], left_y = stick[1], right_x = stick[2], right_y = stick[3];

    if (cake_win32_hid_trace) {
        uint32_t raw =
            (uint32_t)right_buttons | ((uint32_t)shared_buttons << 8) | ((uint32_t)left_buttons << 16);
        bool traced = raw != hid_state->raw_buttons;
        int32_t axis_value[4] = {left_x, left_y, right_x, right_y};
        int axis_index[4] = {CAKE_WIN32_HID_AXIS_X, CAKE_WIN32_HID_AXIS_Y, CAKE_WIN32_HID_AXIS_RX,
                             CAKE_WIN32_HID_AXIS_RY};
        for (int index = 0; index < 4; index++) {
            int64_t difference = (int64_t)axis_value[index] - hid_state->raw_axes[axis_index[index]];
            if (difference < 0) {
                difference = -difference;
            }
            if (difference > 512) {
                traced = true;
            }
            hid_state->raw_axes[axis_index[index]] = axis_value[index];
            hid_state->axis[axis_index[index]].present = true;
        }
        hid_state->raw_buttons = raw;
        if (traced) {
            cake_win32_hid_trace_print(controller, bytes, length);
        }
    }

    return;
}

static void cake_win32_hid_apply_report(cake_win32_controller* controller, const uint8_t* bytes,
                                        ULONG length) {
    cake_win32_hid_state* hid_state = &controller->hid;
    const cake_win32_hid_layout* layout = hid_state->layout;
    PCHAR report = (PCHAR)bytes;
    bool traced = false;

    if (controller->identity.vendor_id == 0x057E && length >= 12 &&
        (bytes[0] == 0x30 || bytes[0] == 0x21 || bytes[0] == 0x31)) {
        cake_win32_hid_apply_switch_full(controller, bytes, length);
        return;
    }

    USAGE usages[64];
    ULONG usage_count = sizeof usages / sizeof usages[0];
    if (HidP_GetUsages(HidP_Input, 0x09, 0, usages, &usage_count, hid_state->preparsed, report, length) ==
        HIDP_STATUS_SUCCESS) {
        uint32_t raw = 0;
        uint16_t mask = 0;
        for (ULONG index = 0; index < usage_count && index < 64; index++) {
            USAGE usage = usages[index];
            if (usage >= 1 && usage <= 32) {
                raw |= 1u << (usage - 1);
            }
            if (usage >= 1 && usage <= 16) {
                mask |= layout->button[usage];
            }
        }
        hid_state->live.buttons = (uint16_t)((hid_state->live.buttons & CAKE_WIN32_HID_DPAD_MASK) | mask);
        if (hid_state->left_trigger_axis < 0 && layout->left_trigger_button) {
            hid_state->live.leftTrigger = (raw & (1u << (layout->left_trigger_button - 1))) ? 255 : 0;
        }
        if (hid_state->right_trigger_axis < 0 && layout->right_trigger_button) {
            hid_state->live.rightTrigger = (raw & (1u << (layout->right_trigger_button - 1))) ? 255 : 0;
        }
        if (raw != hid_state->raw_buttons) {
            traced = true;
        }
        hid_state->raw_buttons = raw;
    }

    for (int axis_index = 0; axis_index < CAKE_WIN32_HID_AXIS_COUNT; axis_index++) {
        int32_t axis_value;
        if (!cake_win32_hid_read_axis(hid_state, axis_index, report, length, &axis_value)) {
            continue;
        }
        const cake_win32_hid_axis* axis = &hid_state->axis[axis_index];
        int64_t range = (int64_t)axis->logical_maximum - axis->logical_minimum;
        int64_t delta = (int64_t)axis_value - hid_state->raw_axes[axis_index];
        if (delta < 0) {
            delta = -delta;
        }
        if (axis_index == CAKE_WIN32_HID_AXIS_HAT ? (axis_value != hid_state->raw_axes[axis_index])
                                                  : (range > 0 && delta > range / 8)) {
            traced = true;
        }
        hid_state->raw_axes[axis_index] = axis_value;

        switch (axis_index) {
        case CAKE_WIN32_HID_AXIS_X:
            hid_state->live.thumbLeftX = cake_win32_hid_norm_stick(axis, axis_value, false);
            break;
        case CAKE_WIN32_HID_AXIS_Y:
            hid_state->live.thumbLeftY = cake_win32_hid_norm_stick(axis, axis_value, true);
            break;
        case CAKE_WIN32_HID_AXIS_HAT: {

            int64_t direction = (int64_t)axis_value - axis->logical_minimum;
            int64_t direction_count = range + 1;
            uint16_t d = 0;
            if (direction >= 0 && direction < direction_count) {
                if (direction_count == 4) {
                    direction *= 2;
                }
                switch (direction) {
                case 0:
                    d = CAKE_BUTTON_DPAD_UP;
                    break;
                case 1:
                    d = CAKE_BUTTON_DPAD_UP | CAKE_BUTTON_DPAD_RIGHT;
                    break;
                case 2:
                    d = CAKE_BUTTON_DPAD_RIGHT;
                    break;
                case 3:
                    d = CAKE_BUTTON_DPAD_DOWN | CAKE_BUTTON_DPAD_RIGHT;
                    break;
                case 4:
                    d = CAKE_BUTTON_DPAD_DOWN;
                    break;
                case 5:
                    d = CAKE_BUTTON_DPAD_DOWN | CAKE_BUTTON_DPAD_LEFT;
                    break;
                case 6:
                    d = CAKE_BUTTON_DPAD_LEFT;
                    break;
                case 7:
                    d = CAKE_BUTTON_DPAD_UP | CAKE_BUTTON_DPAD_LEFT;
                    break;
                default:
                    break;
                }
            }
            hid_state->live.buttons = (uint16_t)((hid_state->live.buttons & ~CAKE_WIN32_HID_DPAD_MASK) | d);
            break;
        }
        default:
            break;
        }
        if (axis_index == hid_state->right_stick_x) {
            hid_state->live.thumbRightX = cake_win32_hid_norm_stick(axis, axis_value, false);
        }
        if (axis_index == hid_state->right_stick_y) {
            hid_state->live.thumbRightY = cake_win32_hid_norm_stick(axis, axis_value, true);
        }
        if (axis_index == hid_state->left_trigger_axis) {
            hid_state->live.leftTrigger = cake_win32_hid_norm_trigger(axis, axis_value);
        }
        if (axis_index == hid_state->right_trigger_axis) {
            hid_state->live.rightTrigger = cake_win32_hid_norm_trigger(axis, axis_value);
        }
    }

    if (cake_win32_hid_trace && traced) {
        cake_win32_hid_trace_print(controller, bytes, length);
    }

    return;
}

void cake_win32_handle_controller_input(HANDLE dev, const RAWHID* hid) {
    cake_win32_controller* controller = cake_win32_hid_slot_for(dev);
    if (!controller || !controller->hid.preparsed) {
        return;
    }

    const uint8_t* p = hid->bRawData;
    for (DWORD k = 0; k < hid->dwCount; k++, p += hid->dwSizeHid) {
        cake_win32_hid_apply_report(controller, p, hid->dwSizeHid);
    }

    return;
}
#endif
