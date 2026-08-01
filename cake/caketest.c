// caketest | This is a simple test harness for the cake components.
// This console tool shows raw mouse, keyboard, and controller input.
// The actual interface API calls will be accessed from a different
// set of helper functions (CAKE_Input).
// This tool only tests the core CAKE implementation.

#include <stdio.h>
#include <string.h>
#include "cake.h"

#define CAKE_TEST_AXIS_THRESHOLD    4000
#define CAKE_TEST_TRIGGER_THRESHOLD 8

static const char *cake_key_name(uint8_t key);
static const char *cake_mouse_button_name(int btn);
static void cake_print_buttons(uint16_t pressed, uint16_t released);

int main(void) {
    int     exit_flag = 0;
    uint8_t prev_keys[CAKE_KEY_TABLE_SIZE]        = {0};
    uint8_t prev_buttons[CAKE_MOUSE_BUTTON_COUNT] = {0};

    /* Per-slot last-printed controller state used for threshold comparison. */
    CAKE_ControllerState prev_controller[CAKE_CONTROLLER_MAX];

    memset(prev_controller, 0, sizeof(prev_controller));

    printf("CAKE input test running. Press ESC to exit.\n");

    while (!exit_flag) {
        CAKE_Poll();

        // Keyboard
        for (int i = 0; i < CAKE_KEY_TABLE_SIZE; i++) {
            if (CAKE_Keys[i] && !prev_keys[i]) {
                if (i == CAKE_KEY_ESCAPE) {
                    exit_flag = 1;
                    break;
                }
                const char *name = cake_key_name((uint8_t)i);
                if (name) { printf("Key pressed: %s\n", name); }
                else { printf("Key pressed: HID 0x%02X\n", i); }
            }
        }
        memcpy(prev_keys, CAKE_Keys, CAKE_KEY_TABLE_SIZE);

        // Mouse
        if (CAKE_MouseDeltaX != 0 || CAKE_MouseDeltaY != 0) { printf("Mouse move: dx=%d dy=%d\n", CAKE_MouseDeltaX, CAKE_MouseDeltaY); }

        if (CAKE_MouseWheel != 0) { printf("Mouse wheel: %d\n", CAKE_MouseWheel); }

        for (int i = 0; i < CAKE_MOUSE_BUTTON_COUNT; i++) {
            if (CAKE_MouseButtons[i] != prev_buttons[i]) {
                printf("Mouse button %s: %s\n",
                       cake_mouse_button_name(i),
                       CAKE_MouseButtons[i] ? "down" : "up");
            }
        }
        memcpy(prev_buttons, CAKE_MouseButtons, CAKE_MOUSE_BUTTON_COUNT);

        // Controllers
        for (int s = 0; s < CAKE_CONTROLLER_MAX; s++) {
            if (!CAKE_IsControllerConnected(s)) { continue; }

            const CAKE_ControllerState *controllerstate = CAKE_GetControllerState(s);
            if (!controllerstate) { continue; }

            /* Rumble test: drive vibration directly from analog trigger
               pressure (0-255 -> 0-65535) -- left trigger controls the
               left/low-frequency motor, right trigger controls the
               right/high-frequency motor. No timing/edge-detection needed
               since this is just a continuous readout like everything
               else in this tool; the threshold-gated trigger printing
               below already reports the values driving it. */
            CAKE_SetControllerVibration(s,
                (uint16_t)(controllerstate->leftTrigger  * 257),
                (uint16_t)(controllerstate->rightTrigger * 257));

            /* Buttons: compare and update prev independently of axes so a
               button event never contaminates the axis threshold baseline. */
            uint16_t prev_btn = prev_controller[s].buttons;
            uint16_t cur_btn  = controllerstate->buttons;
            uint16_t pressed  = cur_btn  & ~prev_btn;
            uint16_t released = prev_btn & ~cur_btn;

            if (pressed || released) {
                printf("Controller slot %d:\n", s);
                cake_print_buttons(pressed, released);
                prev_controller[s].buttons = cur_btn;
            }

            /* Axes and triggers: only print and update prev when the change
               from the last printed state exceeds the output threshold.
               This means prev tracks the last shown value, not the last
               polled value, so small drift never silently resets the baseline. */
            int dlx = (int)controllerstate->thumbLeftX  - (int)prev_controller[s].thumbLeftX;
            int dly = (int)controllerstate->thumbLeftY  - (int)prev_controller[s].thumbLeftY;
            int drx = (int)controllerstate->thumbRightX  - (int)prev_controller[s].thumbRightX;
            int dry = (int)controllerstate->thumbRightY  - (int)prev_controller[s].thumbRightY;
            int dlt = (int)controllerstate->leftTrigger  - (int)prev_controller[s].leftTrigger;
            int drt = (int)controllerstate->rightTrigger - (int)prev_controller[s].rightTrigger;

            if (dlx < 0) { dlx = -dlx; }
            if (dly < 0) { dly = -dly; }
            if (drx < 0) { drx = -drx; }
            if (dry < 0) { dry = -dry; }
            if (dlt < 0) { dlt = -dlt; }
            if (drt < 0) { drt = -drt; }

            if (dlx > CAKE_TEST_AXIS_THRESHOLD || dly > CAKE_TEST_AXIS_THRESHOLD ||
                drx > CAKE_TEST_AXIS_THRESHOLD || dry > CAKE_TEST_AXIS_THRESHOLD ||
                dlt > CAKE_TEST_TRIGGER_THRESHOLD || drt > CAKE_TEST_TRIGGER_THRESHOLD) {
                printf("Controller slot %d | btns %04X | LT %3u RT %3u"
                       " | LX %6d LY %6d | RX %6d RY %6d\n",
                       s,
                       controllerstate->buttons,
                       controllerstate->leftTrigger,  controllerstate->rightTrigger,
                       controllerstate->thumbLeftX,      controllerstate->thumbLeftY,
                       controllerstate->thumbRightX,      controllerstate->thumbRightY);

                prev_controller[s].leftTrigger  = controllerstate->leftTrigger;
                prev_controller[s].rightTrigger = controllerstate->rightTrigger;
                prev_controller[s].thumbLeftX      = controllerstate->thumbLeftX;
                prev_controller[s].thumbLeftY      = controllerstate->thumbLeftY;
                prev_controller[s].thumbRightX      = controllerstate->thumbRightX;
                prev_controller[s].thumbRightY      = controllerstate->thumbRightY;
            }
        }
    }

    printf("ESC detected. Exiting.\n");

    // Stop rumble on every slot -- otherwise a controller left holding a
    // trigger at exit would keep vibrating after the test tool closes.
    for (int s = 0; s < CAKE_CONTROLLER_MAX; s++) {
        if (CAKE_IsControllerConnected(s)) { CAKE_SetControllerVibration(s, 0, 0); }
    }

    CAKE_Shutdown();

    return 0;
}

//   Key name table
static const char *cake_key_name(uint8_t key) {
    switch (key) {
        case CAKE_KEY_ESCAPE:       return "ESCAPE";
        case CAKE_KEY_F1:           return "F1";
        case CAKE_KEY_F2:           return "F2";
        case CAKE_KEY_F3:           return "F3";
        case CAKE_KEY_F4:           return "F4";
        case CAKE_KEY_F5:           return "F5";
        case CAKE_KEY_F6:           return "F6";
        case CAKE_KEY_F7:           return "F7";
        case CAKE_KEY_F8:           return "F8";
        case CAKE_KEY_F9:           return "F9";
        case CAKE_KEY_F10:          return "F10";
        case CAKE_KEY_F11:          return "F11";
        case CAKE_KEY_F12:          return "F12";
        case CAKE_KEY_GRAVE:        return "GRAVE";
        case CAKE_KEY_1:            return "1";
        case CAKE_KEY_2:            return "2";
        case CAKE_KEY_3:            return "3";
        case CAKE_KEY_4:            return "4";
        case CAKE_KEY_5:            return "5";
        case CAKE_KEY_6:            return "6";
        case CAKE_KEY_7:            return "7";
        case CAKE_KEY_8:            return "8";
        case CAKE_KEY_9:            return "9";
        case CAKE_KEY_0:            return "0";
        case CAKE_KEY_MINUS:        return "MINUS";
        case CAKE_KEY_EQUALS:       return "EQUALS";
        case CAKE_KEY_BACKSPACE:    return "BACKSPACE";
        case CAKE_KEY_TAB:          return "TAB";
        case CAKE_KEY_Q:            return "Q";
        case CAKE_KEY_W:            return "W";
        case CAKE_KEY_E:            return "E";
        case CAKE_KEY_R:            return "R";
        case CAKE_KEY_T:            return "T";
        case CAKE_KEY_Y:            return "Y";
        case CAKE_KEY_U:            return "U";
        case CAKE_KEY_I:            return "I";
        case CAKE_KEY_O:            return "O";
        case CAKE_KEY_P:            return "P";
        case CAKE_KEY_LBRACKET:     return "LBRACKET";
        case CAKE_KEY_RBRACKET:     return "RBRACKET";
        case CAKE_KEY_BACKSLASH:    return "BACKSLASH";
        case CAKE_KEY_CAPSLOCK:     return "CAPSLOCK";
        case CAKE_KEY_A:            return "A";
        case CAKE_KEY_S:            return "S";
        case CAKE_KEY_D:            return "D";
        case CAKE_KEY_F:            return "F";
        case CAKE_KEY_G:            return "G";
        case CAKE_KEY_H:            return "H";
        case CAKE_KEY_J:            return "J";
        case CAKE_KEY_K:            return "K";
        case CAKE_KEY_L:            return "L";
        case CAKE_KEY_SEMICOLON:    return "SEMICOLON";
        case CAKE_KEY_APOSTROPHE:   return "APOSTROPHE";
        case CAKE_KEY_ENTER:        return "ENTER";
        case CAKE_KEY_Z:            return "Z";
        case CAKE_KEY_X:            return "X";
        case CAKE_KEY_C:            return "C";
        case CAKE_KEY_V:            return "V";
        case CAKE_KEY_B:            return "B";
        case CAKE_KEY_N:            return "N";
        case CAKE_KEY_M:            return "M";
        case CAKE_KEY_COMMA:        return "COMMA";
        case CAKE_KEY_PERIOD:       return "PERIOD";
        case CAKE_KEY_SLASH:        return "SLASH";
        case CAKE_KEY_SPACE:        return "SPACE";
        case CAKE_KEY_PRTSCR:       return "PRTSCR";
        case CAKE_KEY_SCROLLLOCK:   return "SCROLLLOCK";
        case CAKE_KEY_PAUSE:        return "PAUSE";
        case CAKE_KEY_INSERT:       return "INSERT";
        case CAKE_KEY_DELETE:       return "DELETE";
        case CAKE_KEY_HOME:         return "HOME";
        case CAKE_KEY_END:          return "END";
        case CAKE_KEY_PGUP:         return "PGUP";
        case CAKE_KEY_PGDN:         return "PGDN";
        case CAKE_KEY_UP:           return "UP";
        case CAKE_KEY_DOWN:         return "DOWN";
        case CAKE_KEY_LEFT:         return "LEFT";
        case CAKE_KEY_RIGHT:        return "RIGHT";
        case CAKE_KEY_NUMLOCK:      return "NUMLOCK";
        case CAKE_KEY_NUMPAD_DIV:   return "NUMPAD /";
        case CAKE_KEY_NUMPAD_MUL:   return "NUMPAD *";
        case CAKE_KEY_NUMPAD_SUB:   return "NUMPAD -";
        case CAKE_KEY_NUMPAD_ADD:   return "NUMPAD +";
        case CAKE_KEY_NUMPAD_ENT:   return "NUMPAD ENTER";
        case CAKE_KEY_NUMPAD_0:     return "NUMPAD 0";
        case CAKE_KEY_NUMPAD_1:     return "NUMPAD 1";
        case CAKE_KEY_NUMPAD_2:     return "NUMPAD 2";
        case CAKE_KEY_NUMPAD_3:     return "NUMPAD 3";
        case CAKE_KEY_NUMPAD_4:     return "NUMPAD 4";
        case CAKE_KEY_NUMPAD_5:     return "NUMPAD 5";
        case CAKE_KEY_NUMPAD_6:     return "NUMPAD 6";
        case CAKE_KEY_NUMPAD_7:     return "NUMPAD 7";
        case CAKE_KEY_NUMPAD_8:     return "NUMPAD 8";
        case CAKE_KEY_NUMPAD_9:     return "NUMPAD 9";
        case CAKE_KEY_NUMPAD_DOT:   return "NUMPAD .";
        case CAKE_KEY_LCTRL:        return "LCTRL";
        case CAKE_KEY_LSHIFT:       return "LSHIFT";
        case CAKE_KEY_LALT:         return "LALT";
        case CAKE_KEY_LGUI:         return "LGUI";
        case CAKE_KEY_RCTRL:        return "RCTRL";
        case CAKE_KEY_RSHIFT:       return "RSHIFT";
        case CAKE_KEY_RALT:         return "RALT";
        case CAKE_KEY_RGUI:         return "RGUI";

        default:                    return NULL;
    }
}

// Mouse button name table
static const char *cake_mouse_button_name(int btn) {
    switch (btn) {
        case CAKE_MOUSE_LEFT:   return "LEFT";
        case CAKE_MOUSE_RIGHT:  return "RIGHT";
        case CAKE_MOUSE_MIDDLE: return "MIDDLE";
        case CAKE_MOUSE_X1:     return "X1";
        case CAKE_MOUSE_X2:     return "X2";

        default:                return "UNKNOWN";
    }
}

// Controller helpers
static void cake_print_buttons(uint16_t pressed, uint16_t released) {
    static const struct { uint16_t mask; const char *name; } btns[] = {
        { CAKE_BUTTON_A,              "A"             },
        { CAKE_BUTTON_B,              "B"             },
        { CAKE_BUTTON_X,              "X"             },
        { CAKE_BUTTON_Y,              "Y"             },
        { CAKE_BUTTON_START,          "START"         },
        { CAKE_BUTTON_BACK,           "BACK"          },
        { CAKE_BUTTON_DPAD_UP,        "DPAD_UP"       },
        { CAKE_BUTTON_DPAD_DOWN,      "DPAD_DOWN"     },
        { CAKE_BUTTON_DPAD_LEFT,      "DPAD_LEFT"     },
        { CAKE_BUTTON_DPAD_RIGHT,     "DPAD_RIGHT"    },
        { CAKE_BUTTON_LEFT_SHOULDER,  "LB"           },
        { CAKE_BUTTON_RIGHT_SHOULDER, "RB"           },
        { CAKE_BUTTON_LEFT_THUMB,     "LEFT_THUMB"   },
        { CAKE_BUTTON_RIGHT_THUMB,    "RIGHT_THUMB"  }
    };
    int n = (int)(sizeof(btns) / sizeof(btns[0]));

    for (int i = 0; i < n; i++) {
        if (pressed  & btns[i].mask) { printf("  Button %-14s down\n", btns[i].name); }
        if (released & btns[i].mask) { printf("  Button %-14s up\n",   btns[i].name); }
    }

    return;
}
