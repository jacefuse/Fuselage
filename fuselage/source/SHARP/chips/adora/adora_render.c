// Adora render. SEE: (SHARP.md, Adora sample playback)
#include "adora_private.h"
#include <math.h>
#include <string.h>

static inline float sharp_adora_step_envelope(AdoraVoice* voice) {
    switch (voice->env_stage) {
    case ADORA_ENV_ATTACK:
        voice->env_level += voice->env_a_inc;
        if (voice->env_level >= 1.0f) {
            voice->env_level = 1.0f;
            voice->env_stage = ADORA_ENV_DECAY;
        }
        break;
    case ADORA_ENV_DECAY:
        voice->env_level -= voice->env_d_inc;
        if (voice->env_level <= voice->env_sustain) {
            voice->env_level = voice->env_sustain;
            voice->env_stage = ADORA_ENV_SUSTAIN;
        }
        break;
    case ADORA_ENV_SUSTAIN:
        voice->env_level = voice->env_sustain;
        break;
    case ADORA_ENV_RELEASE:
        voice->env_level -= voice->env_r_inc;
        if (voice->env_level <= 0.0f) {
            voice->env_level = 0.0f;
            voice->env_stage = ADORA_ENV_IDLE;
        }
        break;
    default:
        voice->env_level = 0.0f;
        break;
    }
    return voice->env_level;
}

static inline float sharp_adora_decode(const AdoraVoice* voice, int64_t sample_index) {
    const uint8_t* sample = voice->sample;
    switch (voice->fmt) {
    case ADORA_FMT_S8:
        return (float)((int32_t)sample[sample_index] - (sample[sample_index] >= 128 ? 256 : 0)) / 128.0f;
    case ADORA_FMT_S16LE: {
        size_t byte_index = (size_t)sample_index * 2;
        uint16_t encoded_value =
            (uint16_t)((uint16_t)sample[byte_index] | ((uint16_t)sample[byte_index + 1] << 8));
        int32_t signed_value = (int32_t)encoded_value - (encoded_value >= 32768 ? 65536 : 0);
        return (float)signed_value / 32768.0f;
    }
    case ADORA_FMT_S16BE: {
        size_t byte_index = (size_t)sample_index * 2;
        uint16_t encoded_value =
            (uint16_t)(((uint16_t)sample[byte_index] << 8) | (uint16_t)sample[byte_index + 1]);
        int32_t signed_value = (int32_t)encoded_value - (encoded_value >= 32768 ? 65536 : 0);
        return (float)signed_value / 32768.0f;
    }
    default:
        return ((float)sample[sample_index] - 128.0f) / 128.0f;
    }
}

static inline float sharp_adora_tap(const AdoraVoice* voice, int64_t sample_index) {
    if (voice->in_loop && voice->loop_len >= 2) {
        int64_t loop_start = voice->loop_start;
        int64_t sample_length = voice->loop_len;
        if (voice->loop_mode == ADORA_LOOP_PINGPONG) {
            int64_t period = sample_length * 2;
            int64_t wrapped_index = ((sample_index - loop_start) % period + period) % period;
            if (wrapped_index >= sample_length) {
                wrapped_index = period - 1 - wrapped_index;
            }
            sample_index = loop_start + wrapped_index;
        } else {
            sample_index =
                loop_start + (((sample_index - loop_start) % sample_length) + sample_length) % sample_length;
        }
    } else {
        if (sample_index < 0) {
            sample_index = 0;
        }
        if ((uint64_t)sample_index >= voice->nsamp) {
            sample_index = (int64_t)voice->nsamp - 1;
        }
    }
    return sharp_adora_decode(voice, sample_index);
}

static inline float sharp_adora_hermite(float previous, float current, float next, float following,
                                        float fraction) {
    float constant = current;
    float linear = 0.5f * (next - previous);
    float quadratic = previous - 2.5f * current + 2.0f * next - 0.5f * following;
    float cubic = 0.5f * (following - previous) + 1.5f * (current - next);
    return ((cubic * fraction + quadratic) * fraction + linear) * fraction + constant;
}

void RenderAdora(Adora* chip, float* output, uint32_t frames) {
    for (int sample_index = 0; sample_index < ADORA_VOICES; sample_index++) {
        AdoraVoice* voice = &chip->voices[sample_index];
        if (!voice->active) {
            continue;
        }
        for (uint32_t frame_index = 0; frame_index < frames; frame_index++) {

            voice->cur_lg += (voice->lg - voice->cur_lg) * chip->gain_k;
            voice->cur_rg += (voice->rg - voice->cur_rg) * chip->gain_k;

            if (voice->fading) {
                float fade_weight = (float)voice->fading / (float)ADORA_DECLICK;
                float sample = voice->fade_level * fade_weight;
                voice->last_l = sample * voice->cur_lg;
                voice->last_r = sample * voice->cur_rg;
                output[(size_t)frame_index * 2 + 0] += voice->last_l;
                output[(size_t)frame_index * 2 + 1] += voice->last_r;
                voice->last_out = sample;
                if (--voice->fading == 0) {
                    voice->active = 0;
                    voice->last_l = voice->last_r = 0.0f;
                    break;
                }
                continue;
            }

            double loop_end = (double)voice->loop_start + voice->loop_len;

            if (voice->loop_mode == ADORA_LOOP_FORWARD) {
                while (voice->pos >= loop_end) {
                    voice->pos -= (double)voice->loop_len;
                    voice->in_loop = 1;
                }
            } else if (voice->loop_mode == ADORA_LOOP_PINGPONG) {
                for (;;) {
                    if (!voice->reverse && voice->pos >= loop_end) {
                        voice->pos = 2.0 * loop_end - voice->pos;
                        voice->reverse = 1;
                        voice->in_loop = 1;
                    } else if (voice->reverse && voice->pos < (double)voice->loop_start) {
                        voice->pos = 2.0 * (double)voice->loop_start - voice->pos;
                        voice->reverse = 0;
                    } else {
                        break;
                    }
                }
            } else if (voice->pos >= (double)voice->nsamp || voice->pos < 0.0) {

                voice->fading = ADORA_DECLICK;
                voice->fade_level = voice->last_out;
                frame_index--; // Retry this output frame through the fade branch.
                continue;
            }

            int64_t sample_index = (int64_t)voice->pos;
            float fraction = (float)(voice->pos - (double)sample_index);
            float sample = sharp_adora_hermite(
                sharp_adora_tap(voice, sample_index - 1), sharp_adora_tap(voice, sample_index),
                sharp_adora_tap(voice, sample_index + 1), sharp_adora_tap(voice, sample_index + 2), fraction);

            float crossfade_weight = 0.0f;
            if (voice->dc_n > 0) {
                crossfade_weight = (float)voice->dc_n / (float)ADORA_DECLICK;
                voice->dc_n--;
            }

            float envelope_level = sharp_adora_step_envelope(voice);
            if (voice->env_stage == ADORA_ENV_IDLE) {
                voice->fading = ADORA_DECLICK;
                voice->fade_level = voice->last_out;
                frame_index--; // Retry this output frame through the fade branch.
                continue;
            }
            sample *= envelope_level;
            voice->last_out = sample;
            float output_left = sample * voice->cur_lg, output_right = sample * voice->cur_rg;
            if (crossfade_weight > 0.0f) {

                output_left = output_left * (1.0f - crossfade_weight) + voice->dc_l * crossfade_weight;
                output_right = output_right * (1.0f - crossfade_weight) + voice->dc_r * crossfade_weight;
            }
            voice->last_l = output_left;
            voice->last_r = output_right;
            output[(size_t)frame_index * 2 + 0] += output_left;
            output[(size_t)frame_index * 2 + 1] += output_right;
            voice->pos += (voice->reverse ? -voice->step : voice->step);
        }
    }
}
