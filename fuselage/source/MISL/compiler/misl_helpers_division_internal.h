#pragma once
#include "misl_helpers_words_internal.h"

// SEE: (MISL_HELPERS.md, Division and remainder)
static inline uint64_t misl_helpers_divide_with_remainder(uint64_t numerator, uint64_t denominator,
                                                          uint64_t* remainder) {
    if (denominator == 0) {
        if (remainder) {
            *remainder = 0;
        }
        return 0;
    }
    uint64_t quotient = 0, residue = 0;
    for (int bit_index = 63; bit_index >= 0; bit_index--) {
        residue = (residue << 1) | ((numerator >> bit_index) & 1u);
        if (residue >= denominator) {
            residue -= denominator;
            quotient |= (uint64_t)1 << bit_index;
        }
    }
    if (remainder) {
        *remainder = residue;
    }
    return quotient;
}

static inline unsigned long long misl_helpers_divide_unsigned(unsigned long long numerator,
                                                              unsigned long long denominator) {
    return misl_helpers_divide_with_remainder(numerator, denominator, 0);
}

static inline unsigned long long misl_helpers_remainder_unsigned(unsigned long long numerator,
                                                                 unsigned long long denominator) {
    uint64_t residue;
    misl_helpers_divide_with_remainder(numerator, denominator, &residue);
    return residue;
}

static inline long long misl_helpers_divide_signed(long long numerator, long long denominator) {
    int negative = 0;
    uint64_t numerator_magnitude = (numerator < 0) ? (~(uint64_t)numerator + 1u) : (uint64_t)numerator;
    uint64_t denominator_magnitude =
        (denominator < 0) ? (~(uint64_t)denominator + 1u) : (uint64_t)denominator;
    uint64_t quotient;
    if (numerator < 0) {
        negative = !negative;
    }
    if (denominator < 0) {
        negative = !negative;
    }
    quotient = misl_helpers_divide_with_remainder(numerator_magnitude, denominator_magnitude, 0);
    return (long long)(negative ? (~quotient + 1u) : quotient);
}

static inline long long misl_helpers_remainder_signed(long long numerator, long long denominator) {
    int negative = (numerator < 0);
    uint64_t numerator_magnitude = (numerator < 0) ? (~(uint64_t)numerator + 1u) : (uint64_t)numerator;
    uint64_t denominator_magnitude =
        (denominator < 0) ? (~(uint64_t)denominator + 1u) : (uint64_t)denominator;
    uint64_t residue;
    misl_helpers_divide_with_remainder(numerator_magnitude, denominator_magnitude, &residue);
    return (long long)(negative ? (~residue + 1u) : residue);
}
