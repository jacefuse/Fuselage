// Direct MISL math implementation API. SEE: (MISL_MATH.md, Direct and ROM interfaces)
#ifndef MISL_MATH_H
#define MISL_MATH_H

#ifndef MISL_MATH_API
#define MISL_MATH_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

MISL_MATH_API double MISL_CalculateSine(double x);
MISL_MATH_API double MISL_CalculateCosine(double x);
MISL_MATH_API double MISL_CalculateTangent(double x);
MISL_MATH_API double MISL_CalculateArcTangent2(double y, double x);
MISL_MATH_API double MISL_CalculateExponential(double x);
MISL_MATH_API double MISL_CalculateNaturalLogarithm(double x);
MISL_MATH_API double MISL_CalculatePower(double base, double exponent);
MISL_MATH_API double MISL_RoundDown(double x);
MISL_MATH_API double MISL_RoundUp(double x);
MISL_MATH_API double MISL_Truncate(double x);
MISL_MATH_API double MISL_Round(double x);

MISL_MATH_API float MISL_CalculateSineFloat(float x);
MISL_MATH_API float MISL_CalculateCosineFloat(float x);
MISL_MATH_API float MISL_CalculateTangentFloat(float x);
MISL_MATH_API float MISL_CalculateArcTangent2Float(float y, float x);
MISL_MATH_API float MISL_CalculateExponentialFloat(float x);
MISL_MATH_API float MISL_CalculateNaturalLogarithmFloat(float x);
MISL_MATH_API float MISL_CalculatePowerFloat(float base, float exponent);
MISL_MATH_API float MISL_RoundDownFloat(float x);
MISL_MATH_API float MISL_RoundUpFloat(float x);
MISL_MATH_API float MISL_TruncateFloat(float x);
MISL_MATH_API float MISL_RoundFloat(float x);

#ifdef __cplusplus
}
#endif

#endif
