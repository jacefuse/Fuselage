#pragma once
#include "misl_math.h"
#include <stdint.h>
#include <float.h>
#include <string.h>

// Private binary64 and compensated arithmetic. Compile with -ffp-contract=off.
// SEE: (MISL_MATH.md, Build and numerical scope)
#if FLT_EVAL_METHOD != 0
#error "MISL math requires FLT_EVAL_METHOD == 0"
#endif
_Static_assert(sizeof(double) == 8 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024,
               "MISL math requires binary64");
typedef struct {
    double high, low;
} misl_math_pair;
#define MISL_MATH_D_NAN misl_math_from_bits(0x7FF8000000000000ull)
#define MISL_MATH_D_INF misl_math_from_bits(0x7FF0000000000000ull)
#define MISL_MATH_F_NAN_BITS 0x7FC00000u
#define MISL_MATH_PIO2_1 misl_math_from_bits(0x3FF921FB54400000ull)
#define MISL_MATH_PIO2_2 misl_math_from_bits(0x3DD0B4611A600000ull)
#define MISL_MATH_PIO2_3 misl_math_from_bits(0x3BA3198A2E000000ull)
#define MISL_MATH_PIO2_3T misl_math_from_bits(0x397B839A252049C1ull)
#define MISL_MATH_INVPIO2 misl_math_from_bits(0x3FE45F306DC9C883ull)
#define MISL_MATH_PI_HI misl_math_from_bits(0x400921FB54442D18ull)
#define MISL_MATH_PI_LO misl_math_from_bits(0x3CA1A62633145C07ull)
#define MISL_MATH_PIO2_HI misl_math_from_bits(0x3FF921FB54442D18ull)
#define MISL_MATH_PIO2_LO misl_math_from_bits(0x3C91A62633145C07ull)
#define MISL_MATH_PIO4_HI misl_math_from_bits(0x3FE921FB54442D18ull)
#define MISL_MATH_PIO4_LO misl_math_from_bits(0x3C81A62633145C07ull)
#define MISL_MATH_LN2_HI misl_math_from_bits(0x3FE62E42FEE00000ull)
#define MISL_MATH_LN2_LO misl_math_from_bits(0x3DEA39EF35793C76ull)
#define MISL_MATH_INVLN2 misl_math_from_bits(0x3FF71547652B82FEull)
#define MISL_MATH_LN2 misl_math_from_bits(0x3FE62E42FEFA39EFull)
#define MISL_MATH_SQRT2 misl_math_from_bits(0x3FF6A09E667F3BCDull)
#define MISL_MATH_SQRTH misl_math_from_bits(0x3FE6A09E667F3BCDull)
#define MISL_MATH_LN2_DD_HI misl_math_from_bits(0x3FE62E42FEFA39EFull)
#define MISL_MATH_LN2_DD_LO misl_math_from_bits(0x3C7ABC9E3B39803Full)

static inline double misl_math_from_bits(uint64_t bits) {
    double value;
    memcpy(&value, &bits, 8);
    return value;
}

static inline uint64_t misl_math_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, 8);
    return bits;
}

static inline int misl_math_is_nan(double value) {
    return value != value;
}

static inline int misl_math_is_infinite(double value) {
    return (misl_math_bits(value) & 0x7FFFFFFFFFFFFFFFull) == 0x7FF0000000000000ull;
}

static inline void misl_math_sum_parts(double first, double second, double* sum, double* error) {
    *sum = first + second;
    double rounded_second = *sum - first;
    *error = (first - (*sum - rounded_second)) + (second - rounded_second);
}

static inline void misl_math_product_parts(double first, double second, double* product, double* error) {
    const double splitter = 134217729.0;
    *product = first * second;
    double first_split = splitter * first, first_high = first_split - (first_split - first),
           first_low = first - first_high;
    double second_split = splitter * second, second_high = second_split - (second_split - second),
           second_low = second - second_high;
    *error = ((first_high * second_high - *product) + first_high * second_low + first_low * second_high) +
             first_low * second_low;
}

static inline misl_math_pair misl_math_pair_from(double value) {
    misl_math_pair result = {value, 0.0};
    return result;
}

static inline misl_math_pair misl_math_pair_add(misl_math_pair first, misl_math_pair second) {
    double sum, error;
    misl_math_sum_parts(first.high, second.high, &sum, &error);
    error += first.low + second.low;
    misl_math_pair result;
    misl_math_sum_parts(sum, error, &result.high, &result.low);
    return result;
}

static inline misl_math_pair misl_math_pair_multiply(misl_math_pair first, misl_math_pair second) {
    double product, error;
    misl_math_product_parts(first.high, second.high, &product, &error);
    error += first.high * second.low + first.low * second.high;
    misl_math_pair result;
    misl_math_sum_parts(product, error, &result.high, &result.low);
    return result;
}

static inline misl_math_pair misl_math_pair_negate(misl_math_pair value) {
    misl_math_pair result = {-value.high, -value.low};
    return result;
}

static inline misl_math_pair misl_math_pair_divide(misl_math_pair numerator, misl_math_pair denominator) {
    double first_quotient = numerator.high / denominator.high;
    misl_math_pair remainder = misl_math_pair_add(
        numerator,
        misl_math_pair_negate(misl_math_pair_multiply(denominator, misl_math_pair_from(first_quotient))));
    double second_quotient = remainder.high / denominator.high;
    remainder = misl_math_pair_add(remainder, misl_math_pair_negate(misl_math_pair_multiply(
                                                  denominator, misl_math_pair_from(second_quotient))));
    double third_quotient = remainder.high / denominator.high;
    misl_math_pair quotient;
    misl_math_sum_parts(first_quotient, second_quotient, &quotient.high, &quotient.low);
    quotient.low += third_quotient;
    misl_math_pair result;
    misl_math_sum_parts(quotient.high, quotient.low, &result.high, &result.low);
    return result;
}

static inline misl_math_pair misl_math_pair_reciprocal(double denominator) {
    double high = 1.0 / denominator, product, error;
    misl_math_product_parts(high, denominator, &product, &error);
    double remainder = (1.0 - product) - error;
    misl_math_pair result = {high, remainder / denominator};
    return result;
}
misl_math_pair misl_math_logarithm_pair(double value);
double misl_math_exponential_pair(misl_math_pair value);
