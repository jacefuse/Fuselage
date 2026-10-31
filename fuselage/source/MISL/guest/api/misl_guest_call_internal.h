#pragma once
#include "misl_guest_types.h"
#include "../../misl_abi.h"

// Private ecall register bindings and bit representations.
// SEE: (MISL_GUEST.md, Calls and bit representations)

#define MISL_GUEST_CALL(n, a, b, c, d, e, f, g)                                                              \
    misl_guest_call((int32_t)(n), (int32_t)(a), (int32_t)(b), (int32_t)(c), (int32_t)(d), (int32_t)(e),      \
                    (int32_t)(f), (int32_t)(g))
#define MISL_GUEST_CALL64(n, a, b, c, d, e, f, g)                                                            \
    misl_guest_call64((int32_t)(n), (int32_t)(a), (int32_t)(b), (int32_t)(c), (int32_t)(d), (int32_t)(e),    \
                      (int32_t)(f), (int32_t)(g))
#define MISL_GUEST_LOW(x) ((int32_t)(uint32_t)(uint64_t)(x))
#define MISL_GUEST_HIGH(x) ((int32_t)(uint32_t)((uint64_t)(x) >> 32))

static inline int32_t misl_guest_call(int32_t call_number, int32_t a0, int32_t a1, int32_t a2, int32_t a3,
                                      int32_t a4, int32_t a5, int32_t a6) {
    register int32_t register_0 __asm__("a0") = a0;
    register int32_t register_1 __asm__("a1") = a1;
    register int32_t register_2 __asm__("a2") = a2;
    register int32_t register_3 __asm__("a3") = a3;
    register int32_t register_4 __asm__("a4") = a4;
    register int32_t register_5 __asm__("a5") = a5;
    register int32_t register_6 __asm__("a6") = a6;
    register int32_t register_7 __asm__("a7") = call_number;
    __asm__ volatile("ecall"
                     : "+r"(register_0)
                     : "r"(register_1), "r"(register_2), "r"(register_3), "r"(register_4), "r"(register_5),
                       "r"(register_6), "r"(register_7)
                     : "memory");
    return register_0;
}

static inline uint64_t misl_guest_call64(int32_t call_number, int32_t a0, int32_t a1, int32_t a2, int32_t a3,
                                         int32_t a4, int32_t a5, int32_t a6) {
    register int32_t register_0 __asm__("a0") = a0;
    register int32_t register_1 __asm__("a1") = a1;
    register int32_t register_2 __asm__("a2") = a2;
    register int32_t register_3 __asm__("a3") = a3;
    register int32_t register_4 __asm__("a4") = a4;
    register int32_t register_5 __asm__("a5") = a5;
    register int32_t register_6 __asm__("a6") = a6;
    register int32_t register_7 __asm__("a7") = call_number;
    __asm__ volatile("ecall"
                     : "+r"(register_0), "+r"(register_1)
                     : "r"(register_2), "r"(register_3), "r"(register_4), "r"(register_5), "r"(register_6),
                       "r"(register_7)
                     : "memory");
    return ((uint64_t)(uint32_t)register_1 << 32) | (uint32_t)register_0;
}

static inline int32_t misl_guest_float_bits(float value) {
    union {
        float value;
        int32_t signed_word;
    } representation;
    representation.value = value;
    return representation.signed_word;
}

static inline float misl_guest_bits_float(int32_t bits) {
    union {
        float float_value;
        int32_t bits;
    } representation;
    representation.bits = bits;
    return representation.float_value;
}

static inline int32_t misl_guest_double_low(double value) {
    union {
        double value;
        uint64_t unsigned_bits;
    } representation;
    representation.value = value;
    return (int32_t)(uint32_t)representation.unsigned_bits;
}

static inline int32_t misl_guest_double_high(double value) {
    union {
        double value;
        uint64_t unsigned_bits;
    } representation;
    representation.value = value;
    return (int32_t)(uint32_t)(representation.unsigned_bits >> 32);
}

static inline double misl_guest_bits_double(uint64_t bits) {
    union {
        double double_value;
        uint64_t bits;
    } representation;
    representation.bits = bits;
    return representation.double_value;
}

static inline int32_t misl_guest_pack_color(Color color) {
    return (int32_t)((uint32_t)color.r | ((uint32_t)color.g << 8) | ((uint32_t)color.b << 16) |
                     ((uint32_t)color.a << 24));
}

static inline Color misl_guest_unpack_color(int32_t packed_color) {
    Color color = {(unsigned char)packed_color, (unsigned char)(packed_color >> 8),
                   (unsigned char)(packed_color >> 16), (unsigned char)(packed_color >> 24)};
    return color;
}

static inline int32_t misl_guest_address(const void* pointer) {
    return (int32_t)(intptr_t)pointer;
}
