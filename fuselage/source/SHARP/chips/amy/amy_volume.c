#include "amy_private.h"
#include <math.h>

static float sharp_amy_volume_tables[5][AMY_VOL_LEVELS];
static int sharp_amy_volume_tables_ready;

static void sharp_amy_build_volume_table(float* table, int distinct, double db_per_step) {
    for (int level_index = 0; level_index < AMY_VOL_LEVELS; level_index++) {
        int distinct_level = level_index * distinct / AMY_VOL_LEVELS;
        table[level_index] =
            (level_index * 16 / AMY_VOL_LEVELS == 0)
                ? 0.0f
                : (float)pow(10.0, -(double)(distinct - 1 - distinct_level) * db_per_step / 20.0);
    }

    return;
}

/* Initialize before rendering; concurrent chip initialization is not supported. */
void sharp_amy_ensure_volume_tables(void) {
    if (sharp_amy_volume_tables_ready) {
        return;
    }
    sharp_amy_build_volume_table(sharp_amy_volume_tables[AMY_VOL_AY8910], 16, 3.0);
    sharp_amy_build_volume_table(sharp_amy_volume_tables[AMY_VOL_YM2149], 32, 1.5);
    sharp_amy_build_volume_table(sharp_amy_volume_tables[AMY_VOL_LINEAR_2DB], 16, 2.0);

    /* ENTERON records GME Ay_Apu.cpp as the source of these AY values. */
    {
        static const double measured_ay[16] = {0.000000, 0.011940, 0.017413, 0.024876, 0.036318, 0.054229,
                                               0.072637, 0.122388, 0.174129, 0.239303, 0.323881, 0.410945,
                                               0.527363, 0.651741, 0.832338, 1.000000};
        for (int level_index = 0; level_index < AMY_VOL_LEVELS; level_index++) {
            sharp_amy_volume_tables[AMY_VOL_AY8910_REAL][level_index] =
                (float)measured_ay[level_index * 16 / AMY_VOL_LEVELS];
        }
    }

    /* ENTERON records Ayumi YM_dac_table as the source of these YM values. */
    {
        static const double measured_ym[AMY_VOL_LEVELS] = {
            0.00000000, 0.00000000, 0.00465400, 0.00772107, 0.01095598, 0.01396201, 0.01699855, 0.02001984,
            0.02436866, 0.02969406, 0.03506523, 0.04039063, 0.04853895, 0.05833524, 0.06805524, 0.07777523,
            0.09251545, 0.11108568, 0.12974746, 0.14848554, 0.17666896, 0.21155108, 0.24638743, 0.28110170,
            0.33373007, 0.40042725, 0.46738384, 0.53443198, 0.63517205, 0.75800717, 0.87992676, 1.00000000};
        for (int level_index = 0; level_index < AMY_VOL_LEVELS; level_index++) {
            sharp_amy_volume_tables[AMY_VOL_YM2149_REAL][level_index] = (float)measured_ym[level_index];
        }
    }
    sharp_amy_volume_tables_ready = 1;

    return;
}

const float* sharp_amy_get_volume_table(unsigned index) {
    return sharp_amy_volume_tables[index];
}
