/* Private root pacing contract. SEE: (MISL_LAUNCHER.md, Timing reports) */
#ifndef FUSELAGE_PACING_H
#define FUSELAGE_PACING_H
#include <stdbool.h>
#include <stdint.h>
/* Root state lock held for all operations except init/shutdown. */
void fuselage_pacing_init(uint8_t simulation_timer, uint8_t wait_timer, double simulation_rate);
void fuselage_pacing_shutdown(void);
void fuselage_pacing_tick(uint64_t begin_us, uint64_t end_us);
void fuselage_pacing_frame(uint64_t lock_request_us, uint64_t lock_held_us, uint64_t sample_us,
                           uint64_t previous_submit_us);
bool fuselage_pacing_status(double* refresh_rate, int* ratio, double* period_us);
#endif
