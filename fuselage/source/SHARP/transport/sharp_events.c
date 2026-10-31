// Event scheduling. SEE: (SHARP.md, Event sequencing)
#include "sharp_events.h"
#include <stdlib.h>
#include <string.h>

static uint32_t sharp_sequence_round_capacity(uint32_t requested_capacity) {
    uint32_t capacity = 1;
    while (capacity < requested_capacity) {
        capacity <<= 1;
    }
    return capacity;
}

int SHARP_InitSequence(SharpSeq* sequence, uint32_t event_capacity) {
    memset(sequence, 0, sizeof *sequence);
    if (event_capacity > (UINT32_C(1) << 31)) {
        return 0;
    }
    uint32_t capacity = sharp_sequence_round_capacity(event_capacity ? event_capacity : 1024);
    if ((uint64_t)capacity * sizeof(SharpEvent) > SIZE_MAX) {
        return 0;
    }
    sequence->ring.cap = capacity;
    sequence->ring.mask = sequence->ring.cap - 1;
    sequence->ring.buf = calloc(sequence->ring.cap, sizeof(SharpEvent));
    if (!sequence->ring.buf) {
        return 0;
    }
    atomic_store(&sequence->ring.r, 0);
    atomic_store(&sequence->ring.w, 0);
    atomic_store(&sequence->ring.dropped, 0);
    atomic_store(&sequence->ring.late, 0);
    atomic_store(&sequence->ring.disordered, 0);
    sequence->ring.last_time = 0;
    return 1;
}

void SHARP_FreeSequence(SharpSeq* sequence) {
    free(sequence->ring.buf);
    sequence->ring.buf = NULL;
}

int SHARP_AddSequenceChip(SharpSeq* sequence, const SharpChip* chip) {
    if (sequence->chip_count >= SHARP_MAX_CHIPS) {
        return -1;
    }
    sequence->chips[sequence->chip_count] = *chip;
    return sequence->chip_count++;
}

int SHARP_WriteSequence(SharpSeq* sequence, uint64_t position, uint8_t chip, uint16_t reg, uint16_t value) {
    SharpEventRing* ring = &sequence->ring;
    uint32_t write_cursor = atomic_load_explicit(&ring->w, memory_order_relaxed);
    uint32_t read_cursor = atomic_load_explicit(&ring->r, memory_order_acquire);
    if (write_cursor - read_cursor >= ring->cap) {
        atomic_fetch_add(&ring->dropped, 1);
        return 0;
    }

    if (position < ring->last_time) {
        atomic_fetch_add(&ring->disordered, 1);
        position = ring->last_time;
    }
    ring->last_time = position;

    SharpEvent* event = &ring->buf[write_cursor & ring->mask];
    event->time = position;
    event->chip = chip;
    event->reg = reg;
    event->value = value;
    atomic_store_explicit(&ring->w, write_cursor + 1, memory_order_release);
    return 1;
}

static int sharp_sequence_peek_position(SharpEventRing* ring, uint64_t* output) {
    uint32_t read_cursor = atomic_load_explicit(&ring->r, memory_order_relaxed);
    uint32_t write_cursor = atomic_load_explicit(&ring->w, memory_order_acquire);
    if (read_cursor == write_cursor) {
        return 0;
    }
    *output = ring->buf[read_cursor & ring->mask].time;
    return 1;
}

static void sharp_sequence_dispatch_due(SharpSeq* sequence, uint64_t now) {
    SharpEventRing* ring = &sequence->ring;
    for (;;) {
        uint32_t read_cursor = atomic_load_explicit(&ring->r, memory_order_relaxed);
        uint32_t write_cursor = atomic_load_explicit(&ring->w, memory_order_acquire);
        if (read_cursor == write_cursor) {
            break;
        }
        SharpEvent event = ring->buf[read_cursor & ring->mask];
        if (event.time > now) {
            break;
        }

        if (event.time < now) {
            atomic_fetch_add(&ring->late, 1);
        }
        if (event.chip < sequence->chip_count) {
            SharpChip* source = &sequence->chips[event.chip];
            if (source->write_reg) {
                source->write_reg(source->ctx, event.reg, event.value);
            }
        }
        atomic_store_explicit(&ring->r, read_cursor + 1, memory_order_release);
    }
}

int SHARP_SetSequenceOutput(SharpSeq* sequence, int chip, int output_slot) {
    if (!sequence || chip < 0 || chip >= sequence->chip_count) {
        return 0;
    }
    if (output_slot < 0 || output_slot > 255) {
        return 0;
    }
    sequence->slot[chip] = (uint8_t)output_slot;
    return 1;
}

int SHARP_GetSequenceOutput(const SharpSeq* sequence, int chip) {
    if (!sequence || chip < 0 || chip >= sequence->chip_count) {
        return -1;
    }
    return (int)sequence->slot[chip];
}

void SHARP_RenderSequenceOutputs(SharpSeq* sequence, float* const* outputs, int output_count,
                                 uint32_t frames) {
    for (int output_index = 0; output_index < output_count; output_index++) {
        memset(outputs[output_index], 0, (size_t)frames * 2 * sizeof(float));
    }

    uint32_t rendered_frames = 0;
    while (rendered_frames < frames) {
        uint64_t now = sequence->pos + rendered_frames;

        sharp_sequence_dispatch_due(sequence, now);

        uint32_t segment_frames = frames - rendered_frames;
        uint64_t next_event_position;
        if (sharp_sequence_peek_position(&sequence->ring, &next_event_position)) {
            uint64_t frames_until_event = (next_event_position > now) ? (next_event_position - now) : 0;
            if (frames_until_event == 0) {
                continue;
            }
            if (frames_until_event < (uint64_t)segment_frames) {
                segment_frames = (uint32_t)frames_until_event;
            }
        }

        for (int chip_index = 0; chip_index < sequence->chip_count; chip_index++) {
            SharpChip* source = &sequence->chips[chip_index];
            int output_index = (int)sequence->slot[chip_index];
            if (!source->render || output_index >= output_count) {
                continue;
            }
            source->render(source->ctx, outputs[output_index] + (size_t)rendered_frames * 2, segment_frames);
        }
        rendered_frames += segment_frames;
    }
    sequence->pos += frames;
}

void SHARP_RenderSequence(SharpSeq* sequence, float* output, uint32_t frames) {
    SHARP_RenderSequenceOutputs(sequence, &output, 1, frames);
}

uint64_t SHARP_GetSequencePosition(const SharpSeq* sequence) {
    return sequence->pos;
}
uint32_t SHARP_GetSequenceDroppedEventCount(const SharpSeq* sequence) {
    return atomic_load((_Atomic uint32_t*)&sequence->ring.dropped);
}
uint32_t SHARP_GetSequenceLateEventCount(const SharpSeq* sequence) {
    return atomic_load((_Atomic uint32_t*)&sequence->ring.late);
}
uint32_t SHARP_GetSequenceDisorderedEventCount(const SharpSeq* sequence) {
    return atomic_load((_Atomic uint32_t*)&sequence->ring.disordered);
}
