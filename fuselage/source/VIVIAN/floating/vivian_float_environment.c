/* Calling-thread controls. SEE: (VIVIAN.md, Floating-point environment) */
#include "../vivian.h"
#include <fenv.h>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <xmmintrin.h>
#define VIVIAN_FLOAT_ENVIRONMENT_MASK 0xE040u /* FTZ, rounding control, DAZ */
static unsigned int vivian_read_float_controls(void) {
    return _mm_getcsr();
}
static void vivian_write_float_controls(unsigned int controls) {
    _mm_setcsr(controls);
}
#elif defined(__aarch64__) || defined(_M_ARM64)
#define VIVIAN_FLOAT_ENVIRONMENT_MASK 0x01C80000u /* FZ, rounding control, FZ16 */
static unsigned int vivian_read_float_controls(void) {
    unsigned long long controls;
    __asm__ volatile("mrs %0, fpcr" : "=r"(controls));
    return (unsigned int)controls;
}
static void vivian_write_float_controls(unsigned int controls) {
    unsigned long long control_bits = controls;
    __asm__ volatile("msr fpcr, %0" ::"r"(control_bits));
}
#else
#define VIVIAN_FLOAT_ENVIRONMENT_MASK 0u
static unsigned int vivian_read_float_controls(void) {
    return 0u;
}
static void vivian_write_float_controls(unsigned int controls) {
    (void)controls;
}
#endif

void VIVIAN_EstablishFloatingPointEnvironment(void) {
    fesetround(FE_TONEAREST);
    unsigned int controls = vivian_read_float_controls();
    if (controls & VIVIAN_FLOAT_ENVIRONMENT_MASK) {
        vivian_write_float_controls(controls & ~VIVIAN_FLOAT_ENVIRONMENT_MASK);
    }
}

int VIVIAN_EnsureFloatingPointEnvironment(void) {
    int drifted = 0;
    unsigned int controls = vivian_read_float_controls();
    if (controls & VIVIAN_FLOAT_ENVIRONMENT_MASK) {
        vivian_write_float_controls(controls & ~VIVIAN_FLOAT_ENVIRONMENT_MASK);
        drifted = 1;
    }
    if (fegetround() != FE_TONEAREST) {
        fesetround(FE_TONEAREST);
        drifted = 1;
    }
    return drifted;
}
