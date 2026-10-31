#include "misl_format.h"
#include "misl_format_internal.h"
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#if defined(__riscv)
#include "../bootspur/misl_bootspur_services.h"
#else
#include <stdio.h>
#endif

// Variadic parsing, bounded output and unsupported-conversion reporting.
// SEE: (MISL_FORMAT.md, Format strings and results)
typedef struct {
    char* buffer;
    unsigned capacity, position;
} misl_format_buffer;

static void misl_format_put(misl_format_buffer* output, char character) {
    if (output->position + 1u < output->capacity) {
        output->buffer[output->position++] = character;
    }
}

static void misl_format_fill(misl_format_buffer* output, char character, unsigned count) {
    const unsigned available = output->capacity - 1u - output->position;
    if (count > available) {
        count = available;
    }
    while (count--) {
        misl_format_put(output, character);
    }
}

static void misl_format_report(const char* format, char conversion) {
#if defined(__riscv)
    char line[120];
    char character[2] = {conversion ? conversion : '?', '\0'};
    unsigned position = 0u;
    position = MISL_AppendStr(line, sizeof line, position, "MISL_Format: unsupported conversion %");
    position = MISL_AppendStr(line, sizeof line, position, character);
    position = MISL_AppendStr(line, sizeof line, position, " in \"");
    position = MISL_AppendStr(line, sizeof line, position, format);
    (void)MISL_AppendStr(line, sizeof line, position, "\"");
    (void)MISL_Trace(line);
#elif !defined(NDEBUG)
    fprintf(stderr, "[MISL] MISL_Format: unsupported conversion %%%c in \"%s\"\n",
            conversion ? conversion : '?', format);
#else
    (void)format;
    (void)conversion;
#endif
}

unsigned MISL_FormatV(char* buffer, unsigned capacity, const char* format, va_list arguments) {
    if (!buffer || capacity == 0u) {
        return 0u;
    }
    if (!format) {
        format = "(null)";
    }
    misl_format_buffer output = {buffer, capacity, 0u};

    for (const char* cursor = format; *cursor; cursor++) {
        if (*cursor != '%') {
            misl_format_put(&output, *cursor);
            continue;
        }
        cursor++;

        int left_justified = 0, zero_padding = 0, show_plus = 0, space_sign = 0, alternate_form = 0;
        for (;; cursor++) {
            if (*cursor == '-') {
                left_justified = 1;
            } else if (*cursor == '0') {
                zero_padding = 1;
            } else if (*cursor == '+') {
                show_plus = 1;
            } else if (*cursor == ' ') {
                space_sign = 1;
            } else if (*cursor == '#') {
                alternate_form = 1;
            } else {
                break;
            }
        }

        unsigned width = 0u;
        if (*cursor == '*') {
            int dynamic_width = va_arg(arguments, int);
            if (dynamic_width < 0) {
                left_justified = 1;
                width = 0u - (unsigned)dynamic_width;
            } else {
                width = (unsigned)dynamic_width;
            }
            cursor++;
        } else {
            while (*cursor >= '0' && *cursor <= '9') {
                const unsigned digit = (unsigned)(*cursor++ - '0');
                width = width > (UINT_MAX - digit) / 10u ? UINT_MAX : width * 10u + digit;
            }
        }

        int precision = -1;
        if (*cursor == '.') {
            cursor++;
            precision = 0;
            if (*cursor == '*') {
                precision = va_arg(arguments, int);
                cursor++;
                if (precision < 0) {
                    precision = -1;
                }
            } else {
                while (*cursor >= '0' && *cursor <= '9') {
                    const int digit = *cursor++ - '0';
                    precision = precision > (INT_MAX - digit) / 10 ? INT_MAX : precision * 10 + digit;
                }
            }
        }

        enum {
            MISL_FORMAT_LENGTH_DEFAULT,
            MISL_FORMAT_LENGTH_CHAR,
            MISL_FORMAT_LENGTH_SHORT,
            MISL_FORMAT_LENGTH_LONG,
            MISL_FORMAT_LENGTH_LONG_LONG,
            MISL_FORMAT_LENGTH_SIZE
        } length = MISL_FORMAT_LENGTH_DEFAULT;
        if (*cursor == 'h') {
            cursor++;
            length = MISL_FORMAT_LENGTH_SHORT;
            if (*cursor == 'h') {
                cursor++;
                length = MISL_FORMAT_LENGTH_CHAR;
            }
        } else if (*cursor == 'l') {
            cursor++;
            length = MISL_FORMAT_LENGTH_LONG;
            if (*cursor == 'l') {
                cursor++;
                length = MISL_FORMAT_LENGTH_LONG_LONG;
            }
        } else if (*cursor == 'z') {
            cursor++;
            length = MISL_FORMAT_LENGTH_SIZE;
        }

        char scratch[80];
        char* scratch_end = scratch + sizeof scratch;
        const char* body = scratch;
        unsigned body_bytes = 0u;
        char sign = 0;
        const char* prefix = "";
        int is_numeric = 0;
        int is_float = 0;
        char conversion = *cursor;

        switch (conversion) {
        case 'd':
        case 'i': {
            is_numeric = 1;
            long long signed_value;
            switch (length) {
            case MISL_FORMAT_LENGTH_CHAR:
                signed_value = (signed char)va_arg(arguments, int);
                break;
            case MISL_FORMAT_LENGTH_SHORT:
                signed_value = (short)va_arg(arguments, int);
                break;
            case MISL_FORMAT_LENGTH_LONG:
                signed_value = va_arg(arguments, long);
                break;
            case MISL_FORMAT_LENGTH_LONG_LONG:
                signed_value = va_arg(arguments, long long);
                break;
            case MISL_FORMAT_LENGTH_SIZE:
                signed_value = (long long)va_arg(arguments, ptrdiff_t);
                break;
            default:
                signed_value = va_arg(arguments, int);
                break;
            }
            if (signed_value < 0) {
                sign = '-';
            } else if (show_plus) {
                sign = '+';
            } else if (space_sign) {
                sign = ' ';
            }
            unsigned long long magnitude = (signed_value < 0) ? 0ull - (unsigned long long)signed_value
                                                              : (unsigned long long)signed_value;
            if (magnitude <= 0x7FFFFFFFull) {

                body_bytes = MISL_AppendInt(scratch, sizeof scratch, 0u, (int)magnitude);
                body = scratch;
            } else {
                body = misl_format_digits64(scratch_end, magnitude, 10u, 0);
                body_bytes = (unsigned)(scratch_end - body);
            }
            if (precision == 0 && magnitude == 0ull) {
                body_bytes = 0u;
            }
            break;
        }
        case 'u':
        case 'x':
        case 'X':
        case 'o': {
            is_numeric = 1;
            unsigned base = (conversion == 'u') ? 10u : (conversion == 'o') ? 8u : 16u;
            int upper = (conversion == 'X');
            unsigned long long magnitude;
            switch (length) {
            case MISL_FORMAT_LENGTH_CHAR:
                magnitude = (unsigned char)va_arg(arguments, unsigned);
                break;
            case MISL_FORMAT_LENGTH_SHORT:
                magnitude = (unsigned short)va_arg(arguments, unsigned);
                break;
            case MISL_FORMAT_LENGTH_LONG:
                magnitude = va_arg(arguments, unsigned long);
                break;
            case MISL_FORMAT_LENGTH_LONG_LONG:
                magnitude = va_arg(arguments, unsigned long long);
                break;
            case MISL_FORMAT_LENGTH_SIZE:
                magnitude = va_arg(arguments, size_t);
                break;
            default:
                magnitude = va_arg(arguments, unsigned);
                break;
            }
            body = (magnitude <= 0xFFFFFFFFull)
                       ? misl_format_digits32(scratch_end, (unsigned)magnitude, base, upper)
                       : misl_format_digits64(scratch_end, magnitude, base, upper);
            body_bytes = (unsigned)(scratch_end - body);
            if (precision == 0 && magnitude == 0ull) {
                body_bytes = 0u;
            }
            if (alternate_form) {
                if (base == 16u && magnitude != 0ull) {
                    prefix = upper ? "0X" : "0x";
                } else if (base == 8u) {
                    if (magnitude == 0ull && precision == 0) {
                        body_bytes = 1u;
                    } else if (magnitude != 0ull && (precision < 0 || (unsigned)precision <= body_bytes)) {
                        prefix = "0";
                    }
                }
            }
            break;
        }
        case 'p': {
            is_numeric = 1;
            unsigned long long magnitude = (unsigned long long)(uintptr_t)va_arg(arguments, void*);
            body = (magnitude <= 0xFFFFFFFFull)
                       ? misl_format_digits32(scratch_end, (unsigned)magnitude, 16u, 0)
                       : misl_format_digits64(scratch_end, magnitude, 16u, 0);
            body_bytes = (unsigned)(scratch_end - body);
            prefix = "0x";
            break;
        }
        case 's': {
            const char* string = va_arg(arguments, const char*);
            if (!string) {
                string = "(null)";
            }
            if (precision >= 0) {
                while (body_bytes < (unsigned)precision && string[body_bytes]) {
                    body_bytes++;
                }
            } else {
                body_bytes = (unsigned)strlen(string);
            }
            body = string;
            break;
        }
        case 'c':
            scratch[0] = (char)va_arg(arguments, int);
            body_bytes = 1u;
            break;
        case '%':
            scratch[0] = '%';
            body_bytes = 1u;
            break;
#if MISL_FORMAT_FLOAT
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G': {
            is_numeric = 1;
            is_float = 1;
            double floating_value = va_arg(arguments, double);
            int upper = (conversion == 'F' || conversion == 'E' || conversion == 'G');
            int is_nan = misl_format_is_nan(floating_value);
            int is_negative = !is_nan && misl_format_sign_bit(floating_value);
            double absolute_value = is_negative ? -floating_value : floating_value;
            if (is_negative) {
                sign = '-';
            } else if (show_plus) {
                sign = '+';
            } else if (space_sign) {
                sign = ' ';
            }
            if (is_nan) {
                body = upper ? "NAN" : "nan";
                body_bytes = 3u;
                zero_padding = 0;
            } else if (misl_format_is_infinite(absolute_value)) {
                body = upper ? "INF" : "inf";
                body_bytes = 3u;
                zero_padding = 0;
            } else {
                int effective_precision = (precision < 0) ? 6 : (precision > 17 ? 17 : precision);
                char lowercase_conversion = (char)(conversion | 0x20);
                if (lowercase_conversion == 'f') {
                    body_bytes =
                        misl_format_fixed(scratch, absolute_value, effective_precision, alternate_form);
                } else if (lowercase_conversion == 'e') {
                    body_bytes = misl_format_scientific(scratch, absolute_value, effective_precision,
                                                        alternate_form, upper);
                } else {
                    body_bytes = misl_format_general(scratch, absolute_value, effective_precision,
                                                     alternate_form, upper);
                }
                body = scratch;
            }
            break;
        }
        case 'n':
        case 'a':
        case 'A':
#else
        case 'n':
        case 'a':
        case 'A':
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
#endif

            if (conversion == 'n') {
                switch (length) {
                case MISL_FORMAT_LENGTH_CHAR:
                    (void)va_arg(arguments, signed char*);
                    break;
                case MISL_FORMAT_LENGTH_SHORT:
                    (void)va_arg(arguments, short*);
                    break;
                case MISL_FORMAT_LENGTH_LONG:
                    (void)va_arg(arguments, long*);
                    break;
                case MISL_FORMAT_LENGTH_LONG_LONG:
                    (void)va_arg(arguments, long long*);
                    break;
                case MISL_FORMAT_LENGTH_SIZE:
                    (void)va_arg(arguments, ptrdiff_t*);
                    break;
                default:
                    (void)va_arg(arguments, int*);
                    break;
                }
            } else {
                (void)va_arg(arguments, double);
            }
            misl_format_put(&output, '%');
            misl_format_put(&output, '!');
            misl_format_put(&output, conversion);
            misl_format_report(format, conversion);
            continue;
        default:

            misl_format_put(&output, '%');
            misl_format_put(&output, '!');
            if (conversion) {
                misl_format_put(&output, conversion);
            }
            misl_format_report(format, conversion);
            output.buffer[output.position] = '\0';
            return output.position;
        }

        unsigned digits = body_bytes;
        if (is_numeric && !is_float && precision >= 0 && (unsigned)precision > digits) {
            digits = (unsigned)precision;
        }
        unsigned prefix_bytes = prefix[0] ? (prefix[1] ? 2u : 1u) : 0u;
        unsigned total_bytes = digits + (sign ? 1u : 0u) + prefix_bytes;
        unsigned padding = (width > total_bytes) ? width - total_bytes : 0u;
        int use_zero_padding = (!left_justified && zero_padding && is_numeric && (precision < 0 || is_float));

        if (!left_justified && !use_zero_padding) {
            misl_format_fill(&output, ' ', padding);
        }
        if (sign) {
            misl_format_put(&output, sign);
        }
        for (unsigned index = 0; index < prefix_bytes; index++) {
            misl_format_put(&output, prefix[index]);
        }
        if (use_zero_padding) {
            misl_format_fill(&output, '0', padding);
        }
        misl_format_fill(&output, '0', digits - body_bytes);
        if (conversion == 's' && precision < 0) {

            output.position = MISL_AppendStr(output.buffer, output.capacity, output.position, body);
        } else {
            for (unsigned index = 0; index < body_bytes; index++) {
                misl_format_put(&output, body[index]);
            }
        }
        if (left_justified) {
            misl_format_fill(&output, ' ', padding);
        }
    }

    output.buffer[output.position] = '\0';
    return output.position;
}

unsigned MISL_Format(char* buffer, unsigned capacity, const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    unsigned stored_bytes = MISL_FormatV(buffer, capacity, format, arguments);
    va_end(arguments);
    return stored_bytes;
}
