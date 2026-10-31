#include "jenny_private.h"

void WriteJennyRegister(void* context, uint16_t reg, uint16_t value) {
    Jenny* chip = (Jenny*)context;
    int voice_index = reg / JENNY_REG_STRIDE;
    int register_field = reg % JENNY_REG_STRIDE;

    if (voice_index >= JENNY_VOICES) {
        return;
    }
    JennyVoice* voice = &chip->voice[voice_index];

    switch (register_field) {
#define SHARP_JENNY_LIVE (voice->keyed && !voice->pendingVoice)
    case JENNY_REG_FREQ_HI:
        voice->r_freq = (voice->r_freq & 0x0000FFFFu) | ((uint32_t)value << 16);
        if (SHARP_JENNY_LIVE) {
            voice->l_freq = voice->r_freq;
        }
        return;
    case JENNY_REG_FREQ_LO:
        voice->r_freq = (voice->r_freq & 0xFFFF0000u) | value;
        if (SHARP_JENNY_LIVE) {
            voice->l_freq = voice->r_freq;
        }
        return;
    case JENNY_REG_ALG:
        voice->r_alg = value & 7u;
        if (SHARP_JENNY_LIVE) {
            voice->l_alg = voice->r_alg;
        }
        return;
    case JENNY_REG_FEEDBACK:
        voice->r_feedback = value & 7u;
        if (SHARP_JENNY_LIVE) {
            voice->l_feedback = voice->r_feedback;
        }
        return;
    case JENNY_REG_PAN:
        voice->r_pan = value & 255u;
        if (SHARP_JENNY_LIVE) {
            voice->l_pan = voice->r_pan;
        }
        return;
    case JENNY_REG_VOLUME:
        voice->r_volume = value & 255u;
        if (SHARP_JENNY_LIVE) {
            voice->l_volume = voice->r_volume;
        }
        return;
#undef SHARP_JENNY_LIVE
    case JENNY_REG_KEY:
        sharp_jenny_write_key(voice, value);
        return;
    default:
        break;
    }

    /* Operator writes remain pending until a key/damp commit. */
    if (register_field >= 8 && register_field < 28) {
        JennyOp* operator_state = &voice->op[(register_field - 8) / 5];
        switch ((register_field - 8) % 5) {
        case JENNY_OP_MULT_DT:
            operator_state->r_mult_dt = value;
            return;
        case JENNY_OP_TL_RS:
            operator_state->r_tl_rs = value;
            return;
        case JENNY_OP_AR_D1R:
            operator_state->r_ar_d1r = value;
            return;
        case JENNY_OP_SL_D2R:
            operator_state->r_sl_d2r = value;
            return;
        case JENNY_OP_RR_WS:
            operator_state->r_rr_ws = value;
            return;
        }
    }
}
