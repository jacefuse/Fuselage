// Borrowed diagnostics and host mask copies. SEE: (GDMF_PIXIES.md, RAM and diagnostics)
#include "gdmf_pixies_state_private.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static char gdmf_pixies_orphan_diagnostic[PIXIE_OUTPUT_STRING_LEN];

char* gdmf_pixies_diagnostic_buffer(int id) {
    gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    return state ? state->diagnostic : gdmf_pixies_orphan_diagnostic;
}

bool gdmf_pixies_refuse(int id, const char* format, ...) {
    char* destination = gdmf_pixies_diagnostic_buffer(id);
    va_list arguments;
    va_start(arguments, format);
    int written = vsnprintf(destination, PIXIE_OUTPUT_STRING_LEN, format, arguments);
    va_end(arguments);
    if (written < 0) {
        destination[0] = '\0';
    }
    destination[PIXIE_OUTPUT_STRING_LEN - 1] = '\0';
    return false;
}

size_t ReadPixieString(int id, char* buffer, size_t capacity) {
    if (!buffer || capacity == 0) {
        return 0;
    }
    const char* source = gdmf_pixies_diagnostic_buffer(id);
    size_t length = 0;
    while (length < PIXIE_OUTPUT_STRING_LEN && source[length]) {
        ++length;
    }
    if (length >= capacity) {
        length = capacity - 1;
    }
    memcpy(buffer, source, length);
    buffer[length] = '\0';
    return length;
}

size_t ReadPixieMask(int id, unsigned char* buffer, size_t capacity) {
    const gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    if (!buffer || !capacity || !state || state->mode != PIXIE_MODE_MASK || !state->mask) {
        return 0;
    }
    size_t count = (size_t)state->output_width * (size_t)state->output_height;
    if (count > capacity) {
        count = capacity;
    }
    memcpy(buffer, state->mask, count);
    return count;
}
