#include "misl_math_internal.h"

// Trig operations. SEE: (MISL_MATH.md, Source organization)
static int misl_math_reduce_half_pi(double value, double* high, double* low) {
    double quadrants = value * MISL_MATH_INVPIO2;

    double nearest_quadrant = (quadrants >= 0.0) ? ((quadrants + 4503599627370496.0) - 4503599627370496.0)
                                                 : ((quadrants - 4503599627370496.0) + 4503599627370496.0);
    if (!(nearest_quadrant >= -0x1p63 && nearest_quadrant < 0x1p63)) {
        return -1;
    }
    double remainder = value - nearest_quadrant * MISL_MATH_PIO2_1;
    double correction = nearest_quadrant * MISL_MATH_PIO2_2;
    double previous = remainder;
    remainder = previous - correction;
    double tail = (previous - remainder) - correction;
    correction = nearest_quadrant * MISL_MATH_PIO2_3;
    previous = remainder;
    remainder = previous - correction;
    tail += (previous - remainder) - correction;
    tail -= nearest_quadrant * MISL_MATH_PIO2_3T;
    *high = remainder + tail;
    *low = tail - (*high - remainder);
    int64_t quadrant_index = (int64_t)nearest_quadrant;
    return (int)(quadrant_index & 3);
}

static double misl_math_sine_kernel(double remainder, double tail) {
    static const double coefficient_1 = -1.0 / 6.0;
    static const double coefficient_2 = 1.0 / 120.0;
    static const double coefficient_3 = -1.0 / 5040.0;
    static const double coefficient_4 = 1.0 / 362880.0;
    static const double coefficient_5 = -1.0 / 39916800.0;
    static const double coefficient_6 = 1.0 / 6227020800.0;
    static const double coefficient_7 = -1.0 / 1307674368000.0;
    static const double coefficient_8 = 1.0 / 355687428096000.0;
    static const double coefficient_9 = -1.0 / 355687428096000.0 / 342.0;
    double squared = remainder * remainder;
    double polynomial = coefficient_9;
    polynomial = polynomial * squared + coefficient_8;
    polynomial = polynomial * squared + coefficient_7;
    polynomial = polynomial * squared + coefficient_6;
    polynomial = polynomial * squared + coefficient_5;
    polynomial = polynomial * squared + coefficient_4;
    polynomial = polynomial * squared + coefficient_3;
    polynomial = polynomial * squared + coefficient_2;
    polynomial = polynomial * squared + coefficient_1;

    return remainder + (remainder * squared * polynomial + tail * (1.0 - 0.5 * squared));
}

static double misl_math_cosine_kernel(double remainder, double tail) {
    static const double coefficient_1 = -1.0 / 2.0;
    static const double coefficient_2 = 1.0 / 24.0;
    static const double coefficient_3 = -1.0 / 720.0;
    static const double coefficient_4 = 1.0 / 40320.0;
    static const double coefficient_5 = -1.0 / 3628800.0;
    static const double coefficient_6 = 1.0 / 479001600.0;
    static const double coefficient_7 = -1.0 / 87178291200.0;
    static const double coefficient_8 = 1.0 / 20922789888000.0;
    static const double coefficient_9 = -1.0 / 6402373705728000.0;
    static const double coefficient_10 = 1.0 / 6402373705728000.0 / 380.0;
    double squared = remainder * remainder;
    double polynomial = coefficient_10;
    polynomial = polynomial * squared + coefficient_9;
    polynomial = polynomial * squared + coefficient_8;
    polynomial = polynomial * squared + coefficient_7;
    polynomial = polynomial * squared + coefficient_6;
    polynomial = polynomial * squared + coefficient_5;
    polynomial = polynomial * squared + coefficient_4;
    polynomial = polynomial * squared + coefficient_3;
    polynomial = polynomial * squared + coefficient_2;

    double half_squared = 0.5 * squared;
    double leading = 1.0 - half_squared;
    (void)coefficient_1;
    return leading + (((1.0 - leading) - half_squared) + (squared * squared * polynomial - remainder * tail));
}

MISL_MATH_API double MISL_CalculateSine(double value) {
    if (misl_math_is_nan(value) || misl_math_is_infinite(value)) {
        return MISL_MATH_D_NAN;
    }
    double magnitude = (value < 0.0) ? -value : value;
    if (magnitude < 7.8539816339744828e-01) {
        if (magnitude < 7.450580596923828e-09) {
            return value;
        }
        return misl_math_sine_kernel(value, 0.0);
    }
    double remainder, tail;
    int quadrant = misl_math_reduce_half_pi(value, &remainder, &tail);
    if (quadrant < 0) {
        return MISL_MATH_D_NAN;
    }
    switch (quadrant) {
    case 0:
        return misl_math_sine_kernel(remainder, tail);
    case 1:
        return misl_math_cosine_kernel(remainder, tail);
    case 2:
        return -misl_math_sine_kernel(remainder, tail);
    default:
        return -misl_math_cosine_kernel(remainder, tail);
    }
}

MISL_MATH_API double MISL_CalculateCosine(double value) {
    if (misl_math_is_nan(value) || misl_math_is_infinite(value)) {
        return MISL_MATH_D_NAN;
    }
    double magnitude = (value < 0.0) ? -value : value;
    if (magnitude < 7.8539816339744828e-01) {
        if (magnitude < 7.450580596923828e-09) {
            return 1.0;
        }
        return misl_math_cosine_kernel(value, 0.0);
    }
    double remainder, tail;
    int quadrant = misl_math_reduce_half_pi(value, &remainder, &tail);
    if (quadrant < 0) {
        return MISL_MATH_D_NAN;
    }
    switch (quadrant) {
    case 0:
        return misl_math_cosine_kernel(remainder, tail);
    case 1:
        return -misl_math_sine_kernel(remainder, tail);
    case 2:
        return -misl_math_cosine_kernel(remainder, tail);
    default:
        return misl_math_sine_kernel(remainder, tail);
    }
}

MISL_MATH_API double MISL_CalculateTangent(double value) {
    if (misl_math_is_nan(value) || misl_math_is_infinite(value)) {
        return MISL_MATH_D_NAN;
    }
    double remainder, tail;
    int quadrant;
    double magnitude = (value < 0.0) ? -value : value;
    if (magnitude < 7.8539816339744828e-01) {
        remainder = value;
        tail = 0.0;
        quadrant = 0;
        if (magnitude < 7.450580596923828e-09) {
            return value;
        }
    } else {
        quadrant = misl_math_reduce_half_pi(value, &remainder, &tail);
    }
    if (quadrant < 0) {
        return MISL_MATH_D_NAN;
    }
    double sine = misl_math_sine_kernel(remainder, tail), cosine = misl_math_cosine_kernel(remainder, tail);
    return (quadrant & 1) ? -(cosine / sine) : (sine / cosine);
}

static double misl_math_atan_positive(double value) {
    int reciprocal = 0;
    if (value > 1.0) {
        value = 1.0 / value;
        reciprocal = 1;
    }
    double first_reduction = value / (1.0 + __builtin_sqrt(1.0 + value * value));
    double second_reduction =
        first_reduction / (1.0 + __builtin_sqrt(1.0 + first_reduction * first_reduction));

    double squared = second_reduction * second_reduction;
    double polynomial = -1.0 / 27.0;
    polynomial = polynomial * squared + 1.0 / 25.0;
    polynomial = polynomial * squared - 1.0 / 23.0;
    polynomial = polynomial * squared + 1.0 / 21.0;
    polynomial = polynomial * squared - 1.0 / 19.0;
    polynomial = polynomial * squared + 1.0 / 17.0;
    polynomial = polynomial * squared - 1.0 / 15.0;
    polynomial = polynomial * squared + 1.0 / 13.0;
    polynomial = polynomial * squared - 1.0 / 11.0;
    polynomial = polynomial * squared + 1.0 / 9.0;
    polynomial = polynomial * squared - 1.0 / 7.0;
    polynomial = polynomial * squared + 1.0 / 5.0;
    polynomial = polynomial * squared - 1.0 / 3.0;
    double angle = second_reduction + second_reduction * squared * polynomial;
    angle = 4.0 * angle;
    if (reciprocal) {
        angle = (MISL_MATH_PIO2_HI - angle) + MISL_MATH_PIO2_LO;
    }
    return angle;
}

MISL_MATH_API double MISL_CalculateArcTangent2(double y, double x) {
    if (misl_math_is_nan(x) || misl_math_is_nan(y)) {
        return MISL_MATH_D_NAN;
    }
    const uint64_t x_bits = misl_math_bits(x), y_bits = misl_math_bits(y);
    const int x_negative = (x_bits >> 63) != 0, y_negative = (y_bits >> 63) != 0;
    const int x_infinite = misl_math_is_infinite(x), y_infinite = misl_math_is_infinite(y);
    const int x_zero = (x_bits & 0x7FFFFFFFFFFFFFFFull) == 0, y_zero = (y_bits & 0x7FFFFFFFFFFFFFFFull) == 0;
    if (y_zero) {
        if (x_negative) {
            return y_negative ? -(MISL_MATH_PI_HI + MISL_MATH_PI_LO) : (MISL_MATH_PI_HI + MISL_MATH_PI_LO);
        }
        return y;
    }
    if (x_zero) {
        return y_negative ? -(MISL_MATH_PIO2_HI + MISL_MATH_PIO2_LO)
                          : (MISL_MATH_PIO2_HI + MISL_MATH_PIO2_LO);
    }
    if (x_infinite) {
        if (y_infinite) {
            double angle = x_negative ? 3.0 * (MISL_MATH_PIO4_HI + MISL_MATH_PIO4_LO)
                                      : (MISL_MATH_PIO4_HI + MISL_MATH_PIO4_LO);
            return y_negative ? -angle : angle;
        }
        double angle = x_negative ? (MISL_MATH_PI_HI + MISL_MATH_PI_LO) : 0.0;
        return y_negative ? -angle : angle;
    }
    if (y_infinite) {
        return y_negative ? -(MISL_MATH_PIO2_HI + MISL_MATH_PIO2_LO)
                          : (MISL_MATH_PIO2_HI + MISL_MATH_PIO2_LO);
    }
    double y_magnitude = y_negative ? -y : y, x_magnitude = x_negative ? -x : x;
    double angle = misl_math_atan_positive(y_magnitude / x_magnitude);
    if (x_negative) {
        angle = (MISL_MATH_PI_HI - angle) + MISL_MATH_PI_LO;
    }
    return y_negative ? -angle : angle;
}
