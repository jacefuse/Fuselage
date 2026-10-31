// Adora registers. SEE: (SHARP.md, Adora sample playback)
#include "adora_private.h"
#include <math.h>
#include <string.h>

void WriteAdoraRegister(Adora* chip, uint16_t reg, uint16_t value) {
    unsigned voice_index = (unsigned)(reg / ADORA_REG_STRIDE);
    unsigned register_field = (unsigned)(reg % ADORA_REG_STRIDE);
    if (voice_index >= ADORA_VOICES) {
        return;
    }

    AdoraVoice* voice = &chip->voices[voice_index];

    switch (register_field) {
    case ADORA_REG_OFFSET_HI:
        voice->r_offset = (voice->r_offset & 0x0000FFFFu) | ((uint32_t)value << 16);
        break;
    case ADORA_REG_OFFSET_LO:
        voice->r_offset = (voice->r_offset & 0xFFFF0000u) | value;
        break;
    case ADORA_REG_LENGTH_HI:
        voice->r_length = (voice->r_length & 0x0000FFFFu) | ((uint32_t)value << 16);
        break;
    case ADORA_REG_LENGTH_LO:
        voice->r_length = (voice->r_length & 0xFFFF0000u) | value;
        break;
    case ADORA_REG_PITCH:
        voice->r_pitch = value;
        if (voice->active) {
            sharp_adora_update_step(chip, voice);
        }
        break;

    case ADORA_REG_BASE_RATE:
        voice->r_base_rate = value;
        if (voice->active) {
            sharp_adora_update_step(chip, voice);
        }
        break;
    case ADORA_REG_VOLUME:
        voice->r_volume = value > 255 ? 255 : value;
        if (voice->active) {
            sharp_adora_set_voice_gains(voice);
        }
        break;
    case ADORA_REG_PAN:
        voice->r_pan = value > 255 ? 255 : value;
        if (voice->active) {
            sharp_adora_set_voice_gains(voice);
        }
        break;
    case ADORA_REG_TRIGGER:

        if (value) {
            sharp_adora_trigger_voice(chip, voice);
        } else if (voice->active) {
            voice->env_stage = ADORA_ENV_RELEASE;
        }
        break;
    case ADORA_REG_ENV_AD:
        voice->r_env_ad = value;
        if (voice->active) {
            sharp_adora_configure_envelope(chip, voice);
        }
        break;
    case ADORA_REG_ENV_SR:
        voice->r_env_sr = value;
        if (voice->active) {
            sharp_adora_configure_envelope(chip, voice);
        }
        break;
    case ADORA_REG_LOOP_START_HI:
        voice->r_loop_start = (voice->r_loop_start & 0x0000FFFFu) | ((uint32_t)value << 16);
        break;
    case ADORA_REG_LOOP_START_LO:
        voice->r_loop_start = (voice->r_loop_start & 0xFFFF0000u) | value;
        break;
    case ADORA_REG_LOOP_LEN_HI:
        voice->r_loop_len = (voice->r_loop_len & 0x0000FFFFu) | ((uint32_t)value << 16);
        break;
    case ADORA_REG_LOOP_LEN_LO:
        voice->r_loop_len = (voice->r_loop_len & 0xFFFF0000u) | value;
        break;
    case ADORA_REG_FORMAT:
        voice->r_format = (value > ADORA_FMT_S16BE) ? ADORA_FMT_U8 : value;
        break;
    case ADORA_REG_LOOP_MODE:
        voice->r_loop_mode = value;

        if (voice->active && value == ADORA_LOOP_OFF) {
            voice->loop_mode = ADORA_LOOP_OFF;
        }
        break;
    default:
        break;
    }
}
