/* Device container selection and stereo mapping. SEE: (SHARP_WINDOWS.md, WASAPI output) */
#include "sharp_wasapi_private.h"
#include <math.h>

static const GUID sharp_wasapi_float_subtype = {
    0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}};
static const GUID sharp_wasapi_pcm_subtype = {
    0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}};

int sharp_wasapi_select_format(const WAVEFORMATEX* format, SharpWasapiFormat* selected) {
    if (!format || !format->nChannels || !format->nSamplesPerSec) {
        return 0;
    }
    int is_float = format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
    int is_pcm = format->wFormatTag == WAVE_FORMAT_PCM;
    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        if (format->cbSize < sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
            return 0;
        }
        const WAVEFORMATEXTENSIBLE* extended = (const WAVEFORMATEXTENSIBLE*)format;
        is_float = IsEqualGUID(&extended->SubFormat, &sharp_wasapi_float_subtype);
        is_pcm = IsEqualGUID(&extended->SubFormat, &sharp_wasapi_pcm_subtype);
        if (extended->Samples.wValidBitsPerSample &&
            extended->Samples.wValidBitsPerSample != format->wBitsPerSample) {
            return 0;
        }
    }
    if (is_float && format->wBitsPerSample == 32) {
        *selected = SHARP_WASAPI_F32;
    } else if (is_pcm && format->wBitsPerSample == 16) {
        *selected = SHARP_WASAPI_S16;
    } else {
        return 0;
    }
    return (uint32_t)format->nChannels * (format->wBitsPerSample / 8u) == format->nBlockAlign;
}

static int16_t sharp_wasapi_get_s16_sample(float sample) {
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

void sharp_wasapi_copy_frames(const SharpWasapiState* implementation, void* destination, uint32_t frames) {
    uint32_t channels = implementation->device_channels;
    if (implementation->format == SHARP_WASAPI_F32) {
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
                output[(size_t)frame_index * channels + channel_index] = sharp_wasapi_get_s16_sample(sample);
            }
        }
    }
}
