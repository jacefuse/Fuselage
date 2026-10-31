// SPSC audio transport. SEE: (SHARP.md, Audio ring)
#include "sharp_ring.h"
#include <stdlib.h>
#include <string.h>

static uint32_t sharp_audio_ring_round_capacity(uint32_t requested_capacity) {
    uint32_t capacity = 1;
    while (capacity < requested_capacity) {
        if (capacity > (UINT32_MAX >> 1)) {
            return capacity;
        }
        capacity <<= 1;
    }
    return capacity;
}

#define SHARP_RING_MAX_FRAMES (1u << 24)
#define SHARP_RING_MAX_CHANNELS 8u

int SHARP_InitAudioRing(SharpRing* ring, uint32_t frames, uint32_t channels) {
    memset(ring, 0, sizeof *ring);

    if (frames == 0) {
        frames = 1;
    }
    if (frames > SHARP_RING_MAX_FRAMES) {
        return 0;
    }
    if (channels == 0 || channels > SHARP_RING_MAX_CHANNELS) {
        return 0;
    }

    ring->cap = sharp_audio_ring_round_capacity(frames);
    ring->mask = ring->cap - 1;
    ring->ch = channels;

    ring->buf = calloc((size_t)ring->cap * channels, sizeof(float));
    if (!ring->buf) {
        return 0;
    }
    atomic_store(&ring->r, 0);
    atomic_store(&ring->w, 0);
    return 1;
}

void SHARP_FreeAudioRing(SharpRing* ring) {
    free(ring->buf);
    ring->buf = NULL;
}

void SHARP_ResetAudioRing(SharpRing* ring) {
    atomic_store(&ring->r, 0);
    atomic_store(&ring->w, 0);
}

uint32_t SHARP_GetAudioRingReadableFrames(const SharpRing* ring) {
    uint32_t write_cursor = atomic_load_explicit((_Atomic uint32_t*)&ring->w, memory_order_acquire);
    uint32_t read_cursor = atomic_load_explicit((_Atomic uint32_t*)&ring->r, memory_order_relaxed);
    return write_cursor - read_cursor;
}

uint32_t SHARP_GetAudioRingWritableFrames(const SharpRing* ring) {
    uint32_t write_cursor = atomic_load_explicit((_Atomic uint32_t*)&ring->w, memory_order_relaxed);
    uint32_t read_cursor = atomic_load_explicit((_Atomic uint32_t*)&ring->r, memory_order_acquire);
    return ring->cap - (write_cursor - read_cursor);
}

uint32_t SHARP_WriteAudioRing(SharpRing* ring, const float* source, uint32_t frames) {
    uint32_t write_cursor = atomic_load_explicit(&ring->w, memory_order_relaxed);
    uint32_t read_cursor = atomic_load_explicit(&ring->r, memory_order_acquire);
    uint32_t writable_frames = ring->cap - (write_cursor - read_cursor);
    if (frames > writable_frames) {
        frames = writable_frames;
    }

    uint32_t first_segment_frames = ring->cap - (write_cursor & ring->mask);
    if (first_segment_frames > frames) {
        first_segment_frames = frames;
    }
    memcpy(ring->buf + (size_t)(write_cursor & ring->mask) * ring->ch, source,
           (size_t)first_segment_frames * ring->ch * sizeof(float));
    if (frames > first_segment_frames) {
        memcpy(ring->buf, source + (size_t)first_segment_frames * ring->ch,
               (size_t)(frames - first_segment_frames) * ring->ch * sizeof(float));
    }

    atomic_store_explicit(&ring->w, write_cursor + frames, memory_order_release);
    return frames;
}

uint32_t SHARP_ReadAudioRing(SharpRing* ring, float* destination, uint32_t frames) {
    uint32_t read_cursor = atomic_load_explicit(&ring->r, memory_order_relaxed);
    uint32_t write_cursor = atomic_load_explicit(&ring->w, memory_order_acquire);
    uint32_t readable_frames = write_cursor - read_cursor;
    if (frames > readable_frames) {
        frames = readable_frames;
    }

    uint32_t first_segment_frames = ring->cap - (read_cursor & ring->mask);
    if (first_segment_frames > frames) {
        first_segment_frames = frames;
    }
    memcpy(destination, ring->buf + (size_t)(read_cursor & ring->mask) * ring->ch,
           (size_t)first_segment_frames * ring->ch * sizeof(float));
    if (frames > first_segment_frames) {
        memcpy(destination + (size_t)first_segment_frames * ring->ch, ring->buf,
               (size_t)(frames - first_segment_frames) * ring->ch * sizeof(float));
    }

    atomic_store_explicit(&ring->r, read_cursor + frames, memory_order_release);
    return frames;
}
