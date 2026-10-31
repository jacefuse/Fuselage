#include "gdmf_textlayer.h"
#include <stdarg.h>
#include <stdio.h>

static int gdmf_textlayer_format(Color color, const char* format, va_list arguments) {
    if (!format) {
        return 0;
    }
    char output[256];
    int required_bytes = vsnprintf(output, sizeof(output), format, arguments);
    if (required_bytes < 0 || (size_t)required_bytes >= sizeof(output)) {
        return -1;
    }
    return tlPrintC(output, color);
}
int tlPrintFormatted(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    int result = gdmf_textlayer_format(tlGetColor(), format, arguments);
    va_end(arguments);
    return result;
}
int tlPrintFormattedC(Color color, const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    int result = gdmf_textlayer_format(color, format, arguments);
    va_end(arguments);
    return result;
}
