/* Native float/PCM16 conversion. SEE: (SHARP_LINUX.md, Stream flow and counters) */
#include "sharp_alsa_private.h"
#include <math.h>

static int16_t sharp_alsa_get_s16_sample(float sample) {
    if (isnan(sample)) {
        return 0;
    }
    if (sample > 1.0f) {
        sample = 1.0f;
    }
    if (sample < -1.0f) {
        sample = -1.0f;
    }
    return (int16_t)(sample * 32767.0f + (sample >= 0 ? 0.5f : -0.5f));
}

void sharp_alsa_copy_frames(const SharpAlsaState* implementation, void* destination, uint32_t frames) {
    uint32_t channels = implementation->device_channels;
    if (implementation->format == SHARP_ALSA_F32) {
        float* output = destination;
        for (uint32_t frame_index = 0; frame_index < frames; ++frame_index) {
            output[(size_t)frame_index * channels] = implementation->device_scratch[(size_t)frame_index * 2];
            if (channels > 1) {
                output[(size_t)frame_index * channels + 1] =
                    implementation->device_scratch[(size_t)frame_index * 2 + 1];
            }
            for (uint32_t channel_index = 2; channel_index < channels; ++channel_index) {
                output[(size_t)frame_index * channels + channel_index] = 0;
            }
        }
    } else {
        int16_t* output = destination;
        for (uint32_t frame_index = 0; frame_index < frames; ++frame_index) {
            for (uint32_t channel_index = 0; channel_index < channels; ++channel_index) {
                float sample = channel_index < 2
                                   ? implementation->device_scratch[(size_t)frame_index * 2 + channel_index]
                                   : 0;
                output[(size_t)frame_index * channels + channel_index] = sharp_alsa_get_s16_sample(sample);
            }
        }
    }
}
