// Timestamped register scheduling. SEE: (SHARP.md, Event sequencing)
#ifndef SHARP_EVENTS_H
#define SHARP_EVENTS_H
#include <stdint.h>
#include <stdatomic.h>
#include "../backend/sharp_chip.h"

typedef struct {
    uint64_t time; // Absolute render-frame position.
    uint8_t chip;
    uint8_t _pad;
    uint16_t reg;
    uint16_t value;
} SharpEvent;

typedef struct {
    SharpEvent* buf;
    uint32_t cap;
    uint32_t mask;
    _Atomic uint32_t r;
    _Atomic uint32_t w;
    _Atomic uint32_t dropped;
    _Atomic uint32_t late;
    _Atomic uint32_t disordered;
    uint64_t last_time;
} SharpEventRing;

#define SHARP_MAX_CHIPS 16

typedef struct {
    SharpEventRing ring;
    SharpChip chips[SHARP_MAX_CHIPS];
    uint8_t slot[SHARP_MAX_CHIPS];
    int chip_count;
    uint64_t pos;
} SharpSeq;

// Init returns 1 on success, 0 for an invalid capacity or allocation failure.
// SEE: (SHARP.md, Event sequencing)
int SHARP_InitSequence(SharpSeq* sequence, uint32_t event_capacity);
void SHARP_FreeSequence(SharpSeq* sequence);
int SHARP_AddSequenceChip(SharpSeq* sequence, const SharpChip* chip); // ID or -1 when full.
int SHARP_WriteSequence(SharpSeq* sequence, uint64_t position, uint8_t chip, uint16_t reg, uint16_t value);
int SHARP_SetSequenceOutput(SharpSeq* sequence, int chip, int output_slot);
int SHARP_GetSequenceOutput(const SharpSeq* sequence, int chip);
void SHARP_RenderSequence(SharpSeq* sequence, float* output, uint32_t frames);
void SHARP_RenderSequenceOutputs(SharpSeq* sequence, float* const* outputs, int output_count, uint32_t frames);
uint64_t SHARP_GetSequencePosition(const SharpSeq* sequence);
uint32_t SHARP_GetSequenceDroppedEventCount(const SharpSeq* sequence);
uint32_t SHARP_GetSequenceLateEventCount(const SharpSeq* sequence);
uint32_t SHARP_GetSequenceDisorderedEventCount(const SharpSeq* sequence);
#endif
