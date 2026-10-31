#ifndef DICE_TIMERS_INTERNAL_H
#define DICE_TIMERS_INTERNAL_H
#include <stdbool.h>
#include <stdint.h>
// Pacing-controller service, outside the public API. Caller owns the handle.
// Retunes the period and shifts the next target/paused remainder in nanoseconds.
// Ignores the ownership flag. Callback use is allowed; no worker recreation.
// False for an invalid/retiring timer, nonpositive period or unrepresentable change.
// SEE: (DICE.md, Timers and callbacks)
bool dice_timer_retune(uint8_t handle, int64_t period_nanoseconds, int64_t phase_shift_nanoseconds);
#endif
