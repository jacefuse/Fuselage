#include "misl_math_internal.h"

// Float operations. SEE: (MISL_MATH.md, Source organization)
static float misl_math_narrow(double value) {
    return (float)value;
}

static float misl_math_float_nan(void) {
    float value;
    uint32_t bits = MISL_MATH_F_NAN_BITS;
    memcpy(&value, &bits, 4);
    return value;
}

static int misl_math_is_float_nan(float value) {
    return value != value;
}

MISL_MATH_API float MISL_CalculateSineFloat(float x) {
    return misl_math_is_float_nan(x) ? misl_math_float_nan()
                                     : misl_math_narrow(MISL_CalculateSine((double)x));
}

MISL_MATH_API float MISL_CalculateCosineFloat(float x) {
    return misl_math_is_float_nan(x) ? misl_math_float_nan()
                                     : misl_math_narrow(MISL_CalculateCosine((double)x));
}

MISL_MATH_API float MISL_CalculateTangentFloat(float x) {
    return misl_math_is_float_nan(x) ? misl_math_float_nan()
                                     : misl_math_narrow(MISL_CalculateTangent((double)x));
}

MISL_MATH_API float MISL_CalculateArcTangent2Float(float y, float x) {
    return (misl_math_is_float_nan(x) || misl_math_is_float_nan(y))
               ? misl_math_float_nan()
               : misl_math_narrow(MISL_CalculateArcTangent2((double)y, (double)x));
}

MISL_MATH_API float MISL_CalculateExponentialFloat(float x) {
    return misl_math_is_float_nan(x) ? misl_math_float_nan()
                                     : misl_math_narrow(MISL_CalculateExponential((double)x));
}

MISL_MATH_API float MISL_CalculateNaturalLogarithmFloat(float x) {
    return misl_math_is_float_nan(x) ? misl_math_float_nan()
                                     : misl_math_narrow(MISL_CalculateNaturalLogarithm((double)x));
}

MISL_MATH_API float MISL_CalculatePowerFloat(float base, float exponent) {
    return misl_math_narrow(MISL_CalculatePower((double)base, (double)exponent));
}

MISL_MATH_API float MISL_RoundDownFloat(float x) {
    return misl_math_narrow(MISL_RoundDown((double)x));
}

MISL_MATH_API float MISL_RoundUpFloat(float x) {
    return misl_math_narrow(MISL_RoundUp((double)x));
}

MISL_MATH_API float MISL_TruncateFloat(float x) {
    return misl_math_narrow(MISL_Truncate((double)x));
}

MISL_MATH_API float MISL_RoundFloat(float x) {
    return misl_math_narrow(MISL_Round((double)x));
}
