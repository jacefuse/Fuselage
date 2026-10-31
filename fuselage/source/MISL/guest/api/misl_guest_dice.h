#pragma once
#include "misl_guest_call_internal.h"
#include "../../abi/misl_mmio_input.h"

// Random values, polling timers and positional sequences.
// SEE: (MISL_GUEST.md, Random values and timers)

static inline bool DICE_InitRNG(uint8_t handle, unsigned mode, uint64_t seed) {
    return MISL_GUEST_CALL(MISL_DICE_INIT_RNG, handle, mode, MISL_GUEST_LOW(seed), MISL_GUEST_HIGH(seed), 0,
                           0, 0) != 0;
}

static inline bool DICE_ReleaseRNG(uint8_t handle) {
    return MISL_GUEST_CALL(MISL_DICE_RELEASE_RNG, handle, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool DICE_MixRNGEntropy(uint8_t handle, const void* data, uint32_t len) {
    return MISL_GUEST_CALL(MISL_DICE_RNG_MIX_ENTROPY, handle, misl_guest_address(data), len, 0, 0, 0, 0) != 0;
}

static inline uint64_t DICE_GetRNGSeed(uint8_t handle) {
    return MISL_GUEST_CALL64(MISL_DICE_RNG_GET_SEED, handle, 0, 0, 0, 0, 0, 0);
}

static inline uint32_t DICE_RandUint(uint8_t handle) {
    return (uint32_t)MISL_GUEST_CALL(MISL_DICE_RAND_UINT, handle, 0, 0, 0, 0, 0, 0);
}

static inline int DICE_RandInt(uint8_t handle, int min, int max) {
    return MISL_GUEST_CALL(MISL_DICE_RAND_INT, handle, min, max, 0, 0, 0, 0);
}

static inline float DICE_RandFloat(uint8_t handle) {
    return misl_guest_bits_float(MISL_GUEST_CALL(MISL_DICE_RAND_FLOAT, handle, 0, 0, 0, 0, 0, 0));
}

static inline bool DICE_RandChance(uint8_t handle, float probability) {
    return MISL_GUEST_CALL(MISL_DICE_RAND_CHANCE, handle, misl_guest_float_bits(probability), 0, 0, 0, 0,
                           0) != 0;
}

static inline bool DICE_InitTimer(uint8_t handle, uint64_t microseconds, bool autostart) {
    return MISL_GUEST_CALL(MISL_DICE_INIT_TIMER, handle, MISL_GUEST_LOW(microseconds),
                           MISL_GUEST_HIGH(microseconds), autostart, 0, 0, 0) != 0;
}

static inline bool DICE_ReleaseTimer(uint8_t handle) {
    return MISL_GUEST_CALL(MISL_DICE_RELEASE_TIMER, handle, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool DICE_StartTimer(uint8_t handle) {
    return MISL_GUEST_CALL(MISL_DICE_TIMER_START, handle, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool DICE_StopTimer(uint8_t handle) {
    return MISL_GUEST_CALL(MISL_DICE_TIMER_STOP, handle, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool DICE_PauseTimer(uint8_t handle) {
    return MISL_GUEST_CALL(MISL_DICE_TIMER_PAUSE, handle, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool DICE_ResumeTimer(uint8_t handle) {
    return MISL_GUEST_CALL(MISL_DICE_TIMER_RESUME, handle, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool DICE_IsTimerRunning(uint8_t handle) {
    return MISL_GUEST_CALL(MISL_DICE_TIMER_IS_RUNNING, handle, 0, 0, 0, 0, 0, 0) != 0;
}

static inline uint32_t DICE_ConsumeTimer(uint8_t handle) {
    return (uint32_t)MISL_GUEST_CALL(MISL_DICE_TIMER_CONSUME, handle, 0, 0, 0, 0, 0, 0);
}

static inline double DICE_GetTimerDelta(uint8_t handle) {
    return misl_guest_bits_double(MISL_GUEST_CALL64(MISL_DICE_TIMER_DELTA, handle, 0, 0, 0, 0, 0, 0));
}

static inline bool DICE_SeedDie(uint8_t die, uint64_t seed) {
    return MISL_GUEST_CALL(MISL_DICE_SEED_DIE, die, MISL_GUEST_LOW(seed), MISL_GUEST_HIGH(seed), 0, 0, 0,
                           0) != 0;
}

static inline int DICE_RollDieRange(uint8_t die, int min, int max) {
    return MISL_GUEST_CALL(MISL_DICE_DIE_RANGE, die, min, max, 0, 0, 0, 0);
}

static inline float DICE_RollDieFloat(uint8_t die) {
    return misl_guest_bits_float(MISL_GUEST_CALL(MISL_DICE_DIE_FLOAT, die, 0, 0, 0, 0, 0, 0));
}

static inline bool DICE_RollDieChance(uint8_t die, float probability) {
    return MISL_GUEST_CALL(MISL_DICE_DIE_CHANCE, die, misl_guest_float_bits(probability), 0, 0, 0, 0, 0) != 0;
}

static inline bool DICE_SeedSequence(uint8_t sequence, uint64_t seed) {
    return MISL_GUEST_CALL(MISL_DICE_SEED_SEQUENCE, sequence, MISL_GUEST_LOW(seed), MISL_GUEST_HIGH(seed), 0,
                           0, 0, 0) != 0;
}

static inline int DICE_AllocTimer(void) {
    return (int)MISL_GUEST_CALL(MISL_DICE_ALLOC_TIMER, 0, 0, 0, 0, 0, 0, 0);
}

static inline bool DICE_GetRealTime(MislClock* clock) {
    return MISL_GUEST_CALL(MISL_DICE_REAL_TIME, misl_guest_address(clock), 0, 0, 0, 0, 0, 0) != 0;
}

static inline uint32_t DICE_GetSequenceUint(uint8_t sequence, uint64_t position) {
    return (uint32_t)MISL_GUEST_CALL(MISL_DICE_SEQ_UINT, sequence, MISL_GUEST_LOW(position),
                                     MISL_GUEST_HIGH(position), 0, 0, 0, 0);
}

static inline int DICE_GetSequenceInt(uint8_t sequence, uint64_t position, int min, int max) {
    return MISL_GUEST_CALL(MISL_DICE_SEQ_INT, sequence, MISL_GUEST_LOW(position), MISL_GUEST_HIGH(position),
                           min, max, 0, 0);
}

static inline float DICE_GetSequenceFloat(uint8_t sequence, uint64_t position) {
    return misl_guest_bits_float(MISL_GUEST_CALL(MISL_DICE_SEQ_FLOAT, sequence, MISL_GUEST_LOW(position),
                                                 MISL_GUEST_HIGH(position), 0, 0, 0, 0));
}

static inline bool DICE_GetSequenceChance(uint8_t sequence, uint64_t position, float probability) {
    return MISL_GUEST_CALL(MISL_DICE_SEQ_CHANCE, sequence, MISL_GUEST_LOW(position),
                           MISL_GUEST_HIGH(position), misl_guest_float_bits(probability), 0, 0, 0) != 0;
}
