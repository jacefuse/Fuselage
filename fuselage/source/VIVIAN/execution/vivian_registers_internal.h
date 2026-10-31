/* Internal execution helpers. SEE: (VIVIAN.md, Floating-point helpers) */
#ifndef VIVIAN_REGISTERS_INTERNAL_H
#define VIVIAN_REGISTERS_INTERNAL_H
#include "../vivian.h"
#include <string.h>
#define VIVIAN_BOX_SINGLE(bits) (0xFFFFFFFF00000000ULL | (uint32_t)(bits))
#define VIVIAN_SINGLE_QUIET_NAN 0x7FC00000u
#define VIVIAN_DOUBLE_QUIET_NAN 0x7FF8000000000000ULL

/* The host preserves the initialized zero slot. SEE: (VIVIAN.md, State and ownership) */
static inline uint32_t vivian_read_register(const VivianState* state, uint32_t register_index) {
    return state->regs[register_index];
}

static inline void vivian_write_register(VivianState* state, uint32_t register_index, uint32_t value) {
    if (register_index != 0) {
        state->regs[register_index] = value;
    }
}

/* Bit moves read the low word without a NaN-box check. */
static inline uint32_t vivian_read_single_raw_bits(const VivianState* state, uint32_t register_index) {
    return (uint32_t)state->fregs[register_index];
}

/* An unboxed single-precision operand reads as the canonical quiet NaN. */
static inline uint32_t vivian_read_single_bits(const VivianState* state, uint32_t register_index) {
    uint64_t register_bits = state->fregs[register_index];
    return ((register_bits >> 32) == 0xFFFFFFFFu) ? (uint32_t)register_bits : VIVIAN_SINGLE_QUIET_NAN;
}

static inline void vivian_write_single_bits(VivianState* state, uint32_t register_index, uint32_t bits) {
    state->fregs[register_index] = VIVIAN_BOX_SINGLE(bits);
}

static inline float vivian_read_single(const VivianState* state, uint32_t register_index) {
    uint32_t bits = vivian_read_single_bits(state, register_index);
    float value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

/* Arithmetic results canonicalize NaNs; bit-preserving writes use vivian_write_single_bits. */
static inline void vivian_write_single(VivianState* state, uint32_t register_index, float value) {
    uint32_t bits;
    if (value != value) {
        bits = VIVIAN_SINGLE_QUIET_NAN;
    } else {
        memcpy(&bits, &value, sizeof value);
    }
    state->fregs[register_index] = VIVIAN_BOX_SINGLE(bits);
}

static inline uint64_t vivian_read_double_bits(const VivianState* state, uint32_t register_index) {
    return state->fregs[register_index];
}

static inline void vivian_write_double_bits(VivianState* state, uint32_t register_index, uint64_t bits) {
    state->fregs[register_index] = bits;
}

static inline double vivian_read_double(const VivianState* state, uint32_t register_index) {
    double value;
    memcpy(&value, &state->fregs[register_index], sizeof value);
    return value;
}

static inline void vivian_write_double(VivianState* state, uint32_t register_index, double value) {
    if (value != value) {
        state->fregs[register_index] = VIVIAN_DOUBLE_QUIET_NAN;
    } else {
        memcpy(&state->fregs[register_index], &value, sizeof value);
    }
}

#endif
