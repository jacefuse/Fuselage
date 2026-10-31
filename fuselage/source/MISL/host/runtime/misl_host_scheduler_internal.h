#ifndef MISL_HOST_SCHEDULER_INTERNAL_H
#define MISL_HOST_SCHEDULER_INTERNAL_H
#include <stdint.h>
#define MISL_HOST_MIN_SLICE_US 1000u
#define MISL_HOST_STEP_CHUNK 200000u
uint64_t fuselage_guest_slice_budget(void);
void fuselage_guest_end_frame(uint64_t slice_us);
void fuselage_guest_cut_slice(uint64_t slice_us);
void fuselage_guest_reset_devices(void);
void fuselage_guest_pump_devices(void);
void fuselage_guest_pump_input(void);
void fuselage_guest_pump_audio(void);
void fuselage_guest_drain_audio(void);
#endif
