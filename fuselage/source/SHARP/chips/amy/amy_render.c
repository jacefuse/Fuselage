#include "amy_private.h"
#include <math.h>

static float sharp_amy_get_volume_amplitude(const float* table, unsigned volume, int fine) {
    if (!fine) {
        const int step = (int)(volume * 16u / 256u) * (AMY_VOL_LEVELS / 16) + (AMY_VOL_LEVELS / 16 - 1);
        return table[step];
    }
    const double position = (double)volume * (double)(AMY_VOL_LEVELS - 1) / 255.0;
    int level_index = (int)position;
    if (level_index > AMY_VOL_LEVELS - 2) {
        return table[AMY_VOL_LEVELS - 1];
    }
    const double fraction = position - (double)level_index;
    return (float)((double)table[level_index] +
                   ((double)table[level_index + 1] - (double)table[level_index]) * fraction);
}

/* Steps an envelope takes individually in one frame before whole repeating
 * cycles are skipped arithmetically. Ordinary clocks need a few dozen at most.
 * SEE: (SHARP.md, Amy envelope timing) */
#define AMY_ENV_DIRECT_STEPS_PER_FRAME 256.0

static void sharp_amy_advance_envelope(AmyEnv* envelope) {
    if (envelope->holding) {
        return;
    }

    envelope->level += envelope->rising ? 1 : -1;
    if (envelope->level >= 0 && envelope->level < AMY_VOL_LEVELS) {
        return;
    }

    const int shape = envelope->r_shape;

    if (!(shape & AMY_ENV_CONTINUE)) {
        envelope->level = 0;
        envelope->holding = 1;
        return;
    }

    if (shape & AMY_ENV_HOLD) {
        envelope->level = (shape & AMY_ENV_ALTERNATE) ? ((shape & AMY_ENV_ATTACK) ? 0 : AMY_VOL_LEVELS - 1)
                                                      : ((shape & AMY_ENV_ATTACK) ? AMY_VOL_LEVELS - 1 : 0);
        envelope->holding = 1;
        return;
    }

    if (shape & AMY_ENV_ALTERNATE) {
        /* Repeat the endpoint when changing direction. */
        envelope->rising = !envelope->rising;
        envelope->level = envelope->rising ? 0 : (AMY_VOL_LEVELS - 1);
        return;
    }

    envelope->level = envelope->rising ? 0 : (AMY_VOL_LEVELS - 1);

    return;
}

/* Noise shifts handled for one generator in one substep. Real clocks need a
 * few at most; the cap bounds the loop for extreme clock-to-rate ratios.
 * SEE: (SHARP.md, Amy divider timing) */
#define AMY_NOISE_SHIFTS_PER_SUBSTEP 64u

/* Removes every whole period from the count and returns how many there were,
 * at least one. The usual single expiry is one subtraction; fmod is exact, so
 * several leave the same kind of remainder. */
static unsigned sharp_amy_take_expiries(double* count, unsigned period) {
    const double length = (double)period;
    if (*count < 2.0 * length) {
        *count -= length;
        return 1u;
    }
    const double remainder = fmod(*count, length);
    const double whole = (*count - remainder) / length;
    *count = remainder;
    return whole >= 4294967295.0 ? 4294967295u : (unsigned)whole;
}

/* Removes whole repeating cycles owed by a steady, repeating envelope, so an
 * extreme clock costs at most one cycle of individual steps per frame. Shapes
 * that hold reach their hold within one ramp and need no skipping. */
static void sharp_amy_skip_envelope_cycles(AmyEnv* envelope) {
    const int shape = envelope->r_shape;
    if (!(shape & AMY_ENV_CONTINUE) || (shape & AMY_ENV_HOLD) || envelope->step_len != envelope->pending_len) {
        return;
    }
    const double cycle_len =
        envelope->step_len * ((shape & AMY_ENV_ALTERNATE) ? 2.0 * AMY_VOL_LEVELS : (double)AMY_VOL_LEVELS);
    const double cycles = floor(envelope->acc / cycle_len);
    if (cycles >= 1.0) {
        envelope->acc -= cycles * cycle_len;
    }
}

static void sharp_amy_shift_noise(AmyNoise* noise) {
    uint32_t shift_register = noise->lfsr;

    switch (noise->r_mode) {
    case AMY_NOISE_WHITE_SN: {
        uint32_t feedback_bit = ((shift_register ^ (shift_register >> 1)) & 1u);
        shift_register = (shift_register >> 1) | (feedback_bit << 14);
        break;
    }
    case AMY_NOISE_WHITE_SEGA: {
        uint32_t feedback_bit = ((shift_register ^ (shift_register >> 3)) & 1u);
        shift_register = (shift_register >> 1) | (feedback_bit << 15);
        break;
    }
    case AMY_NOISE_PERIODIC: {
        unsigned length = noise->r_length ? noise->r_length : AMY_NOISE_LENGTH_SN;
        if (length > 31) {
            length = 31;
        }
        uint32_t mask = (1u << length) - 1u;
        shift_register &= mask;
        if (shift_register == 0) {
            shift_register = 1u;
        }
        shift_register = ((shift_register >> 1) | ((shift_register & 1u) << (length - 1))) & mask;
        break;
    }
    default: {
        uint32_t feedback_bit = ((shift_register ^ (shift_register >> 3)) & 1u);
        shift_register = (shift_register >> 1) | (feedback_bit << 16);
        break;
    }
    }

    if (shift_register == 0) {
        shift_register = 1u;
    }
    noise->lfsr = shift_register;
    noise->out = (int)(shift_register & 1u);

    return;
}

void RenderAmy(Amy* chip, float* output, uint32_t frames) {
    if (!isfinite(chip->device_rate) || chip->device_rate <= 0.0) {
        return;
    }

    const float* table = sharp_amy_get_volume_table(chip->r_volume_table);
    const float master_volume = (float)chip->r_master_volume / 255.0f;

    const double inverse_oversampling = 1.0 / (double)AMY_OVERSAMPLE;

    /* One-pole DC block with a 10 Hz corner. The exponential form stays inside
     * (0, 1) at every positive rate. SEE: (SHARP.md, Amy output filtering) */
    const float dc_coefficient = expf(-2.0f * 3.14159265f * 10.0f / (float)chip->device_rate);

    for (uint32_t frame_index = 0; frame_index < frames; frame_index++) {
        for (int generator_index = 0; generator_index < AMY_ENV_GENS; generator_index++) {
            AmyEnv* envelope = &chip->env[generator_index];
            if (envelope->holding) {
                continue; /* Only a shape write moves it again, and that restarts its timing. */
            }
            envelope->acc += 1.0;
            while (envelope->step_len > 0.0 && envelope->acc >= envelope->step_len) {
                if (envelope->acc >= envelope->step_len * AMY_ENV_DIRECT_STEPS_PER_FRAME) {
                    sharp_amy_skip_envelope_cycles(envelope);
                    if (envelope->acc < envelope->step_len) {
                        break;
                    }
                }
                envelope->acc -= envelope->step_len;
                sharp_amy_advance_envelope(envelope);
                envelope->step_len = envelope->pending_len;
                if (envelope->holding) {
                    break; /* The rest of the frame's steps would change nothing. */
                }
            }
        }

        float output_left = 0.0f, output_right = 0.0f;

        const double prescale_ticks = chip->clock / (8.0 * chip->device_rate) * inverse_oversampling;

        for (int substep_index = 0; substep_index < AMY_OVERSAMPLE; substep_index++) {
            for (int generator_index = 0; generator_index < AMY_VOICES; generator_index++) {
                AmyVoice* voice = &chip->voices[generator_index];
                voice->clocked = 0;
                voice->count += prescale_ticks;
                unsigned period = voice->r_tone_period & 0x0FFF;
                if (period == 0) {
                    period = 1;
                }
                if (voice->count >= (double)period) {
                    /* Every whole period owed is an expiry; the output keeps the parity
                     * and the noise SOURCE sees each rising edge. */
                    const unsigned expiries = sharp_amy_take_expiries(&voice->count, period);
                    voice->clocked = (int)((expiries + (unsigned)!voice->high) / 2u);
                    voice->high ^= (int)(expiries & 1u);
                }
            }

            for (int generator_index = 0; generator_index < AMY_NOISE_GENS; generator_index++) {
                AmyNoise* noise = &chip->noise[generator_index];
                if (noise->r_source == AMY_NOISE_SOURCE_OWN) {
                    noise->count += prescale_ticks;
                    unsigned period = noise->r_period & 0x0FFF;
                    if (period == 0) {
                        period = 1;
                    }
                    if (noise->count >= (double)period) {
                        /* The register shifts on every second expiry, as the half flag returns to zero. */
                        const unsigned expiries = sharp_amy_take_expiries(&noise->count, period);
                        unsigned shifts = (expiries + (unsigned)noise->half) / 2u;
                        noise->half ^= (int)(expiries & 1u);
                        if (shifts > AMY_NOISE_SHIFTS_PER_SUBSTEP) {
                            shifts = AMY_NOISE_SHIFTS_PER_SUBSTEP;
                        }
                        while (shifts--) {
                            sharp_amy_shift_noise(noise);
                        }
                    }
                } else {
                    unsigned source_index = (unsigned)(noise->r_source - 1);
                    if (source_index < AMY_VOICES) {
                        unsigned shifts = (unsigned)chip->voices[source_index].clocked;
                        if (shifts > AMY_NOISE_SHIFTS_PER_SUBSTEP) {
                            shifts = AMY_NOISE_SHIFTS_PER_SUBSTEP;
                        }
                        while (shifts--) {
                            sharp_amy_shift_noise(noise);
                        }
                    }
                }
            }

            for (int generator_index = 0; generator_index < AMY_VOICES; generator_index++) {
                AmyVoice* voice = &chip->voices[generator_index];

                const int tone_on = (voice->r_control & AMY_CTL_TONE) != 0;
                const unsigned noise_index = (unsigned)(voice->r_noise_route - 1);
                const int noise_on = (voice->r_noise_route != 0) && (noise_index < AMY_NOISE_GENS);

                /* Disabled sources read high, so the envelope can sound on its own. */
                const int tone_bit = tone_on ? voice->high : 1;
                const int noise_bit = noise_on ? chip->noise[noise_index].out : 1;
                const int mixer_bit = tone_bit & noise_bit;

                float amplitude;
                const unsigned envelope_index = (unsigned)(voice->r_env_route - 1);
                if (voice->r_env_route != 0 && envelope_index < AMY_ENV_GENS) {
                    int level = chip->env[envelope_index].level;
                    if (level < 0) {
                        level = 0;
                    }
                    if (level >= AMY_VOL_LEVELS) {
                        level = AMY_VOL_LEVELS - 1;
                    }
                    amplitude = table[level];
                } else {
                    amplitude = sharp_amy_get_volume_amplitude(table, voice->r_volume,
                                                               (voice->r_control & AMY_CTL_FINE_VOLUME) != 0);
                }

                const float voice_sample = mixer_bit ? (amplitude * AMY_VOICE_SCALE) : 0.0f;
                output_left += voice_sample * voice->lg;
                output_right += voice_sample * voice->rg;
            }
        }

        output_left *= (float)inverse_oversampling;
        output_right *= (float)inverse_oversampling;

        /* Remove the changing DC offset of the unipolar tone/noise mix. */
        float filtered_left = output_left - chip->dc_x[0] + dc_coefficient * chip->dc_y[0];
        chip->dc_x[0] = output_left;
        chip->dc_y[0] = filtered_left;
        float filtered_right = output_right - chip->dc_x[1] + dc_coefficient * chip->dc_y[1];
        chip->dc_x[1] = output_right;
        chip->dc_y[1] = filtered_right;

        output[(size_t)frame_index * 2 + 0] += filtered_left * master_volume;
        output[(size_t)frame_index * 2 + 1] += filtered_right * master_volume;
    }

    return;
}
