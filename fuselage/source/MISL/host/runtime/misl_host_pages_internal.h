/* Native pump/dispatch integration. SEE: (MISL_HOST.md, Mapped pages) */
#ifndef MISL_HOST_PAGES_INTERNAL_H
#define MISL_HOST_PAGES_INTERNAL_H
#include "../../misl_mmio.h"

MislInput* fuselage_guest_get_input_page(void);
MislClock* fuselage_guest_get_clock_page(void);
MislSharpCtrl* fuselage_guest_get_audio_control_page(void);
void* fuselage_guest_get_audio_ring(void);
void* fuselage_guest_get_audio_output_page(void);
void fuselage_guest_fill_clock(MislClock* output);
void fuselage_guest_refresh_pixie_banks(void);
void fuselage_guest_refresh_tile_banks(void);
uint64_t fuselage_guest_get_step_count(void);
#endif
