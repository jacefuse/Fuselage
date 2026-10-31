/* Rolling diagnostic history. SEE: (SHARP.md, Capture)
 * The sequence retry protocol does not establish safe concurrent buffer access. */
#include "sharp_facade_private.h"
#include <math.h>

#define SHARP_FACADE_CAPTURE_MAX_FRAMES 2048u
#define SHARP_FACADE_CAPTURE_SLOTS (SHARP_BUSES + 1)
typedef struct {
    _Atomic uint32_t sequence;
    uint32_t requested_frames;
    uint32_t write_head;
    uint32_t filled_frames;
    float samples[SHARP_FACADE_CAPTURE_MAX_FRAMES * 2];
} SharpFacadeCapture;

static SharpFacadeCapture sharp_facade_captures[SHARP_FACADE_CAPTURE_SLOTS];

static _Atomic uint32_t sharp_facade_capture_enabled[SHARP_FACADE_CAPTURE_SLOTS];

static int16_t sharp_facade_get_capture_s16(float sample) {
    if (isnan(sample)) {
        return 0;
    }
    if (sample > 1.0f) {
        sample = 1.0f;
    }
    if (sample < -1.0f) {
        sample = -1.0f;
    }
    return (int16_t)(sample * 32767.0f);
}

static int sharp_facade_get_capture_slot(int bus) {
    if (bus == SHARP_CAPTURE_MASTER) {
        return SHARP_BUSES;
    }
    return (bus >= 0 && bus < SHARP_BUSES) ? bus : -1;
}

void sharp_facade_push_capture(int slot, const float* stereo, uint32_t frames) {
    SharpFacadeCapture* capture = &sharp_facade_captures[slot];
    uint32_t requested_frames = capture->requested_frames;
    if (!requested_frames) {
        return;
    }

    uint32_t sequence_value = atomic_load_explicit(&capture->sequence, memory_order_relaxed);
    atomic_store_explicit(&capture->sequence, sequence_value + 1, memory_order_relaxed);
    atomic_thread_fence(memory_order_release);

    for (uint32_t frame_index = 0; frame_index < frames; frame_index++) {
        capture->samples[capture->write_head * 2 + 0] = stereo[(size_t)frame_index * 2 + 0];
        capture->samples[capture->write_head * 2 + 1] = stereo[(size_t)frame_index * 2 + 1];
        capture->write_head = (capture->write_head + 1) % requested_frames;
        if (capture->filled_frames < requested_frames) {
            capture->filled_frames++;
        }
    }

    atomic_thread_fence(memory_order_release);
    atomic_store_explicit(&capture->sequence, sequence_value + 2, memory_order_relaxed);
}

uint32_t SHARP_GetCaptureMaxFrames(void) {
    return SHARP_FACADE_CAPTURE_MAX_FRAMES;
}

bool SHARP_SetCapture(int bus, uint32_t frames, bool enabled) {
    int slot = sharp_facade_get_capture_slot(bus);
    if (slot < 0) {
        return false;
    }

    SharpFacadeCapture* capture = &sharp_facade_captures[slot];
    if (!enabled || frames == 0) {
        atomic_store_explicit(&sharp_facade_capture_enabled[slot], 0u, memory_order_release);
        capture->requested_frames = 0;
        return true;
    }
    if (frames > SHARP_FACADE_CAPTURE_MAX_FRAMES) {
        frames = SHARP_FACADE_CAPTURE_MAX_FRAMES;
    }

    capture->requested_frames = frames;
    capture->write_head = 0;
    capture->filled_frames = 0;
    atomic_store_explicit(&sharp_facade_capture_enabled[slot], 1u, memory_order_release);
    return true;
}

uint32_t SHARP_ReadCapture(int bus, void* destination, uint32_t max_frames, int format, int channels) {
    int slot = sharp_facade_get_capture_slot(bus);
    if (slot < 0 || !destination || max_frames == 0) {
        return 0;
    }
    if (channels != 1 && channels != 2) {
        channels = 2;
    }

    SharpFacadeCapture* capture = &sharp_facade_captures[slot];

    for (int attempt = 0; attempt < 4; attempt++) {
        uint32_t sequence_before = atomic_load_explicit(&capture->sequence, memory_order_relaxed);
        if (sequence_before & 1u) {
            continue;
        }
        atomic_thread_fence(memory_order_acquire);

        uint32_t requested_frames = capture->requested_frames, filled_frames = capture->filled_frames,
                 write_head = capture->write_head;
        if (!requested_frames || !filled_frames) {
            return 0;
        }

        uint32_t frame_count = (filled_frames < max_frames) ? filled_frames : max_frames;

        uint32_t start = (write_head + requested_frames - frame_count) % requested_frames;

        for (uint32_t frame_index = 0; frame_index < frame_count; frame_index++) {
            uint32_t ring_index = (start + frame_index) % requested_frames;
            float left = capture->samples[ring_index * 2 + 0], right = capture->samples[ring_index * 2 + 1];

            if (channels == 1) {
                float mono = (left + right) * 0.5f;
                if (format == SHARP_FMT_F32) {
                    ((float*)destination)[frame_index] = mono;
                } else {
                    ((int16_t*)destination)[frame_index] = sharp_facade_get_capture_s16(mono);
                }
            } else if (format == SHARP_FMT_F32) {
                ((float*)destination)[frame_index * 2 + 0] = left;
                ((float*)destination)[frame_index * 2 + 1] = right;
            } else {
                ((int16_t*)destination)[frame_index * 2 + 0] = sharp_facade_get_capture_s16(left);
                ((int16_t*)destination)[frame_index * 2 + 1] = sharp_facade_get_capture_s16(right);
            }
        }

        atomic_thread_fence(memory_order_acquire);
        if (sequence_before == atomic_load_explicit(&capture->sequence, memory_order_relaxed)) {
            return frame_count;
        }
    }

    return 0;
}

int sharp_facade_is_capture_enabled(int slot) {
    return atomic_load_explicit(&sharp_facade_capture_enabled[slot], memory_order_relaxed) != 0;
}
