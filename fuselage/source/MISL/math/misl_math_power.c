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

// Compensated products split each operand with a 2^27+1 splitter and subtract
// the raw product, so an operand above 2^997 or a product above DBL_MAX turns
// into inf-inf and NaN. Pairs are therefore kept within [2^-256, 2^256) between
// operations and the excess is carried as a binary exponent. Scaling by a power
// of two is exact while every part stays normal, so in-range results are
// unchanged. SEE: (MISL_MATH.md, Build and numerical scope)
#define MISL_MATH_POWER_BAND_HIGH misl_math_from_bits(0x4FF0000000000000ull) /* 2^256 */
#define MISL_MATH_POWER_BAND_LOW misl_math_from_bits(0x2FF0000000000000ull)  /* 2^-256 */

static void misl_math_rebalance_pair(misl_math_pair* value, int64_t* binary_exponent) {
    double magnitude = (value->high < 0.0) ? -value->high : value->high;
    while (magnitude >= MISL_MATH_POWER_BAND_HIGH) {
        value->high *= MISL_MATH_POWER_BAND_LOW;
        value->low *= MISL_MATH_POWER_BAND_LOW;
        magnitude *= MISL_MATH_POWER_BAND_LOW;
        *binary_exponent += 256;
    }
    while (magnitude < MISL_MATH_POWER_BAND_LOW) {
        value->high *= MISL_MATH_POWER_BAND_HIGH;
        value->low *= MISL_MATH_POWER_BAND_HIGH;
        magnitude *= MISL_MATH_POWER_BAND_HIGH;
        *binary_exponent -= 256;
    }
}

// value * 2^binary_exponent for a finite nonzero normal value, with one rounding
// for a subnormal result and infinity or zero beyond the representable range.
static double misl_math_scale_by_power_of_two(double value, int64_t binary_exponent) {
    if (binary_exponent == 0) {
        return value;
    }
    const uint64_t bits = misl_math_bits(value);
    const uint64_t sign = bits & 0x8000000000000000ull;
    const uint64_t mantissa = bits & 0x000FFFFFFFFFFFFFull;
    const int64_t exponent = (int64_t)((bits >> 52) & 0x7FF) - 1023 + binary_exponent;
    if (exponent > 1023) {
        return misl_math_from_bits(sign | 0x7FF0000000000000ull);
    }
    if (exponent >= -1022) {
        return misl_math_from_bits(sign | ((uint64_t)(exponent + 1023) << 52) | mantissa);
    }
    if (exponent < -1075) {
        return misl_math_from_bits(sign);
    }
    // A unit-exponent copy scaled to the smallest normal exactly, then once more
    // into the subnormal range so the rounding happens in one multiplication.
    const double smallest_normal = misl_math_from_bits(sign | 0x0010000000000000ull | mantissa);
    return smallest_normal * misl_math_from_bits((uint64_t)(exponent + 1022 + 1023) << 52);
}

static double misl_math_integer_power(double base_magnitude, double exponent) {
    double exponent_magnitude = (exponent < 0.0) ? -exponent : exponent;
    int64_t power = (int64_t)exponent_magnitude;
    int64_t result_exponent = 0, base_exponent = 0;
    misl_math_pair result = misl_math_pair_from(1.0), base = misl_math_pair_from(base_magnitude);
    misl_math_rebalance_pair(&base, &base_exponent);
    while (power) {
        if (power & 1) {
            result = misl_math_pair_multiply(result, base);
            result_exponent += base_exponent;
            misl_math_rebalance_pair(&result, &result_exponent);
        }
        power >>= 1;
        if (power) {
            base = misl_math_pair_multiply(base, base);
            base_exponent *= 2;
            misl_math_rebalance_pair(&base, &base_exponent);
        }
    }
    if (exponent < 0.0) {
        result = misl_math_pair_divide(misl_math_pair_from(1.0), result);
        result_exponent = -result_exponent;
    }
    return misl_math_scale_by_power_of_two(result.high + result.low, result_exponent);
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
        // The exponential saturates beyond about +710 and -746. Deciding there
        // from the plain product keeps the compensated multiply below, whose
        // operand split overflows for exponents near DBL_MAX, within range.
        const double estimate = logarithm.high * exponent;
        if (estimate > 800.0) {
            result = MISL_MATH_D_INF;
        } else if (estimate < -800.0) {
            result = 0.0;
        } else {
            misl_math_pair exponent_pair = misl_math_pair_multiply(logarithm, misl_math_pair_from(exponent));
            result = misl_math_exponential_pair(exponent_pair);
        }
    }
    return (base_negative && misl_math_is_odd_integer(exponent)) ? -result : result;
}
