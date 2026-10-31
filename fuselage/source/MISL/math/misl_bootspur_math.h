#pragma once

// Narrow guest math interface. SEE: (BOOTSPUR.md, Math and floating ABIs)
#ifdef __cplusplus
extern "C" {
#endif

// The result must be representable; the most-negative signed input is unsupported.
static inline int abs(int value) {
    return __builtin_abs(value);
}
static inline long labs(long value) {
    return __builtin_labs(value);
}
static inline long long llabs(long long value) {
    return __builtin_llabs(value);
}

#if defined(__riscv_float_abi_single) || defined(__riscv_float_abi_double)
float sinf(float value);
float cosf(float value);
float tanf(float value);
float atan2f(float y, float x);
float expf(float value);
float logf(float value);
float powf(float base, float exponent);
float floorf(float value);
float ceilf(float value);
float truncf(float value);
float roundf(float value);

// Use the ISA directly: builtin lowering can otherwise require an unavailable errno path.
static inline float sqrtf(float value) {
    float result;
    __asm__ volatile("fsqrt.s %0, %1" : "=f"(result) : "f"(value));
    return result;
}
static inline float fabsf(float value) {
    return __builtin_fabsf(value);
}
static inline float fmaf(float first, float second, float addend) {
    float result;
    __asm__ volatile("fmadd.s %0, %1, %2, %3" : "=f"(result) : "f"(first), "f"(second), "f"(addend));
    return result;
}
#endif

#if defined(__riscv_float_abi_double)
double sin(double value);
double cos(double value);
double tan(double value);
double atan2(double y, double x);
double exp(double value);
double log(double value);
double pow(double base, double exponent);
double floor(double value);
double ceil(double value);
double trunc(double value);
double round(double value);

static inline double sqrt(double value) {
    double result;
    __asm__ volatile("fsqrt.d %0, %1" : "=f"(result) : "f"(value));
    return result;
}
static inline double fabs(double value) {
    return __builtin_fabs(value);
}
static inline double fma(double first, double second, double addend) {
    double result;
    __asm__ volatile("fmadd.d %0, %1, %2, %3" : "=f"(result) : "f"(first), "f"(second), "f"(addend));
    return result;
}
#endif

#ifdef __cplusplus
}
#endif
