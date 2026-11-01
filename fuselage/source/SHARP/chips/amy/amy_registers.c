#include "amy_private.h"
#include <math.h>

/* A period written below the count owed forces one expiry at the next substep,
 * as a counter that has already passed its new period does, instead of a burst.
 * SEE: (SHARP.md, Amy divider timing) */
static void sharp_amy_bound_count(double* count, unsigned period) {
    const double limit = (double)(period ? period : 1u);
    if (*count > limit) {
        *count = limit;
    }
}

void WriteAmyRegister(Amy* chip, uint16_t reg, uint16_t value) {
    if (reg < AMY_REG_GLOBAL) {
        const unsigned voice_index = reg / AMY_REG_STRIDE;
        AmyVoice* voice = &chip->voices[voice_index];
        switch (reg % AMY_REG_STRIDE) {
        case AMY_REG_TONE_PERIOD:
            voice->r_tone_period = value & 0x0FFF;
            sharp_amy_bound_count(&voice->count, voice->r_tone_period);
            break;
        case AMY_REG_VOLUME:
            voice->r_volume = value > 255 ? 255 : value;
            break;
        case AMY_REG_PAN:
            voice->r_pan = value > 255 ? 255 : value;
            sharp_amy_set_voice_gains(voice);
            break;
        case AMY_REG_CONTROL:
            voice->r_control = value;
            break;
        case AMY_REG_NOISE_ROUTE:
            voice->r_noise_route = value;
            break;
        case AMY_REG_ENV_ROUTE:
            voice->r_env_route = value;
            break;
        default:
            break;
        }
        return;
    }

    const uint16_t global_offset = (uint16_t)(reg - AMY_REG_GLOBAL);

    if (global_offset < AMY_NOISE_GENS_MAX * AMY_NOISE_STRIDE) {
        const unsigned generator_index = global_offset / AMY_NOISE_STRIDE;

        if (generator_index >= AMY_NOISE_GENS) {
            return;
        }
        AmyNoise* noise = &chip->noise[generator_index];
        switch (global_offset % AMY_NOISE_STRIDE) {
        case AMY_NREG_PERIOD:
            noise->r_period = value & 0x0FFF;
            sharp_amy_bound_count(&noise->count, noise->r_period);
            break;

        case AMY_NREG_MODE:
            noise->r_mode = (value > AMY_NOISE_WHITE_SEGA) ? AMY_NOISE_WHITE_AY : value;
            noise->lfsr = 1u;
            noise->out = 1;
            break;
        case AMY_NREG_LENGTH:
            noise->r_length = value;
            noise->lfsr = 1u;
            noise->out = 1;
            break;
        case AMY_NREG_SOURCE:
            noise->r_source = value;
            break;
        default:
            break;
        }
        return;
    }

    const uint16_t envelope_offset = (uint16_t)(global_offset - AMY_NOISE_GENS_MAX * AMY_NOISE_STRIDE);
    if (envelope_offset < AMY_ENV_GENS_MAX * AMY_ENV_STRIDE) {
        const unsigned generator_index = envelope_offset / AMY_ENV_STRIDE;
        if (generator_index >= AMY_ENV_GENS) {
            return;
        }
        AmyEnv* envelope = &chip->env[generator_index];
        switch (envelope_offset % AMY_ENV_STRIDE) {
        case AMY_EREG_PERIOD_HI:
            envelope->r_period_hi = value;
            sharp_amy_refresh_envelope_step(chip, envelope);
            break;
        case AMY_EREG_PERIOD_LO:
            envelope->r_period_lo = value;
            sharp_amy_refresh_envelope_step(chip, envelope);
            break;
        case AMY_EREG_SHAPE:

            envelope->r_shape = value & 0x0F;
            sharp_amy_restart_envelope(envelope);
            break;
        default:
            break;
        }
        return;
    }

    switch (reg) {
    case AMY_REG_CLOCK_HI:
        chip->r_clock_hi = value;
        chip->clock = (double)(((uint32_t)chip->r_clock_hi << 16) | chip->r_clock_lo);
        if (chip->clock < 1.0) {
            chip->clock = 1.0;
        }
        sharp_amy_refresh_envelope_steps(chip);
        break;
    case AMY_REG_CLOCK_LO:
        chip->r_clock_lo = value;
        chip->clock = (double)(((uint32_t)chip->r_clock_hi << 16) | chip->r_clock_lo);
        if (chip->clock < 1.0) {
            chip->clock = 1.0;
        }
        sharp_amy_refresh_envelope_steps(chip);
        break;
    case AMY_REG_VOLUME_TABLE:
        chip->r_volume_table = (value > AMY_VOL_YM2149_REAL) ? AMY_VOL_AY8910 : value;
        break;
    case AMY_REG_MASTER_VOLUME:
        chip->r_master_volume = value > 255 ? 255 : value;
        break;
    default:
        break;
    }

    return;
}
