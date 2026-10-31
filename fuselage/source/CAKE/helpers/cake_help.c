// Published input queries. SEE: (CAKE.md, Published input)
#include "../cake_help.h"

uint16_t CAKE_GetControllerButtons(int slot) {
    const CAKE_ControllerState* controller_state = CAKE_GetControllerState(slot);
    return controller_state ? controller_state->buttons : 0;
}
int16_t CAKE_GetControllerAxis(int slot, CAKE_ControllerAxis axis) {
    const CAKE_ControllerState* controller_state = CAKE_GetControllerState(slot);
    if (!controller_state) {
        return 0;
    }
    switch (axis) {
    case CAKE_AXIS_LEFT_X:
        return controller_state->thumbLeftX;
    case CAKE_AXIS_LEFT_Y:
        return controller_state->thumbLeftY;
    case CAKE_AXIS_RIGHT_X:
        return controller_state->thumbRightX;
    case CAKE_AXIS_RIGHT_Y:
        return controller_state->thumbRightY;
    default:
        return 0;
    }
}
uint8_t CAKE_GetControllerTrigger(int slot, CAKE_ControllerTrigger trigger) {
    const CAKE_ControllerState* controller_state = CAKE_GetControllerState(slot);
    if (!controller_state) {
        return 0;
    }
    return trigger == CAKE_TRIGGER_RIGHT ? controller_state->rightTrigger : controller_state->leftTrigger;
}

bool CAKE_IsKeyPressed(uint8_t key_code) {
    return CAKE_Keys[key_code];
}

bool CAKE_WasKeyPressed(uint8_t key_code) {
    return CAKE_KeysPressed[key_code];
}

bool CAKE_WasKeyReleased(uint8_t key_code) {
    return CAKE_KeysReleased[key_code];
}

bool CAKE_IsMouseButtonPressed(int button) {
    return button >= 0 && button < CAKE_MOUSE_BUTTON_COUNT && CAKE_MouseButtons[button];
}

bool CAKE_WasMouseButtonPressed(int button) {
    return button >= 0 && button < CAKE_MOUSE_BUTTON_COUNT && CAKE_MouseButtonsPressed[button];
}

bool CAKE_WasMouseButtonReleased(int button) {
    return button >= 0 && button < CAKE_MOUSE_BUTTON_COUNT && CAKE_MouseButtonsReleased[button];
}

bool CAKE_IsShiftPressed(void) {
    return CAKE_Keys[CAKE_KEY_LSHIFT] || CAKE_Keys[CAKE_KEY_RSHIFT];
}

bool CAKE_IsControlPressed(void) {
    return CAKE_Keys[CAKE_KEY_LCTRL] || CAKE_Keys[CAKE_KEY_RCTRL];
}

bool CAKE_IsAltPressed(void) {
    return CAKE_Keys[CAKE_KEY_LALT] || CAKE_Keys[CAKE_KEY_RALT];
}

bool CAKE_IsControllerButtonPressed(int slot, uint16_t mask) {
    const CAKE_ControllerState* controller_state = CAKE_GetControllerState(slot);

    return controller_state && (controller_state->buttons & mask) != 0;
}

bool CAKE_WasControllerButtonPressed(int slot, uint16_t mask) {
    if (slot < 0 || slot >= CAKE_CONTROLLER_MAX) {
        return false;
    }

    return (CAKE_PadButtonsPressed[slot] & mask) != 0;
}

bool CAKE_WasControllerButtonReleased(int slot, uint16_t mask) {
    if (slot < 0 || slot >= CAKE_CONTROLLER_MAX) {
        return false;
    }

    return (CAKE_PadButtonsReleased[slot] & mask) != 0;
}
