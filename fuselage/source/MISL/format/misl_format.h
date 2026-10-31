#ifndef MISL_FORMAT_H
#define MISL_FORMAT_H
#include <stdarg.h>

// Bounded append/format interface. SEE: (BOOTSPUR.md, Formatting interface)
#ifdef __cplusplus
extern "C" {
#endif

unsigned MISL_AppendStr(char* buffer, unsigned capacity, unsigned position, const char* string);
unsigned MISL_AppendInt(char* buffer, unsigned capacity, unsigned position, int value);
unsigned MISL_AppendFixed(char* buffer, unsigned capacity, unsigned position, int value, unsigned decimals);

// Return characters stored, excluding NUL, rather than snprintf's required length.
unsigned MISL_Format(char* buffer, unsigned capacity, const char* format, ...);
unsigned MISL_FormatV(char* buffer, unsigned capacity, const char* format, va_list arguments);

#ifdef __cplusplus
}
#endif
#endif
