// System lifecycle. SEE: (DICE.md, Lifecycle)
#include "../dice.h"
#include "../clock/dice_clock_private.h"
#include "../helpers/dice_help_private.h"
#include "../timers/dice_timers_private.h"
#include "../rng/dice_rng_private.h"
#include "dice_lifecycle_private.h"
#include <stdio.h>
static bool dice_initialized;
bool DICE_Init(void) {
    if (dice_initialized) {
        return true;
    }

    dice_clock_start();
    dice_initialized = true;

    return true;
}

bool dice_shutdown_checked(void) {
    if (dice_timer_is_worker_thread()) {
        return false;
    }
    if (!dice_initialized && !dice_timers_has_resources() && !dice_rng_has_resources()) {
        return true;
    }
    if (!dice_timers_shutdown()) {
        return false;
    }
    for (int handle = 0; handle < DICE_MAX_RNG; ++handle) {
        DICE_ReleaseRNG((uint8_t)handle);
    }
    dice_help_reset();
    dice_clock_stop();
    dice_initialized = false;
    return true;
}
void DICE_Shutdown(void) {
    if (!dice_shutdown_checked()) {
        fprintf(stderr, "[DICE] Shutdown incomplete; remaining resources retained\n");
    }
}
