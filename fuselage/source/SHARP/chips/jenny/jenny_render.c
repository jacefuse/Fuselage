#include "jenny_private.h"
#include <math.h>

#define SHARP_JENNY_PI 3.14159265358979323846
#define SHARP_JENNY_EG_TICK_HZ (53267.0 / 3.0)
#define SHARP_JENNY_EG_DB_LSB (96.0 / 1024.0)
#define SHARP_JENNY_DT_HZ (53267.0 / 1048576.0)
#define SHARP_JENNY_PM_DEPTH 2.0

static const uint8_t sharp_jenny_detune_table[4][32] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 8, 8},
    {1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 9, 10, 11, 12, 13, 14, 16, 16, 16, 16},
    {2, 2, 2, 2,  2,  3,  3,  3,  4,  4,  4,  5,  5,  6,  6,  7,
     8, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 20, 22, 22, 22, 22},
};

typedef struct {
    uint8_t mod[4];
    uint8_t car;
} SharpJennyAlgorithm;

static const SharpJennyAlgorithm sharp_jenny_algorithms[8] = {
    {{0, 1 << 0, 1 << 1, 1 << 2}, 1 << 3},
    {{0, 0, (1 << 0) | (1 << 1), 1 << 2}, 1 << 3},
    {{0, 0, 1 << 1, (1 << 0) | (1 << 2)}, 1 << 3},
    {{0, 1 << 0, 0, (1 << 1) | (1 << 2)}, 1 << 3},
    {{0, 1 << 0, 0, 1 << 2}, (1 << 1) | (1 << 3)},
    {{0, 1 << 0, 1 << 0, 1 << 0}, (1 << 1) | (1 << 2) | (1 << 3)},
    {{0, 1 << 0, 0, 0}, (1 << 1) | (1 << 2) | (1 << 3)},
    {{0, 0, 0, 0}, 0x0F},
};

/* Continuous-rate approximations retain ENTERON's arithmetic.
 * SEE: (SHARP.md, Jenny envelope and modulation) */
static double sharp_jenny_get_envelope_increments(int effective_rate) {
    if (effective_rate <= 0) {
        return 0.0;
    }
    if (effective_rate >= 60) {
        return 8.0;
    }
    const int pattern_sum = 4 + (effective_rate & 3);
    const int shift = 11 - (effective_rate >> 2);
    if (shift <= 0) {
        return (double)pattern_sum / 8.0 * (double)(1 << -shift);
    }
    return (double)pattern_sum / 8.0 / (double)(1 << shift);
}

static int sharp_jenny_get_keycode(double frequency) {
    if (frequency < 10.0) {
        return 0;
    }
    int keycode = (int)floor(4.0 * log2(frequency)) - 13;
    if (keycode < 0) {
        keycode = 0;
    }
    if (keycode > 31) {
        keycode = 31;
    }
    return keycode;
}

static int sharp_jenny_get_effective_rate(int register_rate, int rate_scaling, int keycode) {
    if (register_rate <= 0) {
        return 0;
    }
    int scaled_rate = 2 * register_rate + (keycode >> (3 - rate_scaling));
    return scaled_rate > 63 ? 63 : scaled_rate;
}

static double sharp_jenny_get_decay_increment(int effective_rate, double rate) {
    return sharp_jenny_get_envelope_increments(effective_rate) * SHARP_JENNY_EG_TICK_HZ *
           SHARP_JENNY_EG_DB_LSB / rate;
}

static double sharp_jenny_get_attack_coefficient(int effective_rate, double rate) {
    return sharp_jenny_get_envelope_increments(effective_rate) * SHARP_JENNY_EG_TICK_HZ / 16.0 / rate;
}

static double sharp_jenny_get_sustain_attenuation(int sustain_level) {
    return sustain_level >= 15 ? 93.0 : 3.0 * (double)sustain_level;
}

static double sharp_jenny_get_waveform_sample(int waveform, double phase) {
    phase -= floor(phase);

    double sine_sample = sin(2.0 * SHARP_JENNY_PI * phase);

    switch (waveform & 7) {
    default:
    case JENNY_WS_SINE:
        return sine_sample;
    case JENNY_WS_HALF:
        return phase < 0.5 ? sine_sample : 0.0;
    case JENNY_WS_ABS:
        return fabs(sine_sample);
    case JENNY_WS_PULSE: {
        double half_cycle_phase = phase - floor(phase * 2.0) * 0.5;
        return half_cycle_phase < 0.25 ? sin(2.0 * SHARP_JENNY_PI * half_cycle_phase) : 0.0;
    }
    case JENNY_WS_EVEN:
        return phase < 0.5 ? sin(4.0 * SHARP_JENNY_PI * phase) : 0.0;
    case JENNY_WS_ABS_EVEN:
        return phase < 0.5 ? fabs(sin(4.0 * SHARP_JENNY_PI * phase)) : 0.0;
    case JENNY_WS_SQUARE:
        return sine_sample >= 0.0 ? 1.0 : -1.0;
    case JENNY_WS_LOGSAW: {
        double double_phase = phase * 2.0;
        double fraction = double_phase - floor(double_phase);
        double exponential = exp(-6.0 * fraction);
        return double_phase < 1.0 ? exponential : -exponential;
    }
    }
}

static double sharp_jenny_get_feedback_depth(int feedback) {
    return feedback <= 0 ? 0.0 : (double)(1 << (feedback - 1)) / 64.0;
}

/* Zero input retains the positive ladder offset. SEE: (SHARP.md, Jenny output) */
static double sharp_jenny_apply_dac_ladder(double sample) {
    if (sample > 2.0) {
        sample = 2.0;
    }
    if (sample < -2.0) {
        sample = -2.0;
    }
    sample = floor(sample * 128.0) / 128.0;
    if (sample >= 0.0) {
        sample += 1.0 / 128.0;
    }
    return sample;
}

static void sharp_jenny_advance_envelope(JennyVoice* voice, JennyOp* operator_state, int keycode,
                                         double rate) {
    int rate_scaling = (operator_state->l_tl_rs >> 8) & 3;

    switch (operator_state->env_stage) {
    case JENNY_ENV_DAMP: {
        operator_state->env_att += SHARP_JENNY_EG_SILENT * 1000.0 / rate;
        if (operator_state->env_att >= SHARP_JENNY_EG_SILENT) {
            operator_state->env_att = SHARP_JENNY_EG_SILENT;
            operator_state->out = operator_state->out_prev = 0.0;
            if (operator_state->pending) {
                operator_state->pending = 0;
                operator_state->phase = 0.0;
                sharp_jenny_commit_operator(operator_state);
                if (voice->pendingVoice) {
                    sharp_jenny_commit_voice(voice);
                    voice->pendingVoice = 0;
                }
                operator_state->env_stage = JENNY_ENV_ATTACK;
            } else {
                operator_state->env_stage = JENNY_ENV_IDLE;
            }
        }
        break;
    }
    case JENNY_ENV_ATTACK: {
        int effective_rate =
            sharp_jenny_get_effective_rate((operator_state->l_ar_d1r >> 8) & 31, rate_scaling, keycode);

        /* The fast-attack coefficient is per sample, not normalized by rate. */
        const double coefficient =
            (effective_rate >= 62) ? 0.35 : sharp_jenny_get_attack_coefficient(effective_rate, rate);
        operator_state->env_att -= (operator_state->env_att + SHARP_JENNY_EG_DB_LSB) * coefficient;
        if (operator_state->env_att <= 0.0) {
            operator_state->env_att = 0.0;
            operator_state->env_stage = JENNY_ENV_DECAY1;
        }
        break;
    }
    case JENNY_ENV_DECAY1: {
        const double sustain_attenuation =
            sharp_jenny_get_sustain_attenuation((operator_state->l_sl_d2r >> 8) & 15);
        int effective_rate =
            sharp_jenny_get_effective_rate((operator_state->l_ar_d1r) & 31, rate_scaling, keycode);
        operator_state->env_att += sharp_jenny_get_decay_increment(effective_rate, rate);
        if (operator_state->env_att >= sustain_attenuation) {
            operator_state->env_att = sustain_attenuation;
            operator_state->env_stage = JENNY_ENV_DECAY2;
        }
        break;
    }
    case JENNY_ENV_DECAY2: {
        int effective_rate =
            sharp_jenny_get_effective_rate((operator_state->l_sl_d2r) & 31, rate_scaling, keycode);
        operator_state->env_att += sharp_jenny_get_decay_increment(effective_rate, rate);
        if (operator_state->env_att >= SHARP_JENNY_EG_SILENT) {
            operator_state->env_att = SHARP_JENNY_EG_SILENT;
            operator_state->env_stage = JENNY_ENV_IDLE;
        }
        break;
    }
    case JENNY_ENV_RELEASE: {
        int effective_rate = sharp_jenny_get_effective_rate((((operator_state->l_rr_ws >> 8) & 15) << 1) | 1,
                                                            rate_scaling, keycode);
        operator_state->env_att += sharp_jenny_get_decay_increment(effective_rate, rate);
        if (operator_state->env_att >= SHARP_JENNY_EG_SILENT) {
            operator_state->env_att = SHARP_JENNY_EG_SILENT;
            operator_state->env_stage = JENNY_ENV_IDLE;
        }
        break;
    }
    default:
        break;
    }
}

static void sharp_jenny_advance_lfos(Jenny* chip, uint32_t frames) {
    chip->lfoVib += (double)frames * 6.1 / chip->rate;
    chip->lfoVib -= floor(chip->lfoVib);
    chip->lfoTrem += (double)frames * 3.7 / chip->rate;
    chip->lfoTrem -= floor(chip->lfoTrem);
}

void RenderJenny(void* context, float* output, uint32_t frames) {
    Jenny* chip = (Jenny*)context;
    if (!isfinite(chip->rate) || chip->rate <= 0.0) {
        return;
    }

    for (int voice_index = 0; voice_index < JENNY_VOICES; voice_index++) {
        JennyVoice* voice = &chip->voice[voice_index];

        int alive = 0;
        for (int operator_index = 0; operator_index < 4; operator_index++) {
            if (voice->op[operator_index].env_stage != JENNY_ENV_IDLE) {
                alive = 1;
                break;
            }
        }
        if (!alive) {
            continue;
        }

        const SharpJennyAlgorithm* algorithm = &sharp_jenny_algorithms[voice->l_alg & 7];
        double frequency = (double)voice->l_freq / 65536.0;

        int keycode = sharp_jenny_get_keycode(frequency);
        double voice_gain = (double)voice->l_volume / 255.0;

        double pan_angle = (double)voice->l_pan * (3.14159265358979323846 / 2.0) / 255.0;
        double pan_left = cos(pan_angle);
        double pan_right = sin(pan_angle);
        double feedback_depth = sharp_jenny_get_feedback_depth(voice->l_feedback);

        for (uint32_t frame_index = 0; frame_index < frames; frame_index++) {
            double carrier_sum = 0.0;

            for (int operator_index = 0; operator_index < 4; operator_index++) {
                JennyOp* operator_state = &voice->op[operator_index];

                int multiple = (operator_state->l_mult_dt >> 8) & 15;
                int detune = operator_state->l_mult_dt & 7;
                double frequency_multiple = multiple == 0 ? 0.5 : (double)multiple;

                double detune_hz = (double)sharp_jenny_detune_table[detune & 3][keycode] * SHARP_JENNY_DT_HZ;
                if (detune & 4) {
                    detune_hz = -detune_hz;
                }
                double operator_frequency = (frequency + detune_hz) * frequency_multiple;
                if (operator_frequency < 0.0) {
                    operator_frequency = 0.0;
                }

                if (operator_state->l_rr_ws & 0x08) {
                    double vibrato =
                        sin(2.0 * SHARP_JENNY_PI * (chip->lfoVib + (double)frame_index * 6.1 / chip->rate));
                    operator_frequency *= 1.0 + 0.00405 * vibrato;
                }

                double modulation = 0.0;
                for (int source_index = 0; source_index < operator_index; source_index++) {
                    if (algorithm->mod[operator_index] & (1 << source_index)) {
                        modulation += voice->op[source_index].out;
                    }
                }
                if (operator_index == 0 && feedback_depth > 0.0) {
                    modulation += (operator_state->out + operator_state->out_prev) * 0.5 * feedback_depth /
                                  SHARP_JENNY_PM_DEPTH;
                }

                sharp_jenny_advance_envelope(voice, operator_state, keycode, chip->rate);

                double attenuation = operator_state->env_att;
                double amplitude =
                    (attenuation >= SHARP_JENNY_EG_SILENT)
                        ? 0.0
                        : pow(10.0, -(attenuation + 0.75 * (double)(operator_state->l_tl_rs & 127)) / 20.0);
                if (operator_state->l_rr_ws & 0x10) {
                    double tremolo =
                        sin(2.0 * SHARP_JENNY_PI * (chip->lfoTrem + (double)frame_index * 3.7 / chip->rate));
                    amplitude *= 1.0 - 0.109 * (0.5 + 0.5 * tremolo);
                }
                double operator_sample = sharp_jenny_get_waveform_sample(
                                             operator_state->l_rr_ws & 7,
                                             operator_state->phase + modulation * SHARP_JENNY_PM_DEPTH) *
                                         amplitude;

                operator_state->out_prev = operator_state->out;
                operator_state->out = operator_sample;

                operator_state->phase += operator_frequency / chip->rate;
                operator_state->phase -= floor(operator_state->phase);

                if (algorithm->car & (1 << operator_index)) {
                    carrier_sum += operator_sample;
                }
            }

            double voice_sample = sharp_jenny_apply_dac_ladder(carrier_sum * voice_gain) * 0.5;
            output[(size_t)frame_index * 2] += (float)(voice_sample * pan_left);
            output[(size_t)frame_index * 2 + 1] += (float)(voice_sample * pan_right);
        }
    }

    sharp_jenny_advance_lfos(chip, frames);
}
