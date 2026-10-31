/* a0..a6 inputs and a0[:a1] results. SEE: (MISL_HOST.md, Argument marshalling) */
#ifndef MISL_HOST_ARGUMENTS_INTERNAL_H
#define MISL_HOST_ARGUMENTS_INTERNAL_H
#include "misl_host_state_internal.h"
#include <string.h>

static inline uint32_t fuselage_guest_get_argument(unsigned index) {
    return index < 7 ? fuselage_guest_session.cpu.regs[10 + index] : 0;
}
static inline uint64_t fuselage_guest_get_wide_argument(unsigned index) {
    return index < 6 ? (uint64_t)fuselage_guest_get_argument(index) |
                           ((uint64_t)fuselage_guest_get_argument(index + 1) << 32)
                     : 0;
}
static inline float fuselage_guest_get_float_argument(unsigned index) {
    uint32_t bits = fuselage_guest_get_argument(index);
    float value;
    memcpy(&value, &bits, sizeof value);
    return value;
}
static inline double fuselage_guest_get_double_argument(unsigned index) {
    uint64_t bits = fuselage_guest_get_wide_argument(index);
    double value;
    memcpy(&value, &bits, sizeof value);
    return value;
}
static inline void fuselage_guest_set_result(uint32_t value) {
    fuselage_guest_session.cpu.regs[10] = value;
}
static inline void fuselage_guest_set_wide_result(uint64_t value) {
    fuselage_guest_session.cpu.regs[10] = (uint32_t)value;
    fuselage_guest_session.cpu.regs[11] = (uint32_t)(value >> 32);
}
static inline void fuselage_guest_set_float_result(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    fuselage_guest_set_result(bits);
}
static inline void fuselage_guest_set_double_result(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof bits);
    fuselage_guest_set_wide_result(bits);
}
#endif
