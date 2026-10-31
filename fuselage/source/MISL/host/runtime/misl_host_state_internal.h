/* Shared native session state. SEE: (MISL_HOST.md, Load ownership) */
#ifndef MISL_HOST_STATE_INTERNAL_H
#define MISL_HOST_STATE_INTERNAL_H
#include "misl_host_image_internal.h"
#include "misl_host_pages_internal.h"
#include "../../../VIVIAN/vivian.h"

#define MISL_HOST_SPRITE_DIRTY_WORDS ((MISL_SPRITE_SLOTS + 31u) / 32u)
#define MISL_HOST_TILE_DIRTY_WORDS ((MISL_TILE_SLOTS + 31u) / 32u)
#define MISL_HOST_SLICE_CEILING_US 250000u

typedef enum { MISL_HOST_UNLOADED, MISL_HOST_RUNNING, MISL_HOST_HALTED, MISL_HOST_PREPARED } MislHostPhase;

enum { MISL_HOST_ASSET_SPAN = 0, MISL_HOST_BOOTSPUR_SPAN = 1, MISL_HOST_SAVE_SPAN = 2 };

typedef struct {
    VivianState cpu;
    uint8_t* memory;
    MislHostImagePlan plan;
    const MislGuestImage* image;
    const uint8_t* save_span;
    uint32_t save_byte_count;
    _Alignas(8) uint8_t window[MISL_MMIO_BYTES];
    MislHostPhase phase;
    uint32_t memory_cap_mib;
    char capture_directory[400];
    char program_path[1024];
    MislGuestHaltInfo halt;
    char halt_reason[160];
    uint8_t* pixie_banks[MISL_PIXIE_RAM_BANKS];
    uint8_t* tile_banks[MISL_TILE_MAP_BANKS];
    uint8_t tile_writable[MISL_TILE_MAP_BANKS];
    uint8_t tile_map_dirty[MISL_TILE_MAP_BANKS];
    int tile_maps_dirty;
    uint32_t sprite_dirty[MISL_HOST_SPRITE_DIRTY_WORDS];
    int sprites_dirty;
    uint32_t tile_dirty[MISL_TILE_LAYERS][MISL_HOST_TILE_DIRTY_WORDS];
    int tiles_dirty;
    int abi_declared;
    uint32_t sleep_ticks;
    uint64_t frame_estimate_us;
    uint64_t frame_accumulated_us;
    int frame_abandoned;
    uint64_t steps_executed;
} MislHostSession;

extern MislHostSession fuselage_guest_session;

void* fuselage_guest_get_window_pointer(uint32_t address, uint32_t byte_count);
void fuselage_guest_release_memory(void);
int fuselage_guest_prepare_load(const MislGuestImage* image);
int fuselage_guest_commit_load(const uint8_t* save_span, uint32_t save_byte_count);
void fuselage_guest_fail_load(MislGuestHaltKind kind, const char* reason);
void fuselage_guest_halt(MislGuestHaltKind kind, const char* reason);
int fuselage_guest_publish_descriptor(void);
#endif
