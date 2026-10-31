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

    for (uint32_t frame_index = 0; frame_index < frames; frame_index++) {
        for (int generator_index = 0; generator_index < AMY_ENV_GENS; generator_index++) {
            AmyEnv* envelope = &chip->env[generator_index];
            envelope->acc += 1.0;
            while (envelope->step_len > 0.0 && envelope->acc >= envelope->step_len) {
                envelope->acc -= envelope->step_len;
                sharp_amy_advance_envelope(envelope);
                envelope->step_len = envelope->pending_len;
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
                    voice->count -= (double)period;
                    /* Extra expiries are discarded here. SEE: (SHARP.md, Amy divider limits) */
                    if (voice->count >= (double)period) {
                        voice->count = 0.0;
                    }
                    voice->high = !voice->high;
                    if (voice->high) {
                        voice->clocked = 1;
                    }
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
                        noise->count -= (double)period;
                        if (noise->count >= (double)period) {
                            noise->count = 0.0;
                        }
                        noise->half ^= 1;
                        if (!noise->half) {
                            sharp_amy_shift_noise(noise);
                        }
                    }
                } else {
                    unsigned source_index = (unsigned)(noise->r_source - 1);
                    if (source_index < AMY_VOICES && chip->voices[source_index].clocked) {
                        sharp_amy_shift_noise(noise);
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
        const float dc_coefficient = 1.0f - 2.0f * 3.14159265f * 10.0f / (float)chip->device_rate;
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
