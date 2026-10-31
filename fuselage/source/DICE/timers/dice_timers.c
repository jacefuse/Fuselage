// Handle ownership and checked teardown. SEE: (DICE.md, Timers and callbacks)
#include "dice_timers_private.h"
bool dice_timer_claimed[DICE_MAX_TIMERS];
static _Thread_local bool dice_timer_worker_context;
bool dice_timer_is_worker_thread(void) {
    return dice_timer_worker_context;
}
void dice_timer_worker_enter(void) {
    dice_timer_worker_context = true;
}
void dice_timer_worker_leave(void) {
    dice_timer_worker_context = false;
}
bool DICE_LockTimer(uint8_t handle) {
    if (handle >= DICE_MAX_TIMERS || dice_timer_claimed[handle]) {
        return false;
    }
    dice_timer_claimed[handle] = true;
    return true;
}
bool DICE_UnlockTimer(uint8_t handle) {
    if (handle >= DICE_MAX_TIMERS || !dice_timer_claimed[handle]) {
        return false;
    }
    dice_timer_claimed[handle] = false;
    return true;
}
bool DICE_IsTimerLocked(uint8_t handle) {
    return handle < DICE_MAX_TIMERS && dice_timer_claimed[handle];
}
int DICE_AllocTimer(void) {
    for (uint8_t handle = 0; handle < DICE_MAX_TIMERS; ++handle) {
        if (!dice_timer_claimed[handle] && !dice_timer_valid(handle)) {
            return handle;
        }
    }
    return -1;
}
bool dice_timers_has_resources(void) {
    for (uint8_t handle = 0; handle < DICE_MAX_TIMERS; ++handle) {
        if (dice_timer_claimed[handle] || dice_timer_valid(handle)) {
            return true;
        }
    }
    return false;
}
bool dice_timers_shutdown(void) {
    if (dice_timer_is_worker_thread()) {
        return false;
    }
    for (uint8_t handle = 0; handle < DICE_MAX_TIMERS; ++handle) {
        dice_timer_claimed[handle] = false;
        if (dice_timer_valid(handle) && !DICE_ReleaseTimer(handle)) {
            return false;
        }
    }
    return true;
}
