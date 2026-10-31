#ifndef SHARP_FACADE_PRIVATE_H
#define SHARP_FACADE_PRIVATE_H
#include "../sharp.h"
#include "../backend/sharp_backend.h"
#include "../transport/sharp_events.h"
#include "../chips/adora/adora.h"
#include "../chips/sweeny/sweeny.h"
#include "../chips/amy/amy.h"
#include "../chips/jenny/jenny.h"
#include "../mixer/lori.h"

#define SHARP_FACADE_POOL_MIB_DEFAULT 16
#define SHARP_FACADE_POOL_MIB_MIN 4

_Static_assert(SHARP_BUSES == LORI_BUSES, "Facade and mixer bus counts must agree");

typedef struct {
    SharpChipKind kind;
    int sequence_id;
    union {
        Adora adora;
        Sweeny sweeny;
        Amy amy;
        Jenny jenny;
    } state;
} SharpFacadeChip;

typedef struct {
    SharpFacadeChip chips[SHARP_MAX_CHIPS];
    int chip_count;
    SharpSeq sequence;
    Lori mixer;
    SharpBackend* backend;
    bool available;
    bool initialized;
    uint8_t* pool;
    uint32_t pool_size;
    int pool_mib;
} SharpFacadeState;

/* Shared only by the facade's composition files. */
extern SharpFacadeState sharp_facade_state;
void sharp_facade_render(void* user, float* output, uint32_t frames);
void sharp_facade_push_capture(int slot, const float* stereo, uint32_t frames);
int sharp_facade_is_capture_enabled(int slot);
#endif
