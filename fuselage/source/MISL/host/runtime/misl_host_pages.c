/* Stable device-window storage, independent of per-load RAM. */
#include "misl_host_state_internal.h"

void* fuselage_guest_get_window_pointer(uint32_t address, uint32_t byte_count) {
    if (address < MISL_MMIO_BASE ||
        (uint64_t)address + byte_count > (uint64_t)MISL_MMIO_BASE + MISL_MMIO_BYTES) {
        return NULL;
    }
    return fuselage_guest_session.window + (address - MISL_MMIO_BASE);
}

MislInput* fuselage_guest_get_input_page(void) {
    return fuselage_guest_get_window_pointer(MISL_INPUT_ADDR, sizeof(MislInput));
}

MislClock* fuselage_guest_get_clock_page(void) {
    return fuselage_guest_get_window_pointer(MISL_CLOCK_ADDR, sizeof(MislClock));
}

MislSharpCtrl* fuselage_guest_get_audio_control_page(void) {
    return fuselage_guest_get_window_pointer(MISL_SHARP_CTRL_ADDR, sizeof(MislSharpCtrl));
}

void* fuselage_guest_get_audio_ring(void) {
    return fuselage_guest_get_window_pointer(MISL_SHARP_RING_ADDR, MISL_SHARP_RING_BYTES);
}

void* fuselage_guest_get_audio_output_page(void) {
    return fuselage_guest_get_window_pointer(MISL_SHARP_OUT_ADDR, MISL_MMIO_STRIDE);
}
