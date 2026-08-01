#ifndef DICE_RNG_H
#define DICE_RNG_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// DICE RNG
//
// Up to DICE_MAX_RNG independent random number streams. Each stream owns
// its own state. There is no shared global stream, no libc rand(). Every
// stream is consumed through the exact same API (DICE_RandInt,
// DICE_RandFloat, ...) regardless of how it's configured; a caller
// (including the VPU/MISL) that only knows a handle has no way to tell
// a locked-seed replay stream from a continuously-perturbed one. The
// difference lives entirely in how the stream was configured at
// DICE_InitRNG.

#define DICE_MAX_RNG 32

typedef enum {
    // Same seed -> byte-identical sequence, forever. For replays, procgen,
    // tests; anything that needs to be reproduced exactly.
    DICE_RNG_DETERMINISTIC,

    // Seeded once from OS entropy (not caller-supplied), deterministic
    // from that point on. Not meaningfully reproducible without knowing
    // the seed DICE drew, but otherwise behaves like DETERMINISTIC.
    DICE_RNG_ENTROPY_SEEDED,

    // Every draw pulls a small amount of fresh OS entropy into the state
    // before generating output. For streams that should never be
    // predictable even under close observation of past outputs.
    DICE_RNG_CONTINUOUS,

    // Never touched automatically. Grows only via explicit
    // DICE_MixRNGEntropy calls, which fold new entropy into the existing
    // state (old + new) rather than replacing it for callers that want
    // to feed in their own entropy sources (input jitter, network timing,
    // whatever) on their own schedule.
    DICE_RNG_ACCUMULATE,
} DICE_RNGMode;

// Lifecycle
// seed is used verbatim for DICE_RNG_DETERMINISTIC, and as an initial
// mix-in (rather than the sole seed) for the other three modes. Pass 0 if
// the mode doesn't need a caller-chosen seed.
bool DICE_InitRNG(uint8_t handle, DICE_RNGMode mode, uint64_t seed);
bool DICE_ReleaseRNG(uint8_t handle);  // false if handle is out of range

// Folds data into the stream's existing state (does not replace it). Valid
// regardless of mode. This is useful even on a DETERMINISTIC stream that
// wants a one-time, deliberate perturbation at a specific moment. Returns
// false if handle is out of range, data is NULL, or len is 0.
bool DICE_MixRNGEntropy(uint8_t handle, const void* data, size_t len);

// Returns the seed the stream was initialized with, for logging/replay of
// DETERMINISTIC streams. Meaningless (but harmless) to call on other modes.
uint64_t DICE_GetRNGSeed(uint8_t handle);

// Consumption
uint32_t DICE_RandUint(uint8_t handle);                       // full-range raw draw
int      DICE_RandInt(uint8_t handle, int min, int max);      // inclusive [min, max]
float    DICE_RandFloat(uint8_t handle);                      // [0, 1)
bool     DICE_RandChance(uint8_t handle, float probability);  // true w/ given probability

#endif // DICE_RNG_H