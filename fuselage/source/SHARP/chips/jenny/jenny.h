#ifndef JENNY_H
#define JENNY_H
#include <stdint.h>
#include "../../backend/sharp_chip.h"

/* Eighteen four-operator FM voices. SEE: (SHARP.md, Jenny registers and synthesis)
 * Exposed state fields retain their layout; use register writes for controls. */

#define JENNY_VOICES 18

#define JENNY_REG_FREQ_HI 0
#define JENNY_REG_FREQ_LO 1
#define JENNY_REG_ALG 2
#define JENNY_REG_FEEDBACK 3
#define JENNY_REG_PAN 4
#define JENNY_REG_VOLUME 5
#define JENNY_REG_KEY 6

#define JENNY_OP_MULT_DT 0
#define JENNY_OP_TL_RS 1
#define JENNY_OP_AR_D1R 2
#define JENNY_OP_SL_D2R 3
#define JENNY_OP_RR_WS 4

#define JENNY_REG_STRIDE 32
#define JENNY_REG(voice, field) ((uint16_t)((voice) * JENNY_REG_STRIDE + (field)))
#define JENNY_OP_REG(voice, op, f) ((uint16_t)((voice) * JENNY_REG_STRIDE + 8 + (op) * 5 + (f)))

#define JENNY_ALG_SERIAL 0
#define JENNY_ALG_TWO_PAIRS 4
#define JENNY_ALG_FANOUT 5
#define JENNY_ALG_ORGAN 7

#define JENNY_WS_SINE 0
#define JENNY_WS_HALF 1
#define JENNY_WS_ABS 2
#define JENNY_WS_PULSE 3
#define JENNY_WS_EVEN 4
#define JENNY_WS_ABS_EVEN 5
#define JENNY_WS_SQUARE 6
#define JENNY_WS_LOGSAW 7

typedef struct {

    uint16_t r_mult_dt, r_tl_rs, r_ar_d1r, r_sl_d2r, r_rr_ws;

    uint16_t l_mult_dt, l_tl_rs, l_ar_d1r, l_sl_d2r, l_rr_ws;

    int env_stage;
    int pending;
    double env_att;
    double out, out_prev;
    double phase;
} JennyOp;

typedef struct {
    uint32_t r_freq;
    uint16_t r_alg, r_feedback, r_pan, r_volume;

    uint32_t l_freq;
    uint16_t l_alg, l_feedback, l_pan, l_volume;
    int pendingVoice;
    int keyed;
    JennyOp op[4];
} JennyVoice;

typedef struct {
    double rate;

    double lfoVib, lfoTrem;
    JennyVoice voice[JENNY_VOICES];
} Jenny;

#define JENNY_ENV_IDLE 0
#define JENNY_ENV_ATTACK 1
#define JENNY_ENV_DECAY1 2
#define JENNY_ENV_DECAY2 3
#define JENNY_ENV_RELEASE 4
#define JENNY_ENV_DAMP 5

/* Initialize silent caller-owned state at a finite positive audio rate. */
void InitJenny(Jenny* chip, double device_rate);
/* Callback-compatible register writer. KEY is edge-driven through key-off.
 * SEE: (SHARP.md, Jenny writes and key lifecycle) */
void WriteJennyRegister(void* context, uint16_t reg, uint16_t value);
/* Add interleaved stereo frames to caller-owned output; context is Jenny*. */
void RenderJenny(void* context, float* output, uint32_t frames);
/* Returned callbacks refer to the same caller-owned chip. */
SharpChip GetJennySource(Jenny* chip);

#endif
