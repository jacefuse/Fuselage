// Guest header subset. SEE: (BOOTSPUR.md, Freestanding include path)
#ifndef FUSELAGE_GUEST_MATH_H
#define FUSELAGE_GUEST_MATH_H

#include "../bootspur.h"

#define M_E        2.7182818284590452354
#define M_PI       3.14159265358979323846
#define M_PI_2     1.57079632679489661923
#define M_PI_4     0.78539816339744830962
#define M_1_PI     0.31830988618379067154
#define M_2_PI     0.63661977236758134308
#define M_SQRT2    1.41421356237309504880
#define M_SQRT1_2  0.70710678118654752440

#define INFINITY   (__builtin_inff())
#define NAN        (__builtin_nanf(""))
#define HUGE_VALF  INFINITY
#define HUGE_VAL   ((double)INFINITY)

#define isnan(x)   __builtin_isnan(x)
#define isinf(x)   __builtin_isinf(x)
#define isfinite(x) __builtin_isfinite(x)

#endif /* FUSELAGE_GUEST_MATH_H */
