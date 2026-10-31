/* Internal execution helpers. SEE: (VIVIAN.md, Floating-point helpers) */
#ifndef VIVIAN_FLOAT_INTERNAL_H
#define VIVIAN_FLOAT_INTERNAL_H
#include "../vivian.h"
#include <fenv.h>
#include <math.h>

static inline void vivian_clear_host_float_flags(void) {
    feclearexcept(FE_ALL_EXCEPT);
}

/* Merge host exception flags into sticky guest flags without changing rounding control. */
static inline void vivian_accrue_float_flags(VivianState* state) {
    int host_flags = fetestexcept(FE_ALL_EXCEPT);
    uint32_t guest_flags = 0;
    if (host_flags & FE_INEXACT) {
        guest_flags |= 0x01;
    }
    if (host_flags & FE_UNDERFLOW) {
        guest_flags |= 0x02;
    }
    if (host_flags & FE_OVERFLOW) {
        guest_flags |= 0x04;
    }
    if (host_flags & FE_DIVBYZERO) {
        guest_flags |= 0x08;
    }
    if (host_flags & FE_INVALID) {
        guest_flags |= 0x10;
    }
    state->fcsr |= guest_flags;
}

static inline int vivian_is_signaling_nan_single(uint32_t bits) {
    return (bits & 0x7F800000u) == 0x7F800000u && (bits & 0x007FFFFFu) != 0 && (bits & 0x00400000u) == 0;
}

static inline int vivian_is_signaling_nan_double(uint64_t bits) {
    return (bits & 0x7FF0000000000000ULL) == 0x7FF0000000000000ULL && (bits & 0x000FFFFFFFFFFFFFULL) != 0 &&
           (bits & 0x0008000000000000ULL) == 0;
}

static inline int vivian_is_nearest_rounding(uint32_t rounding_mode) {
    return rounding_mode == 0u || rounding_mode == 7u;
}

/* Round before saturation. SEE: (VIVIAN.md, Floating-point helpers) */
static inline uint32_t vivian_convert_float_to_word(double value, int is_unsigned, uint32_t rounding_mode,
                                                   int* invalid_rounding_mode) {
    if (value != value) {
        feraiseexcept(FE_INVALID);
        return is_unsigned ? 0xFFFFFFFFu : 0x7FFFFFFFu;
    }
    double rounded_value;
    switch (rounding_mode) {
    case 0:
    case 7:
        rounded_value = rint(value);
        break;
    case 1:
        rounded_value = trunc(value);
        break;
    case 2:
        rounded_value = floor(value);
        break;
    case 3:
        rounded_value = ceil(value);
        break;
    case 4:
        rounded_value = round(value);
        break;
    default:
        *invalid_rounding_mode = 1;
        return 0u;
    }
    if (is_unsigned) {
        if (rounded_value < 0.0) {
            feraiseexcept(FE_INVALID);
            return 0u;
        }
        if (rounded_value > 4294967295.0) {
            feraiseexcept(FE_INVALID);
            return 0xFFFFFFFFu;
        }
        if (rounded_value != value) {
            feraiseexcept(FE_INEXACT);
        }
        return (uint32_t)rounded_value;
    }
    if (rounded_value < -2147483648.0) {
        feraiseexcept(FE_INVALID);
        return 0x80000000u;
    }
    if (rounded_value > 2147483647.0) {
        feraiseexcept(FE_INVALID);
        return 0x7FFFFFFFu;
    }
    if (rounded_value != value) {
        feraiseexcept(FE_INEXACT);
    }
    return (uint32_t)(int32_t)rounded_value;
}

static inline uint32_t vivian_classify_single(uint32_t bits) {
    uint32_t sign = bits >> 31, exponent = (bits >> 23) & 0xFF, fraction = bits & 0x7FFFFFu;
    if (exponent == 0xFF) {
        if (fraction == 0) {
            return sign ? (1u << 0) : (1u << 7);
        }
        return (fraction & 0x400000u) ? (1u << 9) : (1u << 8);
    }
    if (exponent == 0) {
        if (fraction == 0) {
            return sign ? (1u << 3) : (1u << 4);
        }
        return sign ? (1u << 2) : (1u << 5);
    }
    return sign ? (1u << 1) : (1u << 6);
}

static inline uint32_t vivian_classify_double(uint64_t bits) {
    uint32_t sign = (uint32_t)(bits >> 63), exponent = (uint32_t)((bits >> 52) & 0x7FFu);
    uint64_t fraction = bits & 0xFFFFFFFFFFFFFULL;
    if (exponent == 0x7FF) {
        if (fraction == 0) {
            return sign ? (1u << 0) : (1u << 7);
        }
        return (fraction & 0x8000000000000ULL) ? (1u << 9) : (1u << 8);
    }
    if (exponent == 0) {
        if (fraction == 0) {
            return sign ? (1u << 3) : (1u << 4);
        }
        return sign ? (1u << 2) : (1u << 5);
    }
    return sign ? (1u << 1) : (1u << 6);
}

#endif
