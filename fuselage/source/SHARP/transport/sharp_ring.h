// Interleaved audio frame transport. SEE: (SHARP.md, Audio ring)
#ifndef SHARP_RING_H
#define SHARP_RING_H
#include <stdint.h>
#include <stdatomic.h>

typedef struct {
    float* buf;
    uint32_t cap;
    uint32_t mask;
    uint32_t ch;
    _Atomic uint32_t r;
    _Atomic uint32_t w;
} SharpRing;

// One producer and one consumer. Reset/free require both sides to stop.
int SHARP_InitAudioRing(SharpRing* ring, uint32_t frames, uint32_t channels);
void SHARP_FreeAudioRing(SharpRing* ring);
void SHARP_ResetAudioRing(SharpRing* ring);
uint32_t SHARP_GetAudioRingReadableFrames(const SharpRing* ring);
uint32_t SHARP_GetAudioRingWritableFrames(const SharpRing* ring);
uint32_t SHARP_WriteAudioRing(SharpRing* ring, const float* source, uint32_t frames);
uint32_t SHARP_ReadAudioRing(SharpRing* ring, float* destination, uint32_t frames);
#endif
