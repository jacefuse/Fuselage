#include "jenny_private.h"
#include <string.h>

void InitJenny(Jenny* chip, double device_rate) {
    memset(chip, 0, sizeof *chip);
    chip->rate = device_rate;

    for (int voice_index = 0; voice_index < JENNY_VOICES; voice_index++) {
        JennyVoice* voice = &chip->voice[voice_index];
        voice->r_pan = 128;
        voice->r_volume = 255;
        voice->l_pan = 128;
        voice->l_volume = 255;
        for (int operator_index = 0; operator_index < 4; operator_index++) {
            voice->op[operator_index].r_mult_dt = 1 << 8;
            voice->op[operator_index].l_mult_dt = 1 << 8;
            voice->op[operator_index].env_stage = JENNY_ENV_IDLE;

            voice->op[operator_index].env_att = SHARP_JENNY_EG_SILENT;
        }
    }
}

SharpChip GetJennySource(Jenny* chip) {
    SharpChip source;
    source.ctx = chip;
    source.render = RenderJenny;
    source.write_reg = WriteJennyRegister;
    source.name = "Jenny";
    return source;
}
