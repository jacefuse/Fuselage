// Adora instances and source adapters. SEE: (SHARP.md, Adora sample playback)
#include "adora_private.h"
#include <math.h>
#include <string.h>

void InitAdora(Adora* chip, double device_rate, double sample_rate, const uint8_t* pool, uint32_t pool_size) {
    memset(chip, 0, sizeof *chip);
    chip->device_rate = device_rate;
    chip->sample_rate = sample_rate;
    chip->pool = pool;
    chip->pool_size = pool_size;

    chip->gain_k =
        (device_rate > 0.0) ? (float)(1.0 - exp(-1.0 / (ADORA_GAIN_RAMP_MS * 0.001 * device_rate))) : 1.0f;
    for (int voice_index = 0; voice_index < ADORA_VOICES; voice_index++) {
        chip->voices[voice_index].r_pitch = ADORA_PITCH_ONE;

        chip->voices[voice_index].r_base_rate = 0u;
        chip->voices[voice_index].r_volume = 255;
        chip->voices[voice_index].r_pan = 128;

        chip->voices[voice_index].r_env_ad = 0x0000;
        chip->voices[voice_index].r_env_sr = 0xFF00;
    }
}

int GetAdoraActiveVoiceCount(const Adora* chip) {
    int count = 0;
    for (int voice_index = 0; voice_index < ADORA_VOICES; voice_index++) {
        if (chip->voices[voice_index].active) {
            count++;
        }
    }
    return count;
}

int PlayAdoraNote(Adora* chip, uint32_t offset, uint32_t length, double pitch_ratio, float gain, float pan) {
    double pitch_value = pitch_ratio * ADORA_PITCH_ONE + 0.5;
    float gain_value = gain * 255.0f + 0.5f;
    float pan_value = (pan * 0.5f + 0.5f) * 255.0f + 0.5f;
    if (!isfinite(pitch_value) || pitch_value < 0.0 || pitch_value >= 65536.0 || !isfinite(gain_value) ||
        gain_value < 0.0f || gain_value >= 65536.0f || !isfinite(pan_value) || pan_value < 0.0f ||
        pan_value >= 65536.0f) {
        return -1;
    }
    for (int voice_index = 0; voice_index < ADORA_VOICES; voice_index++) {
        if (chip->voices[voice_index].active) {
            continue;
        }
        AdoraVoice* voice = &chip->voices[voice_index];
        voice->r_offset = offset;
        voice->r_length = length;
        voice->r_pitch = (uint16_t)pitch_value;

        voice->r_base_rate = 0u;
        voice->r_volume = (uint16_t)gain_value;
        voice->r_pan = (uint16_t)pan_value;
        sharp_adora_trigger_voice(chip, voice);
        return voice_index;
    }
    return -1;
}

static void sharp_adora_render_callback(void* context, float* output, uint32_t frames) {
    RenderAdora((Adora*)context, output, frames);
}

static void sharp_adora_write_callback(void* context, uint16_t reg, uint16_t value) {
    WriteAdoraRegister((Adora*)context, reg, value);
}

SharpChip GetAdoraSource(Adora* chip) {
    SharpChip source;
    source.ctx = chip;
    source.render = sharp_adora_render_callback;
    source.write_reg = sharp_adora_write_callback;
    source.name = "Adora";
    return source;
}
