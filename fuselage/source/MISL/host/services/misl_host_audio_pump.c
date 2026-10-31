/* Audio MMIO transfer. SEE: (MISL_HOST_RUNTIME.md, Audio pumps) */
#include "../runtime/misl_host_pages_internal.h"
#include "../runtime/misl_host_scheduler_internal.h"
#include "../../../SHARP/sharp.h"
#include "../../../SHARP/transport/sharp_events.h"
#include <stddef.h>
#include <string.h>
#define MISL_HOST_CAPTURE_SLOTS (SHARP_BUSES + 1)
static uint32_t fuselage_guest_capture_frames[MISL_HOST_CAPTURE_SLOTS];
static bool fuselage_guest_capture_applied[MISL_HOST_CAPTURE_SLOTS];
_Static_assert(sizeof(MislSharpEvent) == sizeof(SharpEvent) &&
                   offsetof(MislSharpEvent, time) == offsetof(SharpEvent, time) &&
                   offsetof(MislSharpEvent, chip) == offsetof(SharpEvent, chip) &&
                   offsetof(MislSharpEvent, reg) == offsetof(SharpEvent, reg) &&
                   offsetof(MislSharpEvent, value) == offsetof(SharpEvent, value),
               "Audio event mirror differs");
static int fuselage_guest_capture_slot(uint32_t bus) {
    if (bus == MISL_AUDIO_MASTER) {
        return SHARP_BUSES;
    }
    return (bus < (uint32_t)SHARP_BUSES) ? (int)bus : -1;
}

static int fuselage_guest_capture_bus(int slot) {
    return (slot == SHARP_BUSES) ? SHARP_CAPTURE_MASTER : slot;
}

static void fuselage_guest_pump_capture(void) {
    MislAudioCtrl* control = (MislAudioCtrl*)fuselage_guest_get_audio_output_page();

    control->maxFrames = SHARP_GetCaptureMaxFrames();
    control->dataOffset = MISL_AUDIO_DATA_OFFSET;
    control->dataBytes = MISL_AUDIO_DATA_BYTES;
    control->requests = MISL_AUDIO_REQUESTS;

    uint32_t want[MISL_HOST_CAPTURE_SLOTS];
    for (int slot = 0; slot < MISL_HOST_CAPTURE_SLOTS; slot++) {
        want[slot] = 0;
    }

    for (uint32_t index = 0; index < MISL_AUDIO_REQUESTS; index++) {
        MislAudioRequest* request = &control->req[index];
        if (!request->enabled) {
            continue;
        }
        int slot = fuselage_guest_capture_slot(request->bus);
        if (slot < 0) {
            continue;
        }
        uint32_t frames = request->frames;
        if (frames > control->maxFrames) {
            frames = control->maxFrames;
        }
        if (frames > want[slot]) {
            want[slot] = frames;
        }
    }

    for (int slot = 0; slot < MISL_HOST_CAPTURE_SLOTS; slot++) {
        if (fuselage_guest_capture_applied[slot] && want[slot] == fuselage_guest_capture_frames[slot]) {
            continue;
        }
        if (SHARP_SetCapture(fuselage_guest_capture_bus(slot), want[slot], want[slot] != 0)) {
            fuselage_guest_capture_frames[slot] = want[slot];
            fuselage_guest_capture_applied[slot] = true;
        }
    }

    uint8_t* data = (uint8_t*)control + MISL_AUDIO_DATA_OFFSET;
    uint32_t cursor = 0;

    for (uint32_t index = 0; index < MISL_AUDIO_REQUESTS; index++) {
        MislAudioRequest* request = &control->req[index];
        request->offset = 0;
        request->delivered = 0;
        if (!request->enabled) {
            continue;
        }

        int slot = fuselage_guest_capture_slot(request->bus);
        if (slot < 0) {
            continue;
        }

        uint32_t channels = (request->channels == 1u) ? 1u : 2u;
        uint32_t format = (request->format == MISL_AUDIO_FMT_F32) ? MISL_AUDIO_FMT_F32 : MISL_AUDIO_FMT_S16;
        uint32_t frame_bytes = channels * ((format == MISL_AUDIO_FMT_F32) ? 4u : 2u);

        uint32_t room = (MISL_AUDIO_DATA_BYTES - cursor) / frame_bytes;
        uint32_t requested = request->frames;
        if (requested > control->maxFrames) {
            requested = control->maxFrames;
        }
        if (requested > room) {
            requested = room;
        }
        if (requested == 0) {
            continue;
        }

        uint32_t delivered =
            SHARP_ReadCapture(fuselage_guest_capture_bus(slot), data + cursor, requested,
                              (format == MISL_AUDIO_FMT_F32) ? SHARP_FMT_F32 : SHARP_FMT_S16, (int)channels);
        if (delivered > requested) {
            delivered = requested;
        }
        request->offset = MISL_AUDIO_DATA_OFFSET + cursor;
        request->delivered = delivered;

        cursor += ((delivered * frame_bytes) + 3u) & ~3u;
    }
}

void fuselage_guest_pump_audio(void) {
    MislSharpCtrl* control = (MislSharpCtrl*)fuselage_guest_get_audio_control_page();

    control->capacity = MISL_SHARP_RING_EVENTS;
    control->position = SHARP_GetRenderPosition();
    control->sampleRate = SHARP_GetSampleRate();
    control->available = SHARP_IsAvailable() ? 1u : 0u;
    control->late = SHARP_GetLateEventCount();

    fuselage_guest_pump_capture();
}

void fuselage_guest_drain_audio(void) {
    MislSharpCtrl* control = (MislSharpCtrl*)fuselage_guest_get_audio_control_page();
    const MislSharpEvent* ring = (const MislSharpEvent*)fuselage_guest_get_audio_ring();

    uint32_t write_index = control->writeIndex;
    uint32_t read_index = control->readIndex;
    uint32_t event_count = write_index - read_index;

    if (event_count > MISL_SHARP_RING_EVENTS) {
        control->dropped += event_count - MISL_SHARP_RING_EVENTS;
        read_index = write_index - MISL_SHARP_RING_EVENTS;
        event_count = MISL_SHARP_RING_EVENTS;
    }

    for (uint32_t index = 0; index < event_count; index++) {
        const MislSharpEvent* event = &ring[(read_index + index) % MISL_SHARP_RING_EVENTS];

        SHARP_WriteAt((int)event->chip, event->reg, event->value, event->time);
    }

    control->readIndex = write_index;
    control->late = SHARP_GetLateEventCount();
    control->disordered = SHARP_GetDisorderedEventCount();
}

void fuselage_guest_reset_devices(void) {
    memset(fuselage_guest_get_audio_control_page(), 0, MISL_SHARP_CTRL_BYTES);
    memset(fuselage_guest_get_audio_ring(), 0, MISL_SHARP_RING_BYTES);
    memset(fuselage_guest_get_audio_output_page(), 0, MISL_MMIO_STRIDE);
    for (int slot = 0; slot < MISL_HOST_CAPTURE_SLOTS; slot++) {
        fuselage_guest_capture_applied[slot] = SHARP_SetCapture(fuselage_guest_capture_bus(slot), 0, false);
        fuselage_guest_capture_frames[slot] = 0;
    }
}
