#include "misl_math_internal.h"

// Exponential operations. SEE: (MISL_MATH.md, Source organization)
static double misl_math_exponential_core(double binary_exponent, double remainder, double correction) {

    static const double coefficient_2 = 1.0 / 2.0, coefficient_3 = 1.0 / 6.0, coefficient_4 = 1.0 / 24.0;
    static const double coefficient_5 = 1.0 / 120.0, coefficient_6 = 1.0 / 720.0,
                        coefficient_7 = 1.0 / 5040.0;
    static const double coefficient_8 = 1.0 / 40320.0, coefficient_9 = 1.0 / 362880.0,
                        coefficient_10 = 1.0 / 3628800.0;
    static const double coefficient_11 = 1.0 / 39916800.0, coefficient_12 = 1.0 / 479001600.0,
                        coefficient_13 = 1.0 / 6227020800.0;
    double polynomial = coefficient_13;
    polynomial = polynomial * remainder + coefficient_12;
    polynomial = polynomial * remainder + coefficient_11;
    polynomial = polynomial * remainder + coefficient_10;
    polynomial = polynomial * remainder + coefficient_9;
    polynomial = polynomial * remainder + coefficient_8;
    polynomial = polynomial * remainder + coefficient_7;
    polynomial = polynomial * remainder + coefficient_6;
    polynomial = polynomial * remainder + coefficient_5;
    polynomial = polynomial * remainder + coefficient_4;
    polynomial = polynomial * remainder + coefficient_3;
    polynomial = polynomial * remainder + coefficient_2;

    double reduced_minus_one = remainder + remainder * remainder * polynomial;
    double corrected_minus_one = reduced_minus_one + correction * (1.0 + reduced_minus_one);
    double result = 1.0 + corrected_minus_one;

    int64_t exponent_index = (int64_t)binary_exponent;
    if (exponent_index > 1023) {
        exponent_index -= 1023;
        result *= misl_math_from_bits(0x7FE0000000000000ull);
    }
    if (exponent_index < -1022) {
        exponent_index += 1022;
        result *= misl_math_from_bits(0x0010000000000000ull);
    }
    if (exponent_index < -1022) {
        exponent_index += 1022;
        result *= misl_math_from_bits(0x0010000000000000ull);
    }
    return result * misl_math_from_bits((uint64_t)(exponent_index + 1023) << 52);
}

double misl_math_exponential_pair(misl_math_pair value) {
    double head = value.high;
    if (misl_math_is_nan(head)) {
        return MISL_MATH_D_NAN;
    }
    if (head > 7.09782712893383996843e+02) {
        return MISL_MATH_D_INF;
    }
    if (head < -7.45133219101941108420e+02) {
        return 0.0;
    }
    double exponent = head * MISL_MATH_INVLN2;
    double nearest_exponent = (exponent >= 0.0) ? ((exponent + 4503599627370496.0) - 4503599627370496.0)
                                                : ((exponent - 4503599627370496.0) + 4503599627370496.0);
    double high = head - nearest_exponent * MISL_MATH_LN2_HI;
    double low = nearest_exponent * MISL_MATH_LN2_LO;
    double remainder = high - low;
    double correction = ((high - remainder) - low) + value.low;
    return misl_math_exponential_core(nearest_exponent, remainder, correction);
}

MISL_MATH_API double MISL_CalculateExponential(double value) {
    return misl_math_exponential_pair(misl_math_pair_from(value));
}

MISL_MATH_API double MISL_CalculateNaturalLogarithm(double value) {
    if (misl_math_is_nan(value)) {
        return MISL_MATH_D_NAN;
    }
    uint64_t bits = misl_math_bits(value);
    if (bits >> 63) {
        return (bits & 0x7FFFFFFFFFFFFFFFull) ? MISL_MATH_D_NAN : -MISL_MATH_D_INF;
    }
    if (bits == 0) {
        return -MISL_MATH_D_INF;
    }
    if (misl_math_is_infinite(value)) {
        return MISL_MATH_D_INF;
    }
    int64_t binary_exponent = 0;
    if (bits < 0x0010000000000000ull) {
        value *= 18014398509481984.0;
        bits = misl_math_bits(value);
        binary_exponent = -54;
    }
    binary_exponent += (int64_t)(bits >> 52) - 1023;
    bits = (bits & 0x000FFFFFFFFFFFFFull) | 0x3FF0000000000000ull;
    double mantissa = misl_math_from_bits(bits);
    if (mantissa > MISL_MATH_SQRT2) {
        mantissa *= 0.5;
        binary_exponent += 1;
    }
    double fraction = mantissa - 1.0;
    double reduced = fraction / (2.0 + fraction);
    double squared = reduced * reduced;

    double polynomial = 1.0 / 25.0;
    polynomial = polynomial * squared + 1.0 / 23.0;
    polynomial = polynomial * squared + 1.0 / 21.0;
    polynomial = polynomial * squared + 1.0 / 19.0;
    polynomial = polynomial * squared + 1.0 / 17.0;
    polynomial = polynomial * squared + 1.0 / 15.0;
    polynomial = polynomial * squared + 1.0 / 13.0;
    polynomial = polynomial * squared + 1.0 / 11.0;
    polynomial = polynomial * squared + 1.0 / 9.0;
    polynomial = polynomial * squared + 1.0 / 7.0;
    polynomial = polynomial * squared + 1.0 / 5.0;
    polynomial = polynomial * squared + 1.0 / 3.0;
    double mantissa_logarithm = 2.0 * (reduced + reduced * squared * polynomial);
    double exponent_value = (double)binary_exponent;
    return (exponent_value * MISL_MATH_LN2_HI + mantissa_logarithm) + exponent_value * MISL_MATH_LN2_LO;
}

misl_math_pair misl_math_logarithm_pair(double value) {
    uint64_t bits = misl_math_bits(value);
    int64_t binary_exponent = 0;
    if (bits < 0x0010000000000000ull) {
        value *= 18014398509481984.0;
        bits = misl_math_bits(value);
        binary_exponent = -54;
    }
    binary_exponent += (int64_t)(bits >> 52) - 1023;
    bits = (bits & 0x000FFFFFFFFFFFFFull) | 0x3FF0000000000000ull;
    double mantissa = misl_math_from_bits(bits);
    if (mantissa > MISL_MATH_SQRT2) {
        mantissa *= 0.5;
        binary_exponent += 1;
    }
    double fraction = mantissa - 1.0;
    misl_math_pair fraction_pair = misl_math_pair_from(fraction);
    misl_math_pair denominator;
    misl_math_sum_parts(2.0, fraction, &denominator.high, &denominator.low);
    misl_math_pair reduced = misl_math_pair_divide(fraction_pair, denominator);
    misl_math_pair squared = misl_math_pair_multiply(reduced, reduced);
    misl_math_pair polynomial = misl_math_pair_reciprocal(25.0);
    for (double denominator_index = 23.0; denominator_index >= 3.0; denominator_index -= 2.0) {
        polynomial = misl_math_pair_add(misl_math_pair_multiply(polynomial, squared),
                                        misl_math_pair_reciprocal(denominator_index));
    }
    misl_math_pair tail = misl_math_pair_multiply(misl_math_pair_multiply(reduced, squared), polynomial);
    misl_math_pair logarithm = misl_math_pair_add(reduced, tail);
    logarithm = misl_math_pair_multiply(logarithm, misl_math_pair_from(2.0));
    misl_math_pair log_two = {MISL_MATH_LN2_DD_HI, MISL_MATH_LN2_DD_LO};
    misl_math_pair exponent_term =
        misl_math_pair_multiply(misl_math_pair_from((double)binary_exponent), log_two);
    return misl_math_pair_add(exponent_term, logarithm);
}
