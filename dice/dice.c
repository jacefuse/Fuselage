// dice.c - DICE lifecycle. Version see DICE_VERSION in dice.h.

#include "dice.h"

static bool dice_ready = false;

bool DICE_Init(void) {
    if (dice_ready) { return true; }

    dice_ready = true;

    return true;
}

void DICE_Shutdown(void) {
    if (!dice_ready) { return; }

    for (int h = 0; h < DICE_MAX_TIMERS; h++) {
        DICE_ReleaseTimer((uint8_t)h);
    }

    for (int h = 0; h < DICE_MAX_RNG; h++) {
        DICE_ReleaseRNG((uint8_t)h);
    }

    dice_ready = false;

    return;
}