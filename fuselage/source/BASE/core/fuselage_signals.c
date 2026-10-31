/* Signal admission is atomic; the render thread applies transitions. */
#include "fuselage_internal.h"
#include <stdio.h>
void FuselageSignal(FuselageSignalType signal) {
    switch (signal) {
    case FUSELAGE_QUIT:
        atomic_store(&fuselage_root.signal_quit, true);
        break;
    case FUSELAGE_WINDOWED:
        atomic_store(&fuselage_root.signal_windowed, true);
        break;
    case FUSELAGE_BORDERLESS:
        atomic_store(&fuselage_root.signal_borderless, true);
        break;
    case FUSELAGE_FULLSCREEN_EXCLUSIVE:
        atomic_store(&fuselage_root.signal_exclusive, true);
        break;
    case FUSELAGE_APPLY_TARGET_FPS:
        atomic_store(&fuselage_root.signal_cap, true);
        break;
    }
}
bool fuselage_create_owned_timer(uint8_t handle, double rate, DICE_TimerCallback callback) {
    if (!DICE_UnlockTimer(handle)) {
        return false;
    }
    bool created = DICE_InitTimer(handle, rate, false, callback, NULL);
    bool locked = DICE_LockTimer(handle);
    return created && locked;
}
bool fuselage_release_owned_timer(uint8_t handle) {
    if (!DICE_UnlockTimer(handle)) {
        return false;
    }
    bool released = DICE_ReleaseTimer(handle);
    bool locked = DICE_LockTimer(handle);
    return released && locked;
}
void fuselage_process_signals(void) {
    if (atomic_exchange(&fuselage_root.signal_quit, false)) {
        atomic_store(&fuselage_root.phase, FUSELAGE_STOPPING);
        return;
    }
    if (atomic_exchange(&fuselage_root.signal_windowed, false)) {
        GDMF_SetDisplayMode(GDMF_MODE_WINDOWED);
    }
    if (atomic_exchange(&fuselage_root.signal_borderless, false)) {
        GDMF_SetDisplayMode(GDMF_MODE_BORDERLESS);
    }
    if (atomic_exchange(&fuselage_root.signal_exclusive, false)) {
        GDMF_SetDisplayMode(GDMF_MODE_FULLSCREEN_EXCLUSIVE);
    }
    if (atomic_exchange(&fuselage_root.signal_cap, false)) {
        if (fuselage_root.target_timer_active) {
            if (!fuselage_release_owned_timer(FUSELAGE_CAP_TIMER)) {
                fprintf(stderr, "[Fuselage] Could not release the render-cap timer.\n");
                fuselage_root.exit_code = FUSELAGE_EXIT_FAILED;
                atomic_store(&fuselage_root.phase, FUSELAGE_STOPPING);
                return;
            }
            fuselage_root.target_timer_active = false;
        }
        double fps = atomic_load(&fuselage_root.target_fps);
        if (fps > 0) {
            if (fuselage_create_owned_timer(FUSELAGE_CAP_TIMER, fps, fuselage_cap_tick) &&
                DICE_StartTimer(FUSELAGE_CAP_TIMER)) {
                fuselage_root.target_timer_active = true;
            } else {
                fprintf(stderr, "[Fuselage] Could not apply the render cap; stopping safely.\n");
                fuselage_root.exit_code = FUSELAGE_EXIT_FAILED;
                atomic_store(&fuselage_root.phase, FUSELAGE_STOPPING);
            }
        }
    }
}
