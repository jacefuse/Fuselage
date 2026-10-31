#include "misl_math_internal.h"

// Power operations. SEE: (MISL_MATH.md, Source organization)
static int misl_math_is_integer(double value) {
    return MISL_Truncate(value) == value;
}

static int misl_math_is_odd_integer(double value) {
    if (!misl_math_is_integer(value)) {
        return 0;
    }
    double magnitude = (value < 0.0) ? -value : value;
    if (magnitude >= 9007199254740992.0) {
        return 0;
    }
    return (MISL_Truncate(magnitude * 0.5) * 2.0) != magnitude;
}

static double misl_math_integer_power(double base_magnitude, double exponent) {
    double exponent_magnitude = (exponent < 0.0) ? -exponent : exponent;
    int64_t power = (int64_t)exponent_magnitude;
    misl_math_pair result = misl_math_pair_from(1.0), base = misl_math_pair_from(base_magnitude);
    while (power) {
        if (power & 1) {
            result = misl_math_pair_multiply(result, base);
        }
        power >>= 1;
        if (power) {
            base = misl_math_pair_multiply(base, base);
        }
    }
    if (exponent < 0.0) {
        result = misl_math_pair_divide(misl_math_pair_from(1.0), result);
    }
    return result.high + result.low;
}

MISL_MATH_API double MISL_CalculatePower(double base, double exponent) {
    if (exponent == 0.0) {
        return 1.0;
    }
    if (base == 1.0) {
        return 1.0;
    }
    if (misl_math_is_nan(base) || misl_math_is_nan(exponent)) {
        return MISL_MATH_D_NAN;
    }
    const int base_negative = (misl_math_bits(base) >> 63) != 0;
    double base_magnitude = base_negative ? -base : base;
    if (misl_math_is_infinite(exponent)) {
        if (base_magnitude == 1.0) {
            return 1.0;
        }
        return ((base_magnitude > 1.0) == (exponent > 0.0)) ? MISL_MATH_D_INF : 0.0;
    }
    if (base_magnitude == 0.0) {
        if (exponent < 0.0) {
            return (base_negative && misl_math_is_odd_integer(exponent)) ? -MISL_MATH_D_INF : MISL_MATH_D_INF;
        }
        return (base_negative && misl_math_is_odd_integer(exponent)) ? base : 0.0;
    }
    if (misl_math_is_infinite(base)) {
        if (exponent < 0.0) {
            return (base_negative && misl_math_is_odd_integer(exponent)) ? -0.0 : 0.0;
        }
        return (base_negative && misl_math_is_odd_integer(exponent)) ? -MISL_MATH_D_INF : MISL_MATH_D_INF;
    }
    if (base_negative && !misl_math_is_integer(exponent)) {
        return MISL_MATH_D_NAN;
    }
    double result;
    double exponent_magnitude = (exponent < 0.0) ? -exponent : exponent;
    if (misl_math_is_integer(exponent) && exponent_magnitude <= 1024.0) {
        result = misl_math_integer_power(base_magnitude, exponent);
    } else {
        misl_math_pair logarithm = misl_math_logarithm_pair(base_magnitude);
        misl_math_pair exponent_pair = misl_math_pair_multiply(logarithm, misl_math_pair_from(exponent));
        result = misl_math_exponential_pair(exponent_pair);
    }
    return (base_negative && misl_math_is_odd_integer(exponent)) ? -result : result;
}
