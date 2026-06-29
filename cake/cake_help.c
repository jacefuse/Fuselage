
#include "cake_help.h"

const char* CAKE_KeyName(uint8_t key) {
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


const char* CAKE_MouseButtonName(int btn) {
    switch (btn) {
    case CAKE_MOUSE_LEFT:   return "LEFT";
    case CAKE_MOUSE_RIGHT:  return "RIGHT";
    case CAKE_MOUSE_MIDDLE: return "MIDDLE";
    case CAKE_MOUSE_X1:     return "X1";
    case CAKE_MOUSE_X2:     return "X2";

    default:                return "UNKNOWN";
    }
}

const char* CAKE_ControllerButtonName(uint16_t mask) {
    switch (mask) {
    case CAKE_BUTTON_A:              return "A";
    case CAKE_BUTTON_B:              return "B";
    case CAKE_BUTTON_X:              return "X";
    case CAKE_BUTTON_Y:              return "Y";
    case CAKE_BUTTON_START:          return "START";
    case CAKE_BUTTON_BACK:           return "BACK";
    case CAKE_BUTTON_DPAD_UP:        return "DPAD_UP";
    case CAKE_BUTTON_DPAD_DOWN:      return "DPAD_DOWN";
    case CAKE_BUTTON_DPAD_LEFT:      return "DPAD_LEFT";
    case CAKE_BUTTON_DPAD_RIGHT:     return "DPAD_RIGHT";
    case CAKE_BUTTON_LEFT_SHOULDER:  return "LB";
    case CAKE_BUTTON_RIGHT_SHOULDER: return "RB";
    case CAKE_BUTTON_LEFT_THUMB:     return "LEFT_THUMB";
    case CAKE_BUTTON_RIGHT_THUMB:    return "RIGHT_THUMB";

    default:                         return NULL;
    }
}