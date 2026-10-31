/* Sequencer-to-bus mixing and capture placement. */
#include "sharp_facade_private.h"

static void sharp_facade_fill_buses(void* user, float* const* buses, int bus_count, uint32_t frames) {
    (void)user;
    SHARP_RenderSequenceOutputs(&sharp_facade_state.sequence, buses, bus_count, frames);

    /* Bus capture precedes bus gain/balance, including disabled buses. */
    for (int bus_index = 0; bus_index < bus_count && bus_index < SHARP_BUSES; bus_index++) {
        if (sharp_facade_is_capture_enabled(bus_index)) {
            sharp_facade_push_capture(bus_index, buses[bus_index], frames);
        }
    }
}

void sharp_facade_render(void* user, float* output, uint32_t frames) {
    (void)user;
    RenderLori(&sharp_facade_state.mixer, sharp_facade_fill_buses, NULL, output, frames);

    if (sharp_facade_is_capture_enabled(SHARP_BUSES)) {
        sharp_facade_push_capture(SHARP_BUSES, output, frames);
    }
}

bool SHARP_EnableBus(int bus, bool enabled) {
    return EnableLoriBus(&sharp_facade_state.mixer, bus, enabled ? 1 : 0) != 0;
}

bool SHARP_SetBusVolume(int bus, float volume) {
    return SetLoriBusVolume(&sharp_facade_state.mixer, bus, volume) != 0;
}

bool SHARP_SetBusBalance(int bus, float balance) {
    return SetLoriBusBalance(&sharp_facade_state.mixer, bus, balance) != 0;
}

void SHARP_SetMasterVolume(float gain) {
    SetLoriMasterGain(&sharp_facade_state.mixer, gain);
}
