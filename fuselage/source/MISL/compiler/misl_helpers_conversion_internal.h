#pragma once
#include "misl_helpers_words_internal.h"

// SEE: (MISL_HELPERS.md, Floating conversions)
static inline double misl_helpers_unsigned_to_double(unsigned long long value) {
    double high = (double)(unsigned int)(value >> 32);
    double low = (double)(unsigned int)(value & 0xFFFFFFFFu);
    return high * 4294967296.0 + low;
}

static inline double misl_helpers_signed_to_double(long long value) {
    if (value < 0) {
        return -misl_helpers_unsigned_to_double(~(unsigned long long)value + 1ULL);
    }
    return misl_helpers_unsigned_to_double((unsigned long long)value);
}

static inline unsigned long long misl_helpers_double_to_unsigned(double value) {
    if (!(value > 0.0)) {
        return 0;
    }
    if (value >= 18446744073709551616.0) {
        return ~0ULL;
    }
    unsigned int high = (unsigned int)(value / 4294967296.0);
    double remainder = value - (double)high * 4294967296.0;
    if (remainder < 0.0) {
        remainder = 0.0;
    }
    unsigned int low = (unsigned int)remainder;
    return ((unsigned long long)high << 32) | low;
}

static inline long long misl_helpers_double_to_signed(double value) {
    if (value != value) {
        return 0;
    }
    if (value >= 0x1p63) {
        return INT64_MAX;
    }
    if (value <= -0x1p63) {
        return INT64_MIN;
    }
    if (value < 0.0) {
        return -(long long)misl_helpers_double_to_unsigned(-value);
    }
    return (long long)misl_helpers_double_to_unsigned(value);
}

static inline float misl_helpers_unsigned_to_float(unsigned long long value) {
    if (value < (1ULL << 53)) {
        return (float)misl_helpers_unsigned_to_double(value);
    }
    int shift = 0;
    unsigned long long significand = value;
    while (significand >= (1ULL << 53)) {
        significand >>= 1;
        shift++;
    }
    unsigned long long discarded = value & ((1ULL << shift) - 1ULL);
    if (discarded) {
        significand |= 1ULL;
    }
    double converted = misl_helpers_unsigned_to_double(significand);
    double scale = 1.0;
    for (int scale_index = 0; scale_index < shift; scale_index++) {
        scale *= 2.0;
    }
    return (float)(converted * scale);
}

static inline float misl_helpers_signed_to_float(long long value) {
    if (value < 0) {
        return -misl_helpers_unsigned_to_float(~(unsigned long long)value + 1ULL);
    }
    return misl_helpers_unsigned_to_float((unsigned long long)value);
}

static inline unsigned long long misl_helpers_float_to_unsigned(float value) {
    return misl_helpers_double_to_unsigned((double)value);
}

static inline long long misl_helpers_float_to_signed(float value) {
    return misl_helpers_double_to_signed((double)value);
}
