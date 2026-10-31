// evdev mapping and scaling. SEE: (CAKE.md, Linux controllers)
#include "cake_linux_controller_private.h"
#if defined(__linux__)
bool cake_linux_test_bit(const unsigned long* bits, int bit_index) {
    return (bits[bit_index / (8 * (int)sizeof(unsigned long))] >>
            (bit_index % (8 * (int)sizeof(unsigned long)))) &
           1UL;
}

static int16_t cake_linux_normalize_stick(const struct input_absinfo* range_info, int value, bool invert) {
    if (range_info->maximum <= range_info->minimum) {
        return 0;
    }

    double fraction = (double)((int64_t)value - range_info->minimum) /
                      (double)((int64_t)range_info->maximum - range_info->minimum);
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

static uint8_t cake_linux_normalize_trigger(const struct input_absinfo* range_info, int value) {
    if (range_info->maximum <= range_info->minimum) {
        return 0;
    }

    double fraction = (double)((int64_t)value - range_info->minimum) /
                      (double)((int64_t)range_info->maximum - range_info->minimum);
    if (fraction < 0.0) {
        fraction = 0.0;
    }
    if (fraction > 1.0) {
        fraction = 1.0;
    }

    return (uint8_t)(fraction * 255.0);
}

static int cake_linux_axis_index(int code) {
    for (int index = 0; index < CAKE_LINUX_AXIS_COUNT; index++) {
        if (cake_linux_axes[index] == code) {
            return index;
        }
    }

    return -1;
}

void cake_linux_apply_controller_event(cake_linux_controller* controller, const struct input_event* event) {
    if (event->type == EV_KEY) {
        uint16_t button_mask = 0;
        bool down = event->value != 0;

        switch (event->code) {
        case BTN_SOUTH:
            button_mask = CAKE_BUTTON_A;
            break;
        case BTN_EAST:
            button_mask = CAKE_BUTTON_B;
            break;
        case BTN_NORTH:
            button_mask = CAKE_BUTTON_X;
            break;
        case BTN_WEST:
            button_mask = CAKE_BUTTON_Y;
            break;
        case BTN_TL:
            button_mask = CAKE_BUTTON_LEFT_SHOULDER;
            break;
        case BTN_TR:
            button_mask = CAKE_BUTTON_RIGHT_SHOULDER;
            break;
        case BTN_SELECT:
            button_mask = CAKE_BUTTON_BACK;
            break;
        case BTN_START:
            button_mask = CAKE_BUTTON_START;
            break;
        case BTN_THUMBL:
            button_mask = CAKE_BUTTON_LEFT_THUMB;
            break;
        case BTN_THUMBR:
            button_mask = CAKE_BUTTON_RIGHT_THUMB;
            break;
        case BTN_DPAD_UP:
            button_mask = CAKE_BUTTON_DPAD_UP;
            break;
        case BTN_DPAD_DOWN:
            button_mask = CAKE_BUTTON_DPAD_DOWN;
            break;
        case BTN_DPAD_LEFT:
            button_mask = CAKE_BUTTON_DPAD_LEFT;
            break;
        case BTN_DPAD_RIGHT:
            button_mask = CAKE_BUTTON_DPAD_RIGHT;
            break;

        case BTN_TL2:
            controller->live.leftTrigger = down ? 255 : 0;
            return;
        case BTN_TR2:
            controller->live.rightTrigger = down ? 255 : 0;
            return;
        default:
            return;
        }

        if (down) {
            controller->live.buttons |= button_mask;
        } else {
            controller->live.buttons &= (uint16_t)~button_mask;
        }

        return;
    }

    if (event->type == EV_ABS) {
        int axis_index = cake_linux_axis_index(event->code);
        if (axis_index < 0 || !controller->has_axis[axis_index]) {
            return;
        }
        const struct input_absinfo* range_info = &controller->axis_info[axis_index];

        switch (event->code) {
        case ABS_X:
            controller->live.thumbLeftX = cake_linux_normalize_stick(range_info, event->value, false);
            break;
        case ABS_Y:
            controller->live.thumbLeftY = cake_linux_normalize_stick(range_info, event->value, true);
            break;
        case ABS_RX:
            controller->live.thumbRightX = cake_linux_normalize_stick(range_info, event->value, false);
            break;
        case ABS_RY:
            controller->live.thumbRightY = cake_linux_normalize_stick(range_info, event->value, true);
            break;
        case ABS_Z:
            controller->live.leftTrigger = cake_linux_normalize_trigger(range_info, event->value);
            break;
        case ABS_RZ:
            controller->live.rightTrigger = cake_linux_normalize_trigger(range_info, event->value);
            break;
        case ABS_HAT0X:
            controller->live.buttons &= (uint16_t)~(CAKE_BUTTON_DPAD_LEFT | CAKE_BUTTON_DPAD_RIGHT);
            if (event->value < 0) {
                controller->live.buttons |= CAKE_BUTTON_DPAD_LEFT;
            }
            if (event->value > 0) {
                controller->live.buttons |= CAKE_BUTTON_DPAD_RIGHT;
            }
            break;
        case ABS_HAT0Y:
            controller->live.buttons &= (uint16_t)~(CAKE_BUTTON_DPAD_UP | CAKE_BUTTON_DPAD_DOWN);
            if (event->value < 0) {
                controller->live.buttons |= CAKE_BUTTON_DPAD_UP;
            }
            if (event->value > 0) {
                controller->live.buttons |= CAKE_BUTTON_DPAD_DOWN;
            }
            break;
        default:
            break;
        }

        return;
    }

    return;
}

#endif
