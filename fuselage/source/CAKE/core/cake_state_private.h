#ifndef CAKE_STATE_PRIVATE_H
#define CAKE_STATE_PRIVATE_H

#include "../cake.h"

// Call only for observed state changes, under the backend's intake lock if used.
void cake_latch_key(uint8_t key_code, bool down);
void cake_latch_mouse_button(int button, bool down);

// Publish and clear intake latches under the same lock used by their writers.
void cake_publish_input_edges(void);
// Call once after the backend refreshes controller state.
void cake_derive_controller_edges(void);

// Resume's shared reset; the backend separately clears its pending input.
// Does not reset controllers, PollCount or key-repeat history.
void cake_clear_keyboard_mouse_state(void);

// These functions add no locks. SEE: (CAKE.md, Backend state publication)
#endif
