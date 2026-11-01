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

        /* Seeds stay within the 17 stages of the default white mode; a mode or
         * length write reseeds to one. SEE: (SHARP.md, Amy registers and synthesis) */
        noise->lfsr = (0x1FFFFu ^ (uint32_t)(generator_index * 0x9E3779B9u)) & 0x1FFFFu;
        if (noise->lfsr == 0) {
            noise->lfsr = 1;
        }
    }

    /* Step timing is derived now from period one and the clock above, so a shape
     * write alone runs its envelope. Until then the default shape-zero decay runs
     * from full level and rests at zero. SEE: (SHARP.md, Amy envelope timing) */
    for (int generator_index = 0; generator_index < AMY_ENV_GENS; generator_index++) {
        chip->env[generator_index].r_period_lo = 1;
        chip->env[generator_index].level = AMY_VOL_LEVELS - 1;
    }
    sharp_amy_refresh_envelope_steps(chip);

    return;
}

/* A voice is active while a running source reaches the output: a tone or valid
 * noise route under a nonzero fixed volume or an envelope that is not resting at
 * silence, or a moving envelope alone, which buzzes through the disabled sources.
 * Routes the mixer ignores are ignored here too. SEE: (SHARP.md, Amy activity reporting) */
int GetAmyActiveVoiceCount(const Amy* chip) {
    int active_count = 0;
    for (int voice_index = 0; voice_index < AMY_VOICES; voice_index++) {
        const AmyVoice* voice = &chip->voices[voice_index];
        const int tone_on = (voice->r_control & AMY_CTL_TONE) != 0;
        const int noise_on = voice->r_noise_route != 0 && (unsigned)(voice->r_noise_route - 1) < AMY_NOISE_GENS;
        const int envelope_on = voice->r_env_route != 0 && (unsigned)(voice->r_env_route - 1) < AMY_ENV_GENS;
        int level_on, moving = 0;
        if (envelope_on) {
            const AmyEnv* envelope = &chip->env[voice->r_env_route - 1];
            moving = !envelope->holding;
            level_on = moving || envelope->level > 0;
        } else {
            level_on = voice->r_volume != 0;
        }
        if (level_on && (tone_on || noise_on || moving)) {
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
