#pragma once
#include "misl_mmio_map.h"

// Host publishes control state before the guest slice and drains events afterwards.
// This is a sequential batching protocol, not a concurrent queue.
// SEE: (MISL_ABI_MMIO.md, Audio)

typedef struct {
    uint64_t time; // absolute sample position (SHARP's clock, not DICE's)
    uint8_t chip;  // which instantiated chip; a selector, not an address
    uint8_t _pad;  // explicit: alignment would insert this anyway
    uint16_t reg;  // register within that chip's own map (see note above)
    uint16_t value;
} MislSharpEvent;

// Indices advance modulo 2^32; slots use index modulo capacity.

typedef struct {
    uint32_t capacity;   // ring slots (MISL_SHARP_RING_EVENTS)
    uint32_t readIndex;  // how far the host has drained
    uint64_t position;   // SHARP's monotonic sample counter, as of this tick
    uint32_t sampleRate; // the device's, 0 when there is no device
    uint32_t available;  // 1 when audio is up; writes are ignored when 0
    uint32_t dropped;    // events lost because the guest outran the drain
    uint32_t late;
    uint32_t disordered;
    uint32_t _hostpad[7];

    uint32_t writeIndex;
} MislSharpCtrl;

// Request fields are guest-written; offset and delivered are host-written.

typedef struct {
    uint32_t bus;       // guest -> host: LORI bus index, or MISL_AUDIO_MASTER
    uint32_t format;    // guest -> host: MISL_AUDIO_FMT_*
    uint32_t channels;  // guest -> host: 1 (summed) or 2 (interleaved)
    uint32_t frames;    // guest -> host: how much history is wanted
    uint32_t enabled;   // guest -> host: 0 stops retaining this bus
    uint32_t offset;    // host -> guest: byte offset from the PAGE base
    uint32_t delivered; // host -> guest: frames actually written, <= frames
    uint32_t _pad;      // explicit: keeps the struct 32 bytes on both sides
} MislAudioRequest;

typedef struct {
    uint32_t maxFrames;  // host -> guest: the deepest capture SHARP will keep
    uint32_t dataOffset; // host -> guest: where the data area starts
    uint32_t dataBytes;  // host -> guest: how big it is
    uint32_t requests;   // host -> guest: how many slots below are real
    MislAudioRequest req[MISL_AUDIO_REQUESTS];
} MislAudioCtrl;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(MislSharpEvent) == MISL_SHARP_EVENT_BYTES, "Audio event layout changed");
_Static_assert(sizeof(MislSharpCtrl) == 72u, "Audio ring control layout changed");
_Static_assert(sizeof(MislSharpCtrl) <= MISL_SHARP_CTRL_BYTES, "Audio control exceeds its page");
_Static_assert(sizeof(MislAudioRequest) == 32u, "Audio request layout changed");
_Static_assert(sizeof(MislAudioCtrl) == 272u, "Audio capture control layout changed");
_Static_assert(sizeof(MislAudioCtrl) <= MISL_AUDIO_DATA_OFFSET, "Audio control overlaps samples");
_Static_assert(MISL_SHARP_RING_BYTES <= MISL_MMIO_STRIDE, "Audio ring exceeds its region");
#endif
