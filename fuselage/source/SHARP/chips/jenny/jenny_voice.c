#include "jenny_private.h"

void sharp_jenny_commit_operator(JennyOp* operator_state) {
    operator_state->l_mult_dt = operator_state->r_mult_dt;
    operator_state->l_tl_rs = operator_state->r_tl_rs;
    operator_state->l_ar_d1r = operator_state->r_ar_d1r;
    operator_state->l_sl_d2r = operator_state->r_sl_d2r;
    operator_state->l_rr_ws = operator_state->r_rr_ws;
}

void sharp_jenny_commit_voice(JennyVoice* voice) {
    voice->l_freq = voice->r_freq;
    voice->l_alg = voice->r_alg;
    voice->l_feedback = voice->r_feedback;
    voice->l_pan = voice->r_pan;
    voice->l_volume = voice->r_volume;
}

void sharp_jenny_write_key(JennyVoice* voice, uint16_t value) {
    /* A new strike requires key-off, including resume strikes. */
    if (value == 2 && !voice->keyed) {
        sharp_jenny_commit_voice(voice);
        voice->pendingVoice = 0;
        for (int operator_index = 0; operator_index < 4; operator_index++) {
            JennyOp* operator_state = &voice->op[operator_index];
            sharp_jenny_commit_operator(operator_state);
            operator_state->pending = 0;
            if (operator_state->env_att >= 60.0) {
                operator_state->phase = 0.0;
                operator_state->out = 0.0;
                operator_state->out_prev = 0.0;
                operator_state->env_att = SHARP_JENNY_EG_SILENT;
            }
            operator_state->env_stage = JENNY_ENV_ATTACK;
        }
        voice->keyed = 1;
        return;
    }
    if (value != 0 && !voice->keyed) {
        int all_silent = 1;
        for (int operator_index = 0; operator_index < 4; operator_index++) {
            if (voice->op[operator_index].env_att < 60.0) {
                all_silent = 0;
                break;
            }
        }
        if (all_silent) {
            sharp_jenny_commit_voice(voice);
            voice->pendingVoice = 0;
        } else {
            voice->pendingVoice = 1;
        }
        for (int operator_index = 0; operator_index < 4; operator_index++) {
            JennyOp* operator_state = &voice->op[operator_index];
            if (operator_state->env_att >= 60.0) {
                operator_state->phase = 0.0;
                operator_state->out = 0.0;
                operator_state->out_prev = 0.0;
                operator_state->env_att = SHARP_JENNY_EG_SILENT;
                sharp_jenny_commit_operator(operator_state);
                operator_state->env_stage = JENNY_ENV_ATTACK;
            } else {
                operator_state->env_stage = JENNY_ENV_DAMP;
                operator_state->pending = 1;
            }
        }
        voice->keyed = 1;
    } else if (value == 0 && voice->keyed) {
        for (int operator_index = 0; operator_index < 4; operator_index++) {
            JennyOp* operator_state = &voice->op[operator_index];
            if (operator_state->env_stage == JENNY_ENV_DAMP) {
                operator_state->pending = 0;
            } else {
                operator_state->env_stage = JENNY_ENV_RELEASE;
            }
        }
        voice->keyed = 0;
    }
    return;
}
