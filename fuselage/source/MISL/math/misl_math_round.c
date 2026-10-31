#include "misl_math_internal.h"

// Round operations. SEE: (MISL_MATH.md, Source organization)
MISL_MATH_API double MISL_Truncate(double value) {
    uint64_t bits = misl_math_bits(value);
    int binary_exponent = (int)((bits >> 52) & 0x7FF) - 1023;
    if (binary_exponent >= 52) {
        return value;
    }
    if (binary_exponent < 0) {
        return misl_math_from_bits(bits & 0x8000000000000000ull);
    }
    uint64_t mask = 0x000FFFFFFFFFFFFFull >> binary_exponent;
    return misl_math_from_bits(bits & ~mask);
}

MISL_MATH_API double MISL_RoundDown(double value) {
    double integer_part = MISL_Truncate(value);
    return (integer_part > value) ? integer_part - 1.0 : integer_part;
}

MISL_MATH_API double MISL_RoundUp(double value) {
    double integer_part = MISL_Truncate(value);
    return (integer_part < value) ? integer_part + 1.0 : integer_part;
}

MISL_MATH_API double MISL_Round(double value) {
    if (misl_math_is_nan(value) || misl_math_is_infinite(value)) {
        return value;
    }
    double integer_part = MISL_Truncate(value);
    double fraction = value - integer_part;
    if (fraction >= 0.5) {
        return integer_part + 1.0;
    }
    if (fraction <= -0.5) {
        return integer_part - 1.0;
    }
    return integer_part;
}
