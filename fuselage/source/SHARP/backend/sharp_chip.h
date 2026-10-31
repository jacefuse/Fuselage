// Independent additive stereo source. SEE: (SHARP.md, Chip callback contract)
#ifndef SHARP_CHIP_H
#define SHARP_CHIP_H
#include <stdint.h>

typedef struct {
    void* ctx;
    // Add frames to the caller's interleaved stereo buffer; do not clear it.
    void (*render)(void* context, float* output, uint32_t frames);
    // Apply the register write at the current render position.
    void (*write_reg)(void* context, uint16_t reg, uint16_t value);
    const char* name;
} SharpChip;
#endif
