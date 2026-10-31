// Stream-backed dice. SEE: (DICE.md, Dice helpers)
#include "../dice_help.h"
#include "dice_help_private.h"
#include "../rng/dice_rng_private.h"
static bool dice_seeded_dice[DICE_MAX_DICE];

void dice_help_reset(void) {
    for (int die_index = 0; die_index < DICE_MAX_DICE; die_index++) {
        dice_seeded_dice[die_index] = false;
    }

    return;
}

static void dice_help_ensure_seeded(uint8_t die) {
    if (die >= DICE_MAX_DICE) {
        return;
    }
    if (dice_seeded_dice[die] && dice_rng_is_initialized(die)) {
        return;
    }

    dice_seeded_dice[die] = DICE_InitRNG(die, DICE_RNG_ENTROPY_SEEDED, 0);

    return;
}

bool DICE_SeedDie(uint8_t die, uint64_t seed) {
    if (die >= DICE_MAX_DICE) {
        return false;
    }

    bool initialized = DICE_InitRNG(die, DICE_RNG_DETERMINISTIC, seed);
    dice_seeded_dice[die] = initialized;

    return initialized;
}

int DICE_RollDieRange(uint8_t die, int minimum, int maximum) {
    dice_help_ensure_seeded(die);

    return DICE_RandInt(die, minimum, maximum);
}

float DICE_RollDieFloat(uint8_t die) {
    dice_help_ensure_seeded(die);

    return DICE_RandFloat(die);
}

bool DICE_RollDieChance(uint8_t die, float probability) {
    dice_help_ensure_seeded(die);

    return DICE_RandChance(die, probability);
}
