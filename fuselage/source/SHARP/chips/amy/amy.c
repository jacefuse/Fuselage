#include "amy_private.h"
#include <math.h>
#include <string.h>

void sharp_amy_set_voice_gains(AmyVoice* voice) {
    float pan = ((float)voice->r_pan - 128.0f) / 127.0f;
    if (pan < -1.0f) {
        pan = -1.0f;
    }
    if (pan > 1.0f) {
        pan = 1.0f;
    }
    float angle = (pan * 0.5f + 0.5f) * 1.57079632679f;
    voice->lg = cosf(angle);
    voice->rg = sinf(angle);

    return;
}

void InitAmy(Amy* chip, double device_rate) {
    sharp_amy_ensure_volume_tables();
    memset(chip, 0, sizeof *chip);
    chip->device_rate = device_rate;
    chip->r_master_volume = 255;
    chip->r_volume_table = AMY_VOL_AY8910;
    chip->r_clock_hi = (uint16_t)(AMY_CLOCK_DEFAULT >> 16);
    chip->r_clock_lo = (uint16_t)(AMY_CLOCK_DEFAULT & 0xFFFF);
    chip->clock = (double)AMY_CLOCK_DEFAULT;

    for (int voice_index = 0; voice_index < AMY_VOICES; voice_index++) {
        AmyVoice* voice = &chip->voices[voice_index];
        voice->r_control = AMY_CTL_TONE;
        voice->r_pan = 128;

        voice->r_volume = 0;

        sharp_amy_set_voice_gains(voice);
    }

    for (int generator_index = 0; generator_index < AMY_NOISE_GENS; generator_index++) {
        AmyNoise* noise = &chip->noise[generator_index];
        noise->r_period = 1;
        noise->r_mode = AMY_NOISE_WHITE_AY;
        noise->r_length = AMY_NOISE_LENGTH_SN;
        noise->r_source = AMY_NOISE_SOURCE_OWN;

        noise->lfsr = 0x1FFFFu ^ (uint32_t)(generator_index * 0x9E3779B9u);
        if (noise->lfsr == 0) {
            noise->lfsr = 1;
        }
    }

    /* Timing remains lazy until a period/clock write. SEE: (SHARP.md, Amy envelope timing) */
    for (int generator_index = 0; generator_index < AMY_ENV_GENS; generator_index++) {
        chip->env[generator_index].r_period_lo = 1;
        chip->env[generator_index].level = AMY_VOL_LEVELS - 1;
    }

    return;
}

int GetAmyActiveVoiceCount(const Amy* chip) {
    int active_count = 0;
    for (int voice_index = 0; voice_index < AMY_VOICES; voice_index++) {
        const AmyVoice* voice = &chip->voices[voice_index];
        const int tone_on = (voice->r_control & AMY_CTL_TONE) != 0;
        const int noise_on = voice->r_noise_route != 0;
        if (!tone_on && !noise_on) {
            continue;
        }
        if (voice->r_env_route != 0 || voice->r_volume != 0) {
            active_count++;
        }
    }
    return active_count;
}

static void sharp_amy_render_callback(void* context, float* output, uint32_t frames) {
    RenderAmy((Amy*)context, output, frames);
}

static void sharp_amy_write_callback(void* context, uint16_t reg, uint16_t value) {
    WriteAmyRegister((Amy*)context, reg, value);
}

SharpChip GetAmySource(Amy* chip) {
    SharpChip source;
    source.ctx = chip;
    source.render = sharp_amy_render_callback;
    source.write_reg = sharp_amy_write_callback;
    source.name = "Amy";
    return source;
}
