// Position-addressable values. SEE: (DICE.md, Positional sequences)
#include "../dice_help.h"
#include <limits.h>
_Static_assert(INT_MAX == INT32_MAX && INT_MIN == INT32_MIN, "DICE sequence ranges require 32-bit int");
typedef struct {
    bool configured;
    uint64_t seed;
} dice_sequence;

static dice_sequence dice_sequences[DICE_MAX_SEQUENCES];

static uint64_t dice_sequence_mix(uint64_t value) {
    value += 0x9E3779B97F4A7C15ULL;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;

    return value ^ (value >> 31);
}

static uint64_t dice_sequence_hash(uint64_t seed, uint64_t position) {
    uint64_t hash = dice_sequence_mix(position);
    hash ^= seed;

    return dice_sequence_mix(hash);
}

static uint64_t dice_sequence_seed(uint8_t sequence) {
    if (dice_sequences[sequence].configured) {
        return dice_sequences[sequence].seed;
    }

    return (uint64_t)sequence;
}

bool DICE_SeedSequence(uint8_t sequence, uint64_t seed) {
    if (sequence >= DICE_MAX_SEQUENCES) {
        return false;
    }

    dice_sequences[sequence].seed = seed;
    dice_sequences[sequence].configured = true;

    return true;
}

uint32_t DICE_GetSequenceUint(uint8_t sequence, uint64_t position) {
    if (sequence >= DICE_MAX_SEQUENCES) {
        return 0;
    }

    uint64_t hash = dice_sequence_hash(dice_sequence_seed(sequence), position);

    return (uint32_t)(hash >> 32);
}

int DICE_GetSequenceInt(uint8_t sequence, uint64_t position, int minimum, int maximum) {
    if (sequence >= DICE_MAX_SEQUENCES) {
        return minimum;
    }

    if (maximum < minimum) {
        int saved_bound = minimum;
        minimum = maximum;
        maximum = saved_bound;
    }

    uint32_t range = (uint32_t)((int64_t)maximum - (int64_t)minimum) + 1u;
    if (range == 0u) {

        uint32_t draw = DICE_GetSequenceUint(sequence, position);
        return draw <= INT32_MAX ? (int)draw : (int)((int64_t)draw - INT64_C(4294967296));
    }

    uint32_t draw = DICE_GetSequenceUint(sequence, position);
    uint64_t scaled = (uint64_t)draw * (uint64_t)range;

    return (int)((int64_t)minimum + (int64_t)(scaled >> 32));
}

float DICE_GetSequenceFloat(uint8_t sequence, uint64_t position) {
    if (sequence >= DICE_MAX_SEQUENCES) {
        return 0.0f;
    }

    return (float)(DICE_GetSequenceUint(sequence, position) >> 8) * (1.0f / 16777216.0f);
}

bool DICE_GetSequenceChance(uint8_t sequence, uint64_t position, float probability) {
    if (probability <= 0.0f) {
        return false;
    }
    if (probability >= 1.0f) {
        return true;
    }

    return DICE_GetSequenceFloat(sequence, position) < probability;
}
