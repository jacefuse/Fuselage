#pragma once
#include "misl_guest_memory.h"

// Per-pump input accessors and input ecalls.
// SEE: (MISL_GUEST.md, Input snapshots and calls)

// Snapshot accessors require a discovered input page. Controller indices must be valid.

static inline int MISL_IsKeyPressed(int scancode) {
    return MISL_GetInput()->keys[scancode & 0xFF];
}

static inline int MISL_WasKeyPressed(int scancode) {
    return MISL_GetInput()->keysPressed[scancode & 0xFF];
}

static inline int MISL_WasKeyReleased(int scancode) {
    return MISL_GetInput()->keysReleased[scancode & 0xFF];
}

static inline int MISL_GetMouseX(void) {
    return MISL_GetInput()->mouseX;
}

static inline int MISL_GetMouseY(void) {
    return MISL_GetInput()->mouseY;
}

static inline int MISL_GetMouseWheel(void) {
    return MISL_GetInput()->mouseWheel;
}

static inline int MISL_GetCanvasMouseX(void) {
    return MISL_GetInput()->canvasMouseX;
}

static inline int MISL_GetCanvasMouseY(void) {
    return MISL_GetInput()->canvasMouseY;
}

static inline uint64_t MISL_GetInputElapsedMicroseconds(void) {
    return MISL_GetInput()->elapsedMicroseconds;
}

static inline double MISL_GetInputElapsedSeconds(void) {
    return (double)MISL_GetInput()->elapsedMicroseconds / 1000000.0;
}

static inline int MISL_IsMouseButtonPressed(int button) {
    return MISL_GetInput()->mouseButtons[button & 7];
}

static inline int MISL_WasMouseButtonPressed(int button) {
    return MISL_GetInput()->mouseButtonsPressed[button & 7];
}

static inline int MISL_WasMouseButtonReleased(int button) {
    return MISL_GetInput()->mouseButtonsReleased[button & 7];
}

static inline int MISL_IsControllerConnected(int slot) {
    return MISL_GetInput()->pads[slot].connected;
}

static inline int MISL_IsControllerButtonPressed(int slot, int mask) {
    return (MISL_GetInput()->pads[slot].buttons & mask) != 0;
}

static inline int MISL_WasControllerButtonPressed(int slot, int mask) {
    return (MISL_GetInput()->padButtonsPressed[slot & 3] & mask) != 0;
}

static inline int MISL_WasControllerButtonReleased(int slot, int mask) {
    return (MISL_GetInput()->padButtonsReleased[slot & 3] & mask) != 0;
}

static inline int MISL_GetControllerTrigger(int slot, int right_trigger) {
    return right_trigger ? MISL_GetInput()->pads[slot].rt : MISL_GetInput()->pads[slot].lt;
}

static inline int MISL_GetControllerAxis(int slot, int axis) {
    const volatile MislInput* input_snapshot = MISL_GetInput();
    switch (axis) {
    case 0:
        return input_snapshot->pads[slot].lx;
    case 1:
        return input_snapshot->pads[slot].ly;
    case 2:
        return input_snapshot->pads[slot].rx;
    case 3:
        return input_snapshot->pads[slot].ry;
    }
    return 0;
}

static inline bool CAKE_GetKey(int scancode) {
    return MISL_GUEST_CALL(MISL_CAKE_GET_KEY, scancode, 0, 0, 0, 0, 0, 0) != 0;
}

static inline int CAKE_GetMouseX(void) {
    return MISL_GUEST_CALL(MISL_CAKE_GET_MOUSE_X, 0, 0, 0, 0, 0, 0, 0);
}

static inline int CAKE_GetMouseY(void) {
    return MISL_GUEST_CALL(MISL_CAKE_GET_MOUSE_Y, 0, 0, 0, 0, 0, 0, 0);
}

static inline int CAKE_GetMouseWheel(void) {
    return MISL_GUEST_CALL(MISL_CAKE_GET_MOUSE_WHEEL, 0, 0, 0, 0, 0, 0, 0);
}

static inline bool CAKE_GetMouseButton(int button) {
    return MISL_GUEST_CALL(MISL_CAKE_GET_MOUSE_BUTTON, button, 0, 0, 0, 0, 0, 0) != 0;
}

static inline int CAKE_IsSilenced(void) {
    return MISL_GUEST_CALL(MISL_CAKE_IS_SILENCED, 0, 0, 0, 0, 0, 0, 0);
}

static inline int CAKE_IsControllerConnected(int slot) {
    return MISL_GUEST_CALL(MISL_CAKE_CTRL_CONNECTED, slot, 0, 0, 0, 0, 0, 0);
}

static inline int CAKE_GetControllerConnectionState(int slot) {
    return MISL_GUEST_CALL(MISL_CAKE_CTRL_CONN_STATE, slot, 0, 0, 0, 0, 0, 0);
}

static inline int CAKE_GetControllerBackend(int slot) {
    return MISL_GUEST_CALL(MISL_CAKE_CTRL_BACKEND, slot, 0, 0, 0, 0, 0, 0);
}

static inline int CAKE_GetControllerVendorID(int slot) {
    return MISL_GUEST_CALL(MISL_CAKE_CTRL_VENDOR, slot, 0, 0, 0, 0, 0, 0);
}

static inline int CAKE_GetControllerProductID(int slot) {
    return MISL_GUEST_CALL(MISL_CAKE_CTRL_PRODUCT, slot, 0, 0, 0, 0, 0, 0);
}

static inline int CAKE_GetControllerXInputIndex(int slot) {
    return MISL_GUEST_CALL(MISL_CAKE_CTRL_XINPUT_IDX, slot, 0, 0, 0, 0, 0, 0);
}

static inline int CAKE_SetControllerVibration(int slot, uint16_t left_motor, uint16_t right_motor) {
    return MISL_GUEST_CALL(MISL_CAKE_CTRL_VIBRATION, slot, left_motor, right_motor, 0, 0, 0, 0);
}

static inline int CAKE_GetControllerName(int slot, char* name, int capacity) {
    return MISL_GUEST_CALL(MISL_CAKE_CTRL_COPY_NAME, slot, misl_guest_address(name), capacity, 0, 0, 0, 0);
}
