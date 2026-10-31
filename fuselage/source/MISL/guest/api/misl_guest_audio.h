#pragma once
#include "misl_guest_memory.h"

// Audio ecalls, event batching and captured samples.
// SEE: (MISL_GUEST.md, Audio events and capture)

// Ring accessors require published control/ring pages; check their regions before use.
// Ring writes may overwrite undrained events. The host reports the loss after draining.

static inline bool SHARP_IsAvailable(void) {
    return MISL_GUEST_CALL(MISL_SHARP_AVAILABLE, 0, 0, 0, 0, 0, 0, 0) != 0;
}

static inline uint32_t SHARP_GetSampleRate(void) {
    return (uint32_t)MISL_GUEST_CALL(MISL_SHARP_SAMPLE_RATE, 0, 0, 0, 0, 0, 0, 0);
}

static inline uint64_t SHARP_GetRenderPosition(void) {
    return MISL_GUEST_CALL64(MISL_SHARP_POSITION, 0, 0, 0, 0, 0, 0, 0);
}

static inline int SHARP_CreateChip(unsigned kind) {
    return MISL_GUEST_CALL(MISL_SHARP_CREATE_CHIP, kind, 0, 0, 0, 0, 0, 0);
}

static inline int SHARP_GetChipCount(void) {
    return MISL_GUEST_CALL(MISL_SHARP_CHIP_COUNT, 0, 0, 0, 0, 0, 0, 0);
}

static inline uint32_t SHARP_GetPoolSize(void) {
    return (uint32_t)MISL_GUEST_CALL(MISL_SHARP_POOL_SIZE, 0, 0, 0, 0, 0, 0, 0);
}

static inline bool SHARP_WritePool(uint32_t offset, const void* data, uint32_t bytes) {
    return MISL_GUEST_CALL(MISL_SHARP_POOL_WRITE, offset, misl_guest_address(data), bytes, 0, 0, 0, 0) != 0;
}

static inline bool SHARP_Write(int chip, uint16_t reg, uint16_t value) {
    return MISL_GUEST_CALL(MISL_SHARP_WRITE, chip, reg, value, 0, 0, 0, 0) != 0;
}

static inline bool SHARP_WriteAt(int chip, uint16_t reg, uint16_t value, uint64_t position) {
    return MISL_GUEST_CALL(MISL_SHARP_WRITE_AT, chip, reg, value, MISL_GUEST_LOW(position),
                           MISL_GUEST_HIGH(position), 0, 0) != 0;
}

static inline uint32_t SHARP_GetDroppedEventCount(void) {
    return (uint32_t)MISL_GUEST_CALL(MISL_SHARP_EVENTS_DROPPED, 0, 0, 0, 0, 0, 0, 0);
}

static inline uint32_t SHARP_GetLateEventCount(void) {
    return (uint32_t)MISL_GUEST_CALL(MISL_SHARP_EVENTS_LATE, 0, 0, 0, 0, 0, 0, 0);
}

static inline bool SHARP_SetChipBus(int chip, int bus) {
    return MISL_GUEST_CALL(MISL_SHARP_SET_CHIP_BUS, chip, bus, 0, 0, 0, 0, 0) != 0;
}

static inline int SHARP_GetChipBus(int chip) {
    return MISL_GUEST_CALL(MISL_SHARP_CHIP_BUS, chip, 0, 0, 0, 0, 0, 0);
}

static inline bool SHARP_EnableBus(int bus, bool enabled) {
    return MISL_GUEST_CALL(MISL_SHARP_ENABLE_BUS, bus, enabled ? 1 : 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool SHARP_SetBusVolume(int bus, float volume) {
    return MISL_GUEST_CALL(MISL_SHARP_BUS_VOLUME, bus, misl_guest_float_bits(volume), 0, 0, 0, 0, 0) != 0;
}

static inline bool SHARP_SetBusBalance(int bus, float balance) {
    return MISL_GUEST_CALL(MISL_SHARP_BUS_BALANCE, bus, misl_guest_float_bits(balance), 0, 0, 0, 0, 0) != 0;
}

static inline void SHARP_SetMasterVolume(float gain) {
    MISL_GUEST_CALL(MISL_SHARP_MASTER_VOLUME, misl_guest_float_bits(gain), 0, 0, 0, 0, 0, 0);
}

static inline volatile MislSharpCtrl* SHARP_GetRing(void) {
    return (volatile MislSharpCtrl*)(uintptr_t)misl_guest_region_bases[MISL_REGION_SHARP_CTRL];
}

static inline uint64_t SHARP_GetRingPosition(void) {
    return SHARP_GetRing()->position;
}

static inline uint32_t SHARP_GetRingSampleRate(void) {
    return SHARP_GetRing()->sampleRate;
}

static inline uint32_t SHARP_GetRingCapacity(void) {
    return SHARP_GetRing()->capacity;
}

static inline uint32_t SHARP_GetRingDroppedEventCount(void) {
    return SHARP_GetRing()->dropped;
}

static inline uint32_t SHARP_GetRingLateEventCount(void) {
    return SHARP_GetRing()->late;
}

static inline uint32_t SHARP_GetRingDisorderedEventCount(void) {
    return SHARP_GetRing()->disordered;
}

static inline bool SHARP_IsRingReady(void) {
    return SHARP_GetRing()->available != 0;
}

static inline uint32_t SHARP_GetRingFreeCount(void) {
    volatile MislSharpCtrl* control = SHARP_GetRing();
    uint32_t used = control->writeIndex - control->readIndex;
    return (used >= control->capacity) ? 0u : (control->capacity - used);
}

static inline void SHARP_WriteRingEvent(int chip, uint16_t reg, uint16_t value, uint64_t position) {
    volatile MislSharpCtrl* control = SHARP_GetRing();
    volatile MislSharpEvent* ring =
        (volatile MislSharpEvent*)(uintptr_t)misl_guest_region_bases[MISL_REGION_SHARP_RING];
    uint32_t slot_index = control->writeIndex % MISL_SHARP_RING_EVENTS;

    ring[slot_index].time = position;
    ring[slot_index].chip = (uint8_t)chip;
    ring[slot_index]._pad = 0;
    ring[slot_index].reg = reg;
    ring[slot_index].value = value;

    control->writeIndex = control->writeIndex + 1u;
}

static inline volatile MislAudioCtrl* MISL_GetAudioCapture(void) {
    return (volatile MislAudioCtrl*)(uintptr_t)misl_guest_region_bases[MISL_REGION_SHARP_OUT];
}

static inline bool MISL_IsAudioCaptureAvailable(void) {
    return misl_guest_region_bases[MISL_REGION_SHARP_OUT] != 0u;
}

static inline void MISL_RequestAudioCapture(uint32_t slot, uint32_t bus, uint32_t format, uint32_t channels,
                                            uint32_t frames) {
    volatile MislAudioCtrl* control = MISL_GetAudioCapture();
    if (!control || slot >= MISL_AUDIO_REQUESTS) {
        return;
    }
    control->req[slot].bus = bus;
    control->req[slot].format = format;
    control->req[slot].channels = channels;
    control->req[slot].frames = frames;
    control->req[slot].enabled = 1u;
}

static inline void MISL_StopAudioCapture(uint32_t slot) {
    volatile MislAudioCtrl* control = MISL_GetAudioCapture();
    if (!control || slot >= MISL_AUDIO_REQUESTS) {
        return;
    }
    control->req[slot].enabled = 0u;
}

static inline const void* MISL_GetAudioCaptureData(uint32_t slot, uint32_t* frames) {
    volatile MislAudioCtrl* control = MISL_GetAudioCapture();
    if (frames) {
        *frames = 0;
    }
    if (!control || slot >= MISL_AUDIO_REQUESTS) {
        return 0;
    }
    uint32_t delivered = control->req[slot].delivered;
    if (!delivered) {
        return 0;
    }
    if (frames) {
        *frames = delivered;
    }
    return (const void*)((uintptr_t)control + control->req[slot].offset);
}
