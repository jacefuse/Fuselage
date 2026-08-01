// CAKE helper definitions and functions

#ifndef CAKE_HELP_H
#define CAKE_HELP_H

#include "cake.h"

// Human-readable label for a key / mouse button / controller button constant.
// Returns "UNKNOWN" (never NULL) for an unrecognized code, so the result is
// always safe to print directly.
const char* CAKE_KeyName(uint8_t keycode);
const char* CAKE_MouseButtonName(int btn);
const char* CAKE_ControllerButtonName(uint16_t mask);

// Scalar views of a controller's live state (from CAKE_GetControllerState),
// so callers can read a single field without dereferencing the state struct
// themselves. All return 0 if the slot isn't connected.
uint16_t CAKE_GetControllerButtons(int slot);
int16_t  CAKE_GetControllerAxis(int slot, CAKE_ControllerAxis axis);
uint8_t  CAKE_GetControllerTrigger(int slot, CAKE_ControllerTrigger trigger);

#endif // CAKE_HELP_H