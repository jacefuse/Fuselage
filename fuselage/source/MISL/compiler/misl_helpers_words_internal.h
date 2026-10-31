#pragma once
#include <stdint.h>

// Compiler-support implementation, not an application API.
// SEE: (MISL_HELPERS.md, Integer representation)
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "MISL word helpers require little-endian storage"
#endif
typedef union {
    uint64_t value;
    struct {
        uint32_t low, high;
    } words;
} misl_helpers_double_word;
static inline long long misl_helpers_shift_left(long long value, int shift) {
    misl_helpers_double_word words;
    words.value = (uint64_t)value;
    if (shift <= 0) {
        return value;
    }
    if (shift >= 64) {
        return 0;
    }
    if (shift >= 32) {
        words.words.high = words.words.low << (shift - 32);
        words.words.low = 0;
    } else {
        words.words.high = (words.words.high << shift) | (words.words.low >> (32 - shift));
        words.words.low <<= shift;
    }
    return (long long)words.value;
}

static inline unsigned long long misl_helpers_shift_right(unsigned long long value, int shift) {
    misl_helpers_double_word words;
    words.value = value;
    if (shift <= 0) {
        return value;
    }
    if (shift >= 64) {
        return 0;
    }
    if (shift >= 32) {
        words.words.low = words.words.high >> (shift - 32);
        words.words.high = 0;
    } else {
        words.words.low = (words.words.low >> shift) | (words.words.high << (32 - shift));
        words.words.high >>= shift;
    }
    return words.value;
}

static inline long long misl_helpers_shift_right_signed(long long value, int shift) {
    misl_helpers_double_word words;
    words.value = (uint64_t)value;
    int32_t high = (int32_t)words.words.high;
    if (shift <= 0) {
        return value;
    }
    if (shift >= 64) {
        return (high < 0) ? -1 : 0;
    }
    if (shift >= 32) {
        words.words.high = (uint32_t)(high >> 31);
        words.words.low = (uint32_t)(high >> (shift - 32));
    } else {
        words.words.low = (words.words.low >> shift) | (words.words.high << (32 - shift));
        words.words.high = (uint32_t)(high >> shift);
    }
    return (long long)words.value;
}
