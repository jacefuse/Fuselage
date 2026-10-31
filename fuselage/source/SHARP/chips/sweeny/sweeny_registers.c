// Sweeny registers. SEE: (SHARP.md, Sweeny synthesis)
#include "sweeny_private.h"
#include <math.h>
#include <string.h>

void WriteSweenyRegister(Sweeny* chip, uint16_t reg, uint16_t value) {

    if (reg >= SWEENY_REG_GLOBAL) {
        if (reg == SWEENY_REG_MASTER_VOLUME) {
            chip->r_master_volume = value > 255 ? 255 : value;
            return;
        }

        unsigned global_offset = (unsigned)(reg - SWEENY_REG_GLOBAL);
        unsigned filter_index = global_offset / SWEENY_FILTER_STRIDE;
        unsigned filter_field = global_offset % SWEENY_FILTER_STRIDE;

        if (filter_index >= SWEENY_FILTERS) {
            return;
        }

        switch (filter_field) {
        case SWEENY_FREG_CUTOFF:
            chip->filter[filter_index].r_cutoff = value;
            break;
        case SWEENY_FREG_RES:
            chip->filter[filter_index].r_resonance = value > 255 ? 255 : value;
            break;
        case SWEENY_FREG_MODE:
            chip->filter[filter_index].r_mode = value & 0x0007u;
            break;
        default:
            break;
        }
        return;
    }

    unsigned voice_index = (unsigned)(reg / SWEENY_REG_STRIDE);
    unsigned register_field = (unsigned)(reg % SWEENY_REG_STRIDE);
    if (voice_index >= SWEENY_VOICES) {
        return;
    }
    SweenyVoice* voice = &chip->voices[voice_index];

    switch (register_field) {
    case SWEENY_REG_FREQ_HI:
        voice->r_freq = (voice->r_freq & 0x0000FFFFu) | ((uint32_t)value << 16);
        sharp_sweeny_refresh_step(chip, voice);
        break;
    case SWEENY_REG_FREQ_LO:
        voice->r_freq = (voice->r_freq & 0xFFFF0000u) | value;
        sharp_sweeny_refresh_step(chip, voice);
        break;
    case SWEENY_REG_PULSE_WIDTH:
        voice->r_pulse_width = value;
        break;
    case SWEENY_REG_WAVEFORM:
        voice->r_waveform = (value > SWEENY_WAVE_NOISE) ? SWEENY_WAVE_TRIANGLE : value;
        break;
    case SWEENY_REG_VOLUME:
        voice->r_volume = value > 255 ? 255 : value;
        sharp_sweeny_set_voice_gains(voice);
        break;
    case SWEENY_REG_PAN:
        voice->r_pan = value > 255 ? 255 : value;
        sharp_sweeny_set_voice_gains(voice);
        break;
    case SWEENY_REG_ENV_AD:
        voice->r_env_ad = value;
        sharp_sweeny_configure_envelope(chip, voice);
        break;
    case SWEENY_REG_ENV_SR:
        voice->r_env_sr = value;
        sharp_sweeny_configure_envelope(chip, voice);
        break;
    case SWEENY_REG_CONTROL:
        voice->r_control = value;
        break;
    case SWEENY_REG_FILTER_ROUTE:
        voice->r_filter_route = value;
        break;
    case SWEENY_REG_GATE:
        if (value) {
            if (!isfinite(chip->device_rate) || chip->device_rate <= 0.0) {
                return;
            }

            sharp_sweeny_configure_envelope(chip, voice);
            voice->env_level = 0.0f;
            voice->env_stage = SWEENY_ENV_ATTACK;
            voice->active = 1;
            sharp_sweeny_set_voice_gains(voice);
            sharp_sweeny_refresh_step(chip, voice);
        } else if (voice->active) {
            voice->env_stage = SWEENY_ENV_RELEASE;
        }
        break;
    default:
        break;
    }

    return;
}
