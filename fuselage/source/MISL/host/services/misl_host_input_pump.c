/* Input snapshot. SEE: (MISL_HOST_RUNTIME.md, Input pumps) */
#include "../runtime/misl_host_pages_internal.h"
#include "../runtime/misl_host_scheduler_internal.h"
#include "../../../CAKE/cake.h"
#include "../../../GDMF/gdmf.h"
#include "../../../DICE/dice.h"
#include <limits.h>
#include <string.h>
static int32_t fuselage_guest_canvas_coordinate(float value) {
    if (value != value) {
        return 0;
    }
    if (value <= -2147483648.0f) {
        return INT32_MIN;
    }
    if (value >= 2147483648.0f) {
        return INT32_MAX;
    }
    return (int32_t)value;
}
_Static_assert(MISL_NKEYS >= CAKE_KEY_TABLE_SIZE, "MMIO key page too small for CAKE's key table");
_Static_assert(MISL_NPADS >= CAKE_CONTROLLER_MAX, "MMIO pad array too small for CAKE's controller slots");
_Static_assert(MISL_NMOUSEBTN >= CAKE_MOUSE_BUTTON_COUNT, "MMIO mouse buttons too few for CAKE's");

_Static_assert(MISL_PAD_DPAD_UP == CAKE_BUTTON_DPAD_UP, "pad mask drift: DPAD_UP");
_Static_assert(MISL_PAD_DPAD_DOWN == CAKE_BUTTON_DPAD_DOWN, "pad mask drift: DPAD_DOWN");
_Static_assert(MISL_PAD_DPAD_LEFT == CAKE_BUTTON_DPAD_LEFT, "pad mask drift: DPAD_LEFT");
_Static_assert(MISL_PAD_DPAD_RIGHT == CAKE_BUTTON_DPAD_RIGHT, "pad mask drift: DPAD_RIGHT");
_Static_assert(MISL_PAD_START == CAKE_BUTTON_START, "pad mask drift: START");
_Static_assert(MISL_PAD_BACK == CAKE_BUTTON_BACK, "pad mask drift: BACK");
_Static_assert(MISL_PAD_LTHUMB == CAKE_BUTTON_LEFT_THUMB, "pad mask drift: LTHUMB");
_Static_assert(MISL_PAD_RTHUMB == CAKE_BUTTON_RIGHT_THUMB, "pad mask drift: RTHUMB");
_Static_assert(MISL_PAD_LSHOULDER == CAKE_BUTTON_LEFT_SHOULDER, "pad mask drift: LSHOULDER");
_Static_assert(MISL_PAD_RSHOULDER == CAKE_BUTTON_RIGHT_SHOULDER, "pad mask drift: RSHOULDER");
_Static_assert(MISL_PAD_A == CAKE_BUTTON_A, "pad mask drift: A");
_Static_assert(MISL_PAD_B == CAKE_BUTTON_B, "pad mask drift: B");
_Static_assert(MISL_PAD_X == CAKE_BUTTON_X, "pad mask drift: X");
_Static_assert(MISL_PAD_Y == CAKE_BUTTON_Y, "pad mask drift: Y");

void fuselage_guest_pump_input(void) {
    MislInput* input = (MislInput*)fuselage_guest_get_input_page();
    memset(input, 0, sizeof *input);

    input->elapsedMicroseconds = DICE_GetElapsedMicroseconds();

    for (int key = 0; key < CAKE_KEY_TABLE_SIZE; key++) {
        input->keys[key] = CAKE_Keys[key] ? 1u : 0u;
        input->keysPressed[key] = CAKE_KeysPressed[key] ? 1u : 0u;
        input->keysReleased[key] = CAKE_KeysReleased[key] ? 1u : 0u;
    }

    input->mouseX = CAKE_MouseDeltaX;
    input->mouseY = CAKE_MouseDeltaY;
    input->mouseWheel = CAKE_MouseWheel;

    {
        float canvas_x = 0, canvas_y = 0;
        GDMF_GetMousePosition(&canvas_x, &canvas_y);
        input->canvasMouseX = fuselage_guest_canvas_coordinate(canvas_x);
        input->canvasMouseY = fuselage_guest_canvas_coordinate(canvas_y);
    }

    memset(input->mouseButtons, 0, sizeof input->mouseButtons);
    memset(input->mouseButtonsPressed, 0, sizeof input->mouseButtonsPressed);
    memset(input->mouseButtonsReleased, 0, sizeof input->mouseButtonsReleased);
    for (int button = 0; button < CAKE_MOUSE_BUTTON_COUNT; button++) {
        input->mouseButtons[button] = CAKE_MouseButtons[button] ? 1u : 0u;
        input->mouseButtonsPressed[button] = CAKE_MouseButtonsPressed[button] ? 1u : 0u;
        input->mouseButtonsReleased[button] = CAKE_MouseButtonsReleased[button] ? 1u : 0u;
    }

    for (int pad = 0; pad < MISL_NPADS; pad++) {

        if (pad >= CAKE_CONTROLLER_MAX) {
            continue;
        }
        input->padButtonsPressed[pad] = CAKE_PadButtonsPressed[pad];
        input->padButtonsReleased[pad] = CAKE_PadButtonsReleased[pad];

        const CAKE_ControllerState* controller = CAKE_GetControllerState(pad);
        if (!controller) {
            memset(&input->pads[pad], 0, sizeof input->pads[pad]);
            continue;
        }
        input->pads[pad].connected = 1;
        input->pads[pad].lt = controller->leftTrigger;
        input->pads[pad].rt = controller->rightTrigger;
        input->pads[pad]._pad = 0;
        input->pads[pad].buttons = controller->buttons;
        input->pads[pad].lx = controller->thumbLeftX;
        input->pads[pad].ly = controller->thumbLeftY;
        input->pads[pad].rx = controller->thumbRightX;
        input->pads[pad].ry = controller->thumbRightY;
    }
}
