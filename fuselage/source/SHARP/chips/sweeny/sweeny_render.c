// Sweeny render. SEE: (SHARP.md, Sweeny synthesis)
#include "sweeny_private.h"
#include <math.h>
#include <string.h>

#define SHARP_SWEENY_CUTOFF_MIN 30.0
#define SHARP_SWEENY_CUTOFF_MAX 12000.0
#define SHARP_SWEENY_ENVELOPE_EPSILON 0.0005f

static float sharp_sweeny_step_envelope(SweenyVoice* voice) {
    switch (voice->env_stage) {
    case SWEENY_ENV_ATTACK:
        voice->env_level += voice->env_a_inc;
        if (voice->env_level >= 1.0f) {
            voice->env_level = 1.0f;
            voice->env_stage = SWEENY_ENV_DECAY;
        }
        break;
    case SWEENY_ENV_DECAY:
        voice->env_level += (voice->env_sustain - voice->env_level) * voice->env_d_coef;
        if (fabsf(voice->env_level - voice->env_sustain) < SHARP_SWEENY_ENVELOPE_EPSILON) {
            voice->env_level = voice->env_sustain;
            voice->env_stage = SWEENY_ENV_SUSTAIN;
        }
        break;
    case SWEENY_ENV_SUSTAIN:
        voice->env_level = voice->env_sustain;
        break;
    case SWEENY_ENV_RELEASE:
        voice->env_level -= voice->env_level * voice->env_r_coef;
        if (voice->env_level < SHARP_SWEENY_ENVELOPE_EPSILON) {
            voice->env_level = 0.0f;
            voice->env_stage = SWEENY_ENV_IDLE;
        }
        break;
    default:
        voice->env_level = 0.0f;
        break;
    }

    return voice->env_level;
}

static float sharp_sweeny_polyblep(double phase, double phase_step) {
    if (phase < phase_step) {
        phase = phase / phase_step;
        return (float)(phase + phase - phase * phase - 1.0);
    }
    if (phase > 1.0 - phase_step) {
        phase = (phase - 1.0) / phase_step;
        return (float)(phase * phase + phase + phase + 1.0);
    }

    return 0.0f;
}

static void sharp_sweeny_clock_noise(SweenyVoice* voice) {
    uint32_t bit = ((voice->noise_lfsr >> 22) ^ (voice->noise_lfsr >> 17)) & 1u;
    voice->noise_lfsr = ((voice->noise_lfsr << 1) | bit) & 0x7FFFFFu;
    voice->noise_out = (float)((int32_t)(voice->noise_lfsr & 0xFFFFu) - 32768) / 32768.0f;

    return;
}

void RenderSweeny(Sweeny* chip, float* output, uint32_t frames) {
    const double rate = chip->device_rate;
    if (!isfinite(rate) || rate <= 0.0) {
        return;
    }

    float coefficient_1[SWEENY_FILTERS], coefficient_2[SWEENY_FILTERS], coefficient_3[SWEENY_FILTERS],
        damping[SWEENY_FILTERS];
    int mode[SWEENY_FILTERS];
    for (int filter_index = 0; filter_index < SWEENY_FILTERS; filter_index++) {
        double cut = SHARP_SWEENY_CUTOFF_MIN * pow(SHARP_SWEENY_CUTOFF_MAX / SHARP_SWEENY_CUTOFF_MIN,
                                                   (double)chip->filter[filter_index].r_cutoff / 65535.0);
        if (cut > rate * 0.49) {
            cut = rate * 0.49;
        }

        double warped_cutoff = tan(3.14159265358979 * cut / rate);

        double damping_value = 2.0 - 1.95 * ((double)chip->filter[filter_index].r_resonance / 255.0);
        if (damping_value < 0.05) {
            damping_value = 0.05;
        }

        double denominator_inverse = 1.0 / (1.0 + warped_cutoff * (warped_cutoff + damping_value));
        coefficient_1[filter_index] = (float)denominator_inverse;
        coefficient_2[filter_index] = (float)(warped_cutoff * denominator_inverse);
        coefficient_3[filter_index] = (float)(warped_cutoff * warped_cutoff * denominator_inverse);
        damping[filter_index] = (float)damping_value;
        mode[filter_index] = (int)chip->filter[filter_index].r_mode;
    }
    float master_volume = (float)chip->r_master_volume / 255.0f;

    for (uint32_t frame_index = 0; frame_index < frames; frame_index++) {
        float dry_left = 0.0f, dry_right = 0.0f;
        float filter_inputs[SWEENY_FILTERS][2] = {{0.0f}};

        for (int voice_index = 0; voice_index < SWEENY_VOICES; voice_index++) {
            SweenyVoice* voice = &chip->voices[voice_index];
            if (!voice->active) {
                voice->wrapped = 0;
                continue;
            }

            SweenyVoice* neighbor = &chip->voices[(voice_index + SWEENY_VOICES - 1) % SWEENY_VOICES];

            voice->wrapped = 0;
            if (voice->r_control & SWEENY_CTL_TEST) {
                voice->phase = 0.0;
            } else {
                voice->phase += voice->step;
                if (voice->phase >= 1.0) {
                    voice->phase -= floor(voice->phase);
                    voice->wrapped = 1;
                }
            }

            if ((voice->r_control & SWEENY_CTL_SYNC) && neighbor->wrapped) {
                voice->phase = 0.0;
                voice->wrapped = 1;
            }

            const int bandlimited = (voice->r_control & SWEENY_CTL_BANDLIMIT) != 0;
            const double phase = voice->phase, phase_step = voice->step;
            float signal;

            switch (voice->r_waveform) {
            case SWEENY_WAVE_SAW:
                signal = (float)(2.0 * phase - 1.0);
                if (bandlimited) {
                    signal -= sharp_sweeny_polyblep(phase, phase_step);
                }
                break;

            case SWEENY_WAVE_PULSE: {
                double pulse_width = (double)voice->r_pulse_width / 65536.0;
                signal = (phase < pulse_width) ? 1.0f : -1.0f;
                if (bandlimited) {
                    double shifted_phase = phase - pulse_width;
                    if (shifted_phase < 0.0) {
                        shifted_phase += 1.0;
                    }
                    signal += sharp_sweeny_polyblep(phase, phase_step);
                    signal -= sharp_sweeny_polyblep(shifted_phase, phase_step);
                }
                break;
            }

            case SWEENY_WAVE_NOISE:

                voice->noise_acc += voice->step * 16.0;
                while (voice->noise_acc >= 1.0) {
                    voice->noise_acc -= 1.0;
                    sharp_sweeny_clock_noise(voice);
                }
                signal = voice->noise_out;
                break;

            default:
                signal = (phase < 0.5) ? (float)(4.0 * phase - 1.0) : (float)(3.0 - 4.0 * phase);

                if (voice->r_control & SWEENY_CTL_RING) {
                    signal *= (neighbor->phase < 0.5) ? 1.0f : -1.0f;
                }
                break;
            }

            float envelope_level = sharp_sweeny_step_envelope(voice);
            if (voice->env_stage == SWEENY_ENV_IDLE) {
                voice->active = 0;
                continue;
            }

            signal *= envelope_level;
            voice->last_out = signal;

            int filter_route = (int)voice->r_filter_route;
            if (filter_route > 0 && filter_route <= SWEENY_FILTERS) {
                filter_inputs[filter_route - 1][0] += signal * voice->lg;
                filter_inputs[filter_route - 1][1] += signal * voice->rg;
            } else {
                dry_left += signal * voice->lg;
                dry_right += signal * voice->rg;
            }
        }

        float output_left = dry_left, output_right = dry_right;
        for (int filter_index = 0; filter_index < SWEENY_FILTERS; filter_index++) {
            for (int channel_index = 0; channel_index < 2; channel_index++) {
                float input = filter_inputs[filter_index][channel_index];
                float integrator_1 = chip->filter[filter_index].ic1[channel_index],
                      integrator_2 = chip->filter[filter_index].ic2[channel_index];

                float input_difference = input - integrator_2;
                float bandpass = coefficient_1[filter_index] * integrator_1 +
                                 coefficient_2[filter_index] * input_difference;
                float lowpass = integrator_2 + coefficient_2[filter_index] * integrator_1 +
                                coefficient_3[filter_index] * input_difference;
                chip->filter[filter_index].ic1[channel_index] = 2.0f * bandpass - integrator_1;
                chip->filter[filter_index].ic2[channel_index] = 2.0f * lowpass - integrator_2;

                float filtered_output = 0.0f;
                if (mode[filter_index] & SWEENY_FILT_LP) {
                    filtered_output += lowpass;
                }
                if (mode[filter_index] & SWEENY_FILT_BP) {
                    filtered_output += bandpass;
                }
                if (mode[filter_index] & SWEENY_FILT_HP) {
                    filtered_output += input - damping[filter_index] * bandpass - lowpass;
                }
                if (channel_index) {
                    output_right += filtered_output;
                } else {
                    output_left += filtered_output;
                }
            }
        }

        output[(size_t)frame_index * 2 + 0] += output_left * master_volume;
        output[(size_t)frame_index * 2 + 1] += output_right * master_volume;
    }

    return;
}
