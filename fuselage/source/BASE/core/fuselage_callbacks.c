/* DICE workers publish events or run the serialized simulation callback. */
#include "fuselage_internal.h"
#include "../pacing/fuselage_pacing.h"
void fuselage_simulation_tick(uint8_t handle, void* context) {
    (void)handle;
    (void)context;
    if (atomic_load(&fuselage_root.phase) != FUSELAGE_RUNNING) {
        return;
    }
    fuselage_mutex_lock(&fuselage_root.state_lock);
    if (atomic_load(&fuselage_root.phase) != FUSELAGE_RUNNING) {
        fuselage_mutex_unlock(&fuselage_root.state_lock);
        return;
    }
    uint64_t begin_us = DICE_GetElapsedMicroseconds();
    bool focused = GDMF_IsFocused();
    bool policy_changed = atomic_exchange(&fuselage_root.focus_policy_changed, false);
    bool required = atomic_load(&fuselage_root.input_requires_focus);
    if (policy_changed || (required && focused != fuselage_root.previously_focused)) {
        if (required && !focused) {
            CAKE_Silence();
        } else {
            CAKE_Resume();
        }
    }
    fuselage_root.previously_focused = focused;
    CAKE_Poll();
    FuselageTickCallback callback = atomic_load(&fuselage_root.tick_callback);
    if (callback) {
        callback();
    }
    fuselage_pacing_tick(begin_us, DICE_GetElapsedMicroseconds());
    fuselage_mutex_unlock(&fuselage_root.state_lock);
}
void fuselage_frame_tick(uint8_t handle, void* context) {
    (void)handle;
    (void)context;
    fuselage_event_set(&fuselage_root.frame_event);
}
void fuselage_cap_tick(uint8_t handle, void* context) {
    (void)handle;
    (void)context;
    fuselage_event_set(&fuselage_root.cap_event);
}
