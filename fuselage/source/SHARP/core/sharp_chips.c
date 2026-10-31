/* Built-in chip composition, routing and queued writes. */
#include "sharp_facade_private.h"

#define SHARP_FACADE_WRITE_LEAD 64u

int SHARP_CreateChip(SharpChipKind kind) {
    if (!sharp_facade_state.available || sharp_facade_state.chip_count >= SHARP_MAX_CHIPS) {
        return -1;
    }

    SharpFacadeChip* facade_chip = &sharp_facade_state.chips[sharp_facade_state.chip_count];
    facade_chip->kind = kind;

    double rate = (double)SHARP_GetBackendSampleRate(sharp_facade_state.backend);
    SharpChip contract;

    switch (kind) {
    case SHARP_CHIP_SWEENY:
        InitSweeny(&facade_chip->state.sweeny, rate);
        contract = GetSweenySource(&facade_chip->state.sweeny);
        break;
    case SHARP_CHIP_AMY:

        InitAmy(&facade_chip->state.amy, rate);
        contract = GetAmySource(&facade_chip->state.amy);
        break;
    case SHARP_CHIP_JENNY:
        InitJenny(&facade_chip->state.jenny, rate);
        contract = GetJennySource(&facade_chip->state.jenny);
        break;
    case SHARP_CHIP_ADORA:
    default:
        /* Retain the public fallback to Adora for unknown kinds. */

        facade_chip->kind = SHARP_CHIP_ADORA;
        InitAdora(&facade_chip->state.adora, rate, rate, sharp_facade_state.pool,
                  sharp_facade_state.pool_size);
        contract = GetAdoraSource(&facade_chip->state.adora);
        break;
    }

    int sequence_id = SHARP_AddSequenceChip(&sharp_facade_state.sequence, &contract);
    if (sequence_id < 0) {
        return -1;
    }
    facade_chip->sequence_id = sequence_id;
    sharp_facade_state.chip_count++;

    SHARP_SetSequenceOutput(&sharp_facade_state.sequence, sequence_id, 0);
    EnableLoriBus(&sharp_facade_state.mixer, 0, 1);
    return sequence_id;
}

int SHARP_GetChipCount(void) {
    return sharp_facade_state.chip_count;
}

const char* SHARP_GetChipName(int chip) {
    if (chip < 0 || chip >= sharp_facade_state.chip_count) {
        return "(none)";
    }

    switch (sharp_facade_state.chips[chip].kind) {
    case SHARP_CHIP_SWEENY:
        return "Sweeny";
    case SHARP_CHIP_AMY:
        return "Amy";
    case SHARP_CHIP_JENNY:
        return "Jenny";
    case SHARP_CHIP_ADORA:
        return "Adora";
    default:
        return "(unknown)";
    }
}

bool SHARP_WriteAt(int chip, uint16_t reg, uint16_t value, uint64_t position) {
    if (!sharp_facade_state.available || chip < 0 || chip >= sharp_facade_state.chip_count) {
        return false;
    }
    return SHARP_WriteSequence(&sharp_facade_state.sequence, position,
                               (uint8_t)sharp_facade_state.chips[chip].sequence_id, reg, value) != 0;
}

bool SHARP_Write(int chip, uint16_t reg, uint16_t value) {
    if (!sharp_facade_state.available) {
        return false;
    }
    return SHARP_WriteAt(chip, reg, value,
                         SHARP_GetSequencePosition(&sharp_facade_state.sequence) + SHARP_FACADE_WRITE_LEAD);
}

uint32_t SHARP_GetDroppedEventCount(void) {
    return sharp_facade_state.available ? SHARP_GetSequenceDroppedEventCount(&sharp_facade_state.sequence)
                                        : 0;
}

uint32_t SHARP_GetLateEventCount(void) {
    return sharp_facade_state.available ? SHARP_GetSequenceLateEventCount(&sharp_facade_state.sequence) : 0;
}

uint32_t SHARP_GetDisorderedEventCount(void) {
    return sharp_facade_state.available ? SHARP_GetSequenceDisorderedEventCount(&sharp_facade_state.sequence)
                                        : 0;
}

bool SHARP_SetChipBus(int chip, int bus) {
    if (!sharp_facade_state.available || chip < 0 || chip >= sharp_facade_state.chip_count) {
        return false;
    }
    if (bus < 0 || bus >= SHARP_BUSES) {
        return false;
    }
    if (!SHARP_SetSequenceOutput(&sharp_facade_state.sequence, sharp_facade_state.chips[chip].sequence_id,
                                 bus)) {
        return false;
    }

    EnableLoriBus(&sharp_facade_state.mixer, bus, 1);
    return true;
}

int SHARP_GetChipBus(int chip) {
    if (!sharp_facade_state.available || chip < 0 || chip >= sharp_facade_state.chip_count) {
        return -1;
    }
    return SHARP_GetSequenceOutput(&sharp_facade_state.sequence, sharp_facade_state.chips[chip].sequence_id);
}
