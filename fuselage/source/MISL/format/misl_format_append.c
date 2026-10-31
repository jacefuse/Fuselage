#include "misl_format.h"

// Native primitives; guest definitions are ROM thunks.
// SEE: (MISL_FORMAT.md, Append primitives)
#if !defined(__riscv)

unsigned MISL_AppendStr(char* buffer, unsigned capacity, unsigned position, const char* string) {
    if (!buffer || capacity == 0u) {
        return 0u;
    }
    if (position >= capacity) {
        position = capacity - 1u;
    }
    if (string) {
        while (*string && position < capacity - 1u) {
            buffer[position++] = *string++;
        }
    }
    buffer[position] = '\0';
    return position;
}

unsigned MISL_AppendFixed(char* buffer, unsigned capacity, unsigned position, int value, unsigned decimals) {
    if (!buffer || capacity == 0u) {
        return 0u;
    }
    if (position >= capacity) {
        position = capacity - 1u;
    }
    if (decimals > 9u) {
        decimals = 9u;
    }

    char scratch[16];
    char* digit_start = scratch + sizeof scratch - 1;
    *digit_start = '\0';
    unsigned magnitude = (value < 0) ? 0u - (unsigned)value : (unsigned)value;
    unsigned digit_count = 0u;
    do {
        *--digit_start = (char)('0' + magnitude % 10u);
        magnitude /= 10u;
        digit_count++;
        if (decimals && digit_count == decimals) {
            *--digit_start = '.';
            if (magnitude == 0u) {
                *--digit_start = '0';
            }
        }
    } while (magnitude != 0u || digit_count < decimals);
    if (value < 0) {
        *--digit_start = '-';
    }
    for (; *digit_start && position < capacity - 1u; digit_start++, position++) {
        buffer[position] = *digit_start;
    }
    buffer[position] = '\0';
    return position;
}

unsigned MISL_AppendInt(char* buffer, unsigned capacity, unsigned position, int value) {
    return MISL_AppendFixed(buffer, capacity, position, value, 0u);
}

#endif
