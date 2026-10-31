#include "misl_format_internal.h"
#include <stdint.h>
#include <string.h>

// Integer digits and floating conversion.
// Compile with -ffp-contract=off and without fast-math.
// SEE: (MISL_FORMAT.md, Floating conversions)
const char* misl_format_digits32(char* end, unsigned value, unsigned base, int upper) {
    char* digit_start = end;
    do {
        unsigned digit = value % base;
        *--digit_start = (char)(digit < 10u ? '0' + digit : (upper ? 'A' : 'a') + (digit - 10u));
        value /= base;
    } while (value);
    return digit_start;
}

const char* misl_format_digits64(char* end, unsigned long long value, unsigned base, int upper) {
    char* digit_start = end;
    do {
        unsigned digit = (unsigned)(value % base);
        *--digit_start = (char)(digit < 10u ? '0' + digit : (upper ? 'A' : 'a') + (digit - 10u));
        value /= base;
    } while (value);
    return digit_start;
}

#if MISL_FORMAT_FLOAT

static const double misl_format_powers_of_ten[23] = {1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,
                                                     1e8,  1e9,  1e10, 1e11, 1e12, 1e13, 1e14, 1e15,
                                                     1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};

static double misl_format_power_of_ten(int exponent) {
    double result = 1.0;
    while (exponent > 22) {
        result *= 1e22;
        exponent -= 22;
    }
    return result * misl_format_powers_of_ten[exponent];
}

static double misl_format_scale(double value, int exponent) {
    if (exponent >= 0) {
        while (exponent > 22) {
            value *= 1e22;
            exponent -= 22;
        }
        return value * misl_format_powers_of_ten[exponent];
    }
    exponent = -exponent;
    while (exponent > 22) {
        value /= 1e22;
        exponent -= 22;
    }
    return value / misl_format_powers_of_ten[exponent];
}

static unsigned long long misl_format_double_bits(double value) {
    unsigned long long bits;
    memcpy(&bits, &value, 8);
    return bits;
}
int misl_format_is_nan(double value) {
    return value != value;
}
int misl_format_is_infinite(double value) {
    return ((misl_format_double_bits(value) >> 52) & 0x7FFu) == 0x7FFu &&
           (misl_format_double_bits(value) << 12) == 0ull;
}
int misl_format_sign_bit(double value) {
    return (int)(misl_format_double_bits(value) >> 63);
}

static unsigned misl_format_write_decimal(char* output, unsigned long long value, unsigned minimum_digits) {
    char scratch[24];
    const char* digits = misl_format_digits64(scratch + sizeof scratch, value, 10u, 0);
    unsigned digit_count = (unsigned)((scratch + sizeof scratch) - digits), position = 0u;
    while (digit_count + position < minimum_digits) {
        output[position++] = '0';
    }
    for (unsigned digit_index = 0; digit_index < digit_count; digit_index++) {
        output[position++] = digits[digit_index];
    }
    return position;
}

static void misl_format_product_parts(double first, double second, double* high, double* low) {
    const double splitter = 134217729.0;
    double product = first * second;
    double split = splitter * first;
    double first_high = split - (split - first), first_low = first - first_high;
    split = splitter * second;
    double second_high = split - (split - second), second_low = second - second_high;
    *high = product;
    *low = ((first_high * second_high - product) + first_high * second_low + first_low * second_high) +
           first_low * second_low;
}

static unsigned long long misl_format_round_product(double first, double second,
                                                    unsigned long long parity_base) {
    double high, low;
    misl_format_product_parts(first, second, &high, &low);
    unsigned long long integer = (unsigned long long)high;
    double remainder = high - (double)integer;
    if (remainder > 0.5) {
        integer++;
    } else if (remainder == 0.5) {
        if (low > 0.0) {
            integer++;
        } else if (low == 0.0 && ((integer + parity_base) & 1ull)) {
            integer++;
        }
    }
    return integer;
}

static int misl_format_scientific_digits(double value, int precision, unsigned long long* digits) {
    if (value == 0.0) {
        *digits = 0ull;
        return 0;
    }
    int binary_exponent = (int)((misl_format_double_bits(value) >> 52) & 0x7FFu) - 1023;
    if (binary_exponent == -1023) {
        binary_exponent = -1074;
    }
    int decimal_exponent = (int)((double)binary_exponent * 0.30102999566398120);
    double mantissa = misl_format_scale(value, -decimal_exponent);
    while (mantissa >= 10.0) {
        mantissa /= 10.0;
        decimal_exponent++;
    }
    while (mantissa < 1.0) {
        mantissa *= 10.0;
        decimal_exponent--;
    }
    unsigned long long rounded_digits =
        misl_format_round_product(mantissa, misl_format_power_of_ten(precision), 0ull);
    if (rounded_digits >= (unsigned long long)misl_format_power_of_ten(precision + 1)) {
        rounded_digits /= 10ull;
        decimal_exponent++;
    }
    *digits = rounded_digits;
    return decimal_exponent;
}

static unsigned misl_format_scientific_output(char* output, unsigned long long digits, int decimal_exponent,
                                              int precision, int alternate_form, int upper) {
    char scratch[24];
    (void)misl_format_write_decimal(scratch, digits, (unsigned)precision + 1u);
    unsigned position = 0u;
    output[position++] = scratch[0];
    if (precision > 0 || alternate_form) {
        output[position++] = '.';
    }
    for (int digit_index = 1; digit_index <= precision; digit_index++) {
        output[position++] = scratch[digit_index];
    }
    output[position++] = upper ? 'E' : 'e';
    output[position++] = (decimal_exponent < 0) ? '-' : '+';
    position += misl_format_write_decimal(
        output + position, (unsigned long long)(decimal_exponent < 0 ? -decimal_exponent : decimal_exponent),
        2u);
    output[position] = '\0';
    return position;
}

unsigned misl_format_scientific(char* output, double value, int precision, int alternate_form, int upper) {
    unsigned long long digits;
    int decimal_exponent = misl_format_scientific_digits(value, precision, &digits);
    return misl_format_scientific_output(output, digits, decimal_exponent, precision, alternate_form, upper);
}

unsigned misl_format_fixed(char* output, double value, int precision, int alternate_form) {
    double scale = misl_format_power_of_ten(precision);
    unsigned long long fraction = 0ull;
    unsigned position = 0u;
    if (value < 9.2e18) {
        unsigned long long integer_part = (unsigned long long)value;

        fraction = misl_format_round_product(value - (double)integer_part, scale,
                                             (precision == 0) ? (integer_part & 1ull) : 0ull);
        // General format may need more fractional places than fit in a 64-bit scale.
        if (scale < 0x1p64 && fraction >= (unsigned long long)scale) {
            integer_part++;
            fraction = 0ull;
        }
        position += misl_format_write_decimal(output + position, integer_part, 1u);
    } else {

        double quotient = value / 1e18;
        if (quotient >= 9.2e18) {
            return misl_format_scientific(output, value, precision, alternate_form, 0);
        }
        unsigned long long high_digits = (unsigned long long)quotient;
        double remainder = value - (double)high_digits * 1e18;
        if (remainder < 0.0) {
            high_digits--;
            remainder += 1e18;
        }
        if (remainder >= 1e18) {
            high_digits++;
            remainder -= 1e18;
        }
        position += misl_format_write_decimal(output + position, high_digits, 1u);
        position += misl_format_write_decimal(output + position, (unsigned long long)remainder, 18u);
    }
    if (precision > 0 || alternate_form) {
        output[position++] = '.';
    }
    if (precision > 0) {
        position += misl_format_write_decimal(output + position, fraction, (unsigned)precision);
    }
    output[position] = '\0';
    return position;
}

static unsigned misl_format_strip_zeros(char* output, unsigned length) {
    unsigned decimal_point = length;
    for (unsigned index = 0; index < length; index++) {
        if (output[index] == '.') {
            decimal_point = index;
            break;
        }
    }
    if (decimal_point == length) {
        return length;
    }
    unsigned exponent_start = length;
    for (unsigned index = decimal_point; index < length; index++) {
        if (output[index] == 'e' || output[index] == 'E') {
            exponent_start = index;
            break;
        }
    }
    unsigned fraction_end = exponent_start;
    while (fraction_end > decimal_point + 1u && output[fraction_end - 1u] == '0') {
        fraction_end--;
    }
    if (fraction_end == decimal_point + 1u) {
        fraction_end = decimal_point;
    }
    for (unsigned index = 0; index < length - exponent_start; index++) {
        output[fraction_end + index] = output[exponent_start + index];
    }
    length = fraction_end + (length - exponent_start);
    output[length] = '\0';
    return length;
}

unsigned misl_format_general(char* output, double value, int precision, int alternate_form, int upper) {
    if (precision == 0) {
        precision = 1;
    }
    unsigned long long digits;
    int decimal_exponent = misl_format_scientific_digits(value, precision - 1, &digits);
    unsigned length;
    if (value != 0.0 && (decimal_exponent < -4 || decimal_exponent >= precision)) {
        length = misl_format_scientific_output(output, digits, decimal_exponent, precision - 1,
                                               alternate_form, upper);
    } else {
        length = misl_format_fixed(output, value, precision - 1 - decimal_exponent, alternate_form);
    }
    return alternate_form ? length : misl_format_strip_zeros(output, length);
}

#endif
