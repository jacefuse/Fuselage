#pragma once
#include <stddef.h>

// Standard names served through the system ROM. SEE: (BOOTSPUR.md, Memory and strings)
#ifdef __cplusplus
extern "C" {
#endif

void* memcpy(void* destination, const void* source, size_t bytes);
void* memset(void* destination, int value, size_t bytes);
void* memmove(void* destination, const void* source, size_t bytes);
int memcmp(const void* first, const void* second, size_t bytes);
size_t strlen(const char* string);

#ifdef __cplusplus
}
#endif
