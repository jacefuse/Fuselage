#ifndef DICE_TIMERS_PRIVATE_H
#define DICE_TIMERS_PRIVATE_H
#include "../dice_timers.h"
#include <limits.h>
#include <math.h>
typedef enum {
    DICE_TSTATE_STOPPED,
    DICE_TSTATE_RUNNING,
    DICE_TSTATE_PAUSED,
    DICE_TSTATE_RELEASING,
} dice_timer_state;
extern bool dice_timer_claimed[DICE_MAX_TIMERS];
bool dice_timer_valid(uint8_t handle);
bool dice_timer_is_worker_thread(void);
void dice_timer_worker_enter(void);
void dice_timer_worker_leave(void);
bool dice_timers_has_resources(void);
bool dice_timers_shutdown(void);
static inline bool dice_timer_add(int64_t left, int64_t right, int64_t* result) {
    if ((right > 0 && left > INT64_MAX - right) || (right < 0 && left < INT64_MIN - right)) {
        return false;
    }
    *result = left + right;
    return true;
}
static inline bool dice_timer_integer(double value, int64_t* result) {
    if (!isfinite(value) || value < -0x1p63 || value >= 0x1p63) {
        return false;
    }
    *result = (int64_t)value;
    return true;
}
#endif
