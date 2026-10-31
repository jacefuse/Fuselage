#ifndef FUSELAGE_PACING_INTERNAL_H
#define FUSELAGE_PACING_INTERNAL_H
#include "fuselage_pacing.h"
#define FUSELAGE_PACING_RING 256
#define FUSELAGE_PACING_MIN_GOOD 60
#define FUSELAGE_PACING_BOOT 64
#define FUSELAGE_PACING_SETTLED 64
#define FUSELAGE_PACING_LOCK_TOL 0.010
#define FUSELAGE_PACING_UNLOCK_TOL 0.015
#define FUSELAGE_PACING_RESIDUAL_TOL 0.10
#define FUSELAGE_PACING_BAD_RUN 8
#define FUSELAGE_PACING_GAP_US 500000
#define FUSELAGE_PACING_MAX_RATIO 8
#define FUSELAGE_PACING_MARGIN_US 2000
#define FUSELAGE_PACING_OVERLAP_US 200
#define FUSELAGE_PACING_ADVANCE_GAIN 0.10
#define FUSELAGE_PACING_MAX_SHIFT_FRAC 0.05
#define FUSELAGE_PACING_PERIOD_EPS_US 0.25
typedef struct {
    uint64_t sample_us, refresh_index;
} FuselagePacingMark;
typedef struct {
    uint8_t simulation_timer, wait_timer;
    double declared_us;
    bool enabled, trace, trace_frames, initialized, retune_failed;
    uint64_t tick_count, last_tick_end_us, tick_duration_sum_us, tick_duration_max_us;
    uint32_t tick_duration_count;
    uint64_t last_sample_us, ticks_seen;
    FuselagePacingMark ring[FUSELAGE_PACING_RING];
    uint32_t ring_head, ring_count;
    uint64_t refresh_index;
    double refresh_us;
    uint32_t good_intervals, bad_run;
    double bootstrap[FUSELAGE_PACING_BOOT];
    uint32_t bootstrap_count;
    bool locked;
    int ratio;
    double target_us, applied_us, last_error;
    uint64_t trace_start_us;
    uint32_t trace_frames_count, trace_histogram[4];
    double trace_lag_sum, trace_lag_max, trace_wait_max, trace_prepare_max, trace_submit_max, trace_shift_sum;
    uint32_t trace_anomalies;
} FuselagePacingState;
extern FuselagePacingState fuselage_pacing_state;
bool fuselage_pacing_apply_period(double period_us);
void fuselage_pacing_unlock(const char* reason);
void fuselage_pacing_reset_estimator(const char* reason);
int fuselage_pacing_cmp_double(const void* left, const void* right);
double fuselage_pacing_evaluate(int* ratio_output, double* target_output);
void fuselage_pacing_trace_second(uint64_t now_us);
#endif
