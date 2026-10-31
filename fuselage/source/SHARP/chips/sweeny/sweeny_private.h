#ifndef SWEENY_PRIVATE_H
#define SWEENY_PRIVATE_H
#include "sweeny.h"

void sharp_sweeny_configure_envelope(Sweeny* chip, SweenyVoice* voice);
void sharp_sweeny_set_voice_gains(SweenyVoice* voice);
void sharp_sweeny_refresh_step(Sweeny* chip, SweenyVoice* voice);
#endif
