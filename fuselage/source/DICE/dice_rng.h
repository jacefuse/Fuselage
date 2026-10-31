// Independent PCG32 streams. SEE: (DICE.md, Random streams)
#ifndef DICE_RNG_H
#define DICE_RNG_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define DICE_MAX_RNG 32

typedef enum {
    DICE_RNG_DETERMINISTIC,
    DICE_RNG_ENTROPY_SEEDED,
    DICE_RNG_CONTINUOUS,
    DICE_RNG_ACCUMULATE,
} DICE_RNGMode;

// Invalid handle/mode returns false without replacing an existing stream.
bool DICE_InitRNG(uint8_t handle, DICE_RNGMode mode, uint64_t seed);
// False for an invalid or uninitialized handle.
bool DICE_ReleaseRNG(uint8_t handle);

// False for an invalid stream, NULL data or zero bytes; otherwise advances state.
bool DICE_MixRNGEntropy(uint8_t handle, const void* data, size_t len);

// Original caller seed, or zero for an invalid stream.
uint64_t DICE_GetRNGSeed(uint8_t handle);

uint32_t DICE_RandUint(uint8_t handle);
// Inclusive bounds; reversed bounds are exchanged for a valid stream.
int DICE_RandInt(uint8_t handle, int min, int max);
float DICE_RandFloat(uint8_t handle);
bool DICE_RandChance(uint8_t handle, float probability);

#endif
