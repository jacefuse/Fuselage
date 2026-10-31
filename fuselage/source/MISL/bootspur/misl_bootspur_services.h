#pragma once
#include <stdbool.h>
#include <stdint.h>

// Guest diagnostics and save writes. SEE: (BOOTSPUR.md, Diagnostics and saves)
#ifdef __cplusplus
extern "C" {
#endif

// Whether the host printed the line; output depends on host configuration and rate limits.
bool MISL_Trace(const char* line);
// True after a nonempty write; false for zero bytes. Invalid nonempty ranges halt.
bool MISL_WriteSram(uint32_t offset, const void* source, uint32_t bytes);

#ifdef __cplusplus
}
#endif
