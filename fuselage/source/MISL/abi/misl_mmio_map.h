#pragma once
#include <stdint.h>

// Current host allocation map and descriptor contract.
// Discover regions through MISL_SYS_DESCRIPTOR. Atlas helpers also use fixed addresses.
// SEE: (MISL_ABI_MMIO.md, Memory discovery)

// Default program allocation and placement policy.
#define MISL_MEM_SIZE (16u * 1024u * 1024u)
#define MISL_STACK_RESERVE (1u * 1024u * 1024u)
#define MISL_ENGINE_ZONE 0xE0000000u
#define MISL_SHARED_TOP MISL_ENGINE_ZONE
#define MISL_PLACE_ALIGN 0x00010000u

// Writable window and descriptor location.
#define MISL_MMIO_BASE 0xF8000000u
#define MISL_MMIO_STRIDE 0x00040000u
#define MISL_INPUT_ADDR (MISL_MMIO_BASE + 0u * MISL_MMIO_STRIDE)
#define MISL_DESC_ADDR (MISL_MMIO_BASE + 1u * MISL_MMIO_STRIDE)
#define MISL_DESC_MAGIC 0x50414F52u

// Region IDs are append-only; pool IDs occupy 17 through 24.
#define MISL_REGION_INPUT 1u
#define MISL_REGION_DESC 2u
#define MISL_REGION_STATUS 3u
#define MISL_REGION_SHARP_CTRL 4u
#define MISL_REGION_SHARP_RING 5u
#define MISL_REGION_SPRITE_ATLAS 6u
#define MISL_REGION_TILE_POOL 7u
#define MISL_REGION_SHARP_OUT 9u
#define MISL_REGION_PIXIE_LIST 10u
#define MISL_REGION_PIXIE_RAM 11u
#define MISL_REGION_PROGRAM 8u
#define MISL_REGION_ASSET_ROM 12u
#define MISL_REGION_CLOCK 13u
#define MISL_REGION_BOOTSPUR 14u
#define MISL_REGION_SRAM 15u
#define MISL_REGION_POOL 17u
#define MISL_REGION_TILE_MAP 16u

// Published access flags.
#define MISL_REGION_READ 0x1u
#define MISL_REGION_WRITE 0x2u
#define MISL_REGION_EXEC 0x4u
#define MISL_DESC_MAX_REGIONS 32u

// Submission result page; counters are unsigned 32-bit values.
#define MISL_STATUS_ADDR (MISL_MMIO_BASE + 2u * MISL_MMIO_STRIDE)
#define MISL_STATUS_BYTES 0x1000u
#define MISL_PIXIE_NO_REJECT 0xFFFFFFFFu

// Audio ring and capture allocation.
#define MISL_SHARP_CTRL_ADDR (MISL_MMIO_BASE + 3u * MISL_MMIO_STRIDE)
#define MISL_SHARP_CTRL_BYTES 0x1000u
#define MISL_SHARP_RING_ADDR (MISL_MMIO_BASE + 4u * MISL_MMIO_STRIDE)
#define MISL_SHARP_RING_EVENTS 2048u
#define MISL_SHARP_EVENT_BYTES 16u
#define MISL_SHARP_RING_BYTES (MISL_SHARP_RING_EVENTS * MISL_SHARP_EVENT_BYTES)
#define MISL_SHARP_OUT_ADDR (MISL_MMIO_BASE + 5u * MISL_MMIO_STRIDE)
#define MISL_AUDIO_FMT_S16 0u
#define MISL_AUDIO_FMT_F32 1u
#define MISL_AUDIO_MASTER 0xFFFFFFFFu
#define MISL_AUDIO_REQUESTS 8u
#define MISL_AUDIO_DATA_OFFSET 0x1000u
#define MISL_AUDIO_DATA_BYTES (MISL_MMIO_STRIDE - MISL_AUDIO_DATA_OFFSET)

// Read-only executable system ROM, outside the writable window.
#define MISL_ROM_ADDR 0xF9F00000u
#define MISL_ROM_BYTES 0x00040000u

// Guest-built pixie command scratch list.
#define MISL_PIXIE_LIST_ADDR (MISL_MMIO_BASE + 7u * MISL_MMIO_STRIDE)
#define MISL_PIXIE_LIST_MAX 4096u
#define MISL_PIXIE_CMD_BYTES 20u
#define MISL_PIXIE_LIST_BYTES (MISL_PIXIE_LIST_MAX * MISL_PIXIE_CMD_BYTES)

// Packed 4-bit sprite and tile bitmap storage.
#define MISL_SPRITE_ADDR (MISL_MMIO_BASE + 0x00800000u)
#define MISL_SPRITE_STRIDE 2048u
#define MISL_SPRITE_SLOTS 1024u
#define MISL_SPRITE_BYTES (MISL_SPRITE_SLOTS * MISL_SPRITE_STRIDE)
#define MISL_TILE_ADDR (MISL_SPRITE_ADDR + MISL_SPRITE_BYTES)
#define MISL_TILE_LAYERS 16u
#define MISL_TILE_SLOTS 1024u
#define MISL_TILE_LAYER_STRIDE 0x00080000u
#define MISL_TILE_BYTES (MISL_TILE_LAYERS * MISL_TILE_LAYER_STRIDE)
#define MISL_TILE_LAYER_ADDR(layer) (MISL_TILE_ADDR + (uint32_t)(layer) * MISL_TILE_LAYER_STRIDE)
#define MISL_MMIO_BYTES ((MISL_TILE_ADDR - MISL_MMIO_BASE) + MISL_TILE_BYTES)

// Read-only pixie RAM banks; each bank has a 4 MiB stride.
#define MISL_PIXIE_RAM_ADDR 0xE8000000u
#define MISL_PIXIE_RAM_SLOT 0x08000000u
#define MISL_PIXIE_RAM_SHIFT 22u
#define MISL_PIXIE_RAM_STRIDE (1u << MISL_PIXIE_RAM_SHIFT)
#define MISL_PIXIE_RAM_BANKS 16u
#define MISL_PIXIE_RAM_BYTES (MISL_PIXIE_RAM_BANKS * MISL_PIXIE_RAM_STRIDE)
#define MISL_PIXIE_RAM_OF(id) (MISL_PIXIE_RAM_ADDR + (uint32_t)(id) * MISL_PIXIE_RAM_STRIDE)

// Writable tile cells; 512 by 512 cells per bank.
#define MISL_TILE_MAP_ADDR 0xF4000000u
#define MISL_TILE_MAP_SHIFT 22u
#define MISL_TILE_MAP_STRIDE (1u << MISL_TILE_MAP_SHIFT)
#define MISL_TILE_MAP_BANKS 16u
#define MISL_TILE_MAP_WIDTH 512u
#define MISL_TILE_MAP_HEIGHT 512u
#define MISL_TILE_CELL_BYTES 12u
#define MISL_TILE_MAP_BYTES (MISL_TILE_MAP_WIDTH * MISL_TILE_MAP_HEIGHT * MISL_TILE_CELL_BYTES)
#define MISL_TILE_MAP_SPAN (MISL_TILE_MAP_BANKS * MISL_TILE_MAP_STRIDE)
#define MISL_TILE_MAP_OF(layer) (MISL_TILE_MAP_ADDR + (uint32_t)(layer) * MISL_TILE_MAP_STRIDE)
#define MISL_TILE_CELL_COLLISION (1u << 0)
#define MISL_TILE_CELL_HFLIP (1u << 1)
#define MISL_TILE_CELL_VFLIP (1u << 2)
#define MISL_TILE_CELL_ROT90 (1u << 3)

// Wall-clock snapshot.
#define MISL_CLOCK_ADDR (MISL_MMIO_BASE + 8u * MISL_MMIO_STRIDE)
#define MISL_CLOCK_BYTES 0x1000u

// Legacy default-program capacity bound, not a general file-format limit.
#define MISL_ASSET_ROM_MAX (MISL_SHARED_TOP - MISL_MEM_SIZE)

typedef struct {
    uint32_t id;
    uint32_t base;  // guest address
    uint32_t size;  // bytes
    uint32_t flags; // MISL_REGION_READ / _WRITE / _EXEC
} MislRegion;

typedef struct {
    uint32_t magic;      // MISL_DESC_MAGIC
    uint32_t abiVersion; // same value MISL_SYS_ABI_DECLARE accepts
    uint32_t descBytes;  // Descriptor size in bytes, as published by the host.
    uint32_t regionCount;
    MislRegion region[MISL_DESC_MAX_REGIONS];
} MislDescriptor;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(MislRegion) == 16u, "MislRegion layout changed");
_Static_assert(sizeof(MislDescriptor) == 528u, "MislDescriptor layout changed");
_Static_assert(sizeof(MislDescriptor) <= 0x1000u, "Descriptor exceeds its page");
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(MISL_PIXIE_RAM_ADDR >= MISL_ENGINE_ZONE,
               "pixie RAM must sit in the engine zone, above the shared space");
_Static_assert((uint64_t)MISL_PIXIE_RAM_ADDR + MISL_PIXIE_RAM_SLOT <= MISL_TILE_MAP_ADDR,
               "pixie RAM's slot runs into the tile cell maps");
_Static_assert((uint64_t)MISL_TILE_MAP_ADDR + MISL_TILE_MAP_SPAN <= MISL_MMIO_BASE,
               "the tile cell maps run into the mapped window");
_Static_assert((uint64_t)MISL_MMIO_BASE + MISL_MMIO_BYTES <= MISL_ROM_ADDR,
               "the system ROM must sit outside the writable window, or the window answers first");
_Static_assert((uint64_t)MISL_ROM_ADDR + MISL_ROM_BYTES <= 0x100000000ull,
               "the system ROM runs off the top of the address space");
_Static_assert(MISL_MEM_SIZE <= MISL_SHARED_TOP, "the shared zone cannot hold even the default program");
#endif
