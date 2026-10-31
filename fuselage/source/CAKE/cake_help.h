// Labels, state queries and poll-based repeat. SEE: (CAKE.md, Overview)
#ifndef CAKE_HELP_H
#define CAKE_HELP_H

#include "cake.h"

// Labels return "UNKNOWN" for unrecognized codes.
const char* CAKE_GetKeyName(uint8_t key_code);
const char* CAKE_GetMouseButtonName(int button);
const char* CAKE_GetControllerButtonName(uint16_t mask);

// Scalar queries return zero when no controller state is available.
uint16_t CAKE_GetControllerButtons(int slot);
int16_t CAKE_GetControllerAxis(int slot, CAKE_ControllerAxis axis);
uint8_t CAKE_GetControllerTrigger(int slot, CAKE_ControllerTrigger trigger);

// Is reads published state; Was reads the latest poll edges. Neither consumes.
// SEE: (CAKE.md, Published input)
bool CAKE_IsKeyPressed(uint8_t key_code);
bool CAKE_WasKeyPressed(uint8_t key_code);
bool CAKE_WasKeyReleased(uint8_t key_code);

bool CAKE_IsMouseButtonPressed(int button);
bool CAKE_WasMouseButtonPressed(int button);
bool CAKE_WasMouseButtonReleased(int button);

// Multiple mask bits ask whether any selected button matches.
bool CAKE_IsControllerButtonPressed(int slot, uint16_t mask);
bool CAKE_WasControllerButtonPressed(int slot, uint16_t mask);
bool CAKE_WasControllerButtonReleased(int slot, uint16_t mask);

// Either physical modifier key satisfies the query.
bool CAKE_IsShiftPressed(void);
bool CAKE_IsControlPressed(void);
bool CAKE_IsAltPressed(void);

// Poll-based repeat. SEE: (CAKE.md, Key repeat)
bool CAKE_CheckKeyRepeat(uint8_t key_code, unsigned delay_polls, unsigned interval_polls);

#endif
