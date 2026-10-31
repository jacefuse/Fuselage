#pragma once
#include "misl_mmio_map.h"

// Shared graphics records. Host integration must check mirrors against GDMF.
// SEE: (MISL_ABI_MMIO.md, Graphics and submission results)

typedef struct {
    uint32_t executed;         // records interpreted by the last submit
    uint32_t rejected;         // of those, how many IssuePixieCommand refused
    uint32_t firstRejectIndex; // MISL_PIXIE_NO_REJECT when none were
    uint32_t firstRejectOpcode;
    uint32_t firstRejectPixie;
    uint32_t submits; // MISL_PIX_SUBMIT calls since this ROAP loaded
    uint32_t totalExecuted;
    uint32_t totalRejected;
} MislPixieListStatus;

typedef struct {
    MislPixieListStatus pixieList;
} MislStatus;

typedef struct {
    uint8_t pixie;  // target pixie id
    uint8_t opcode; // PixieOpcode; the same numbers MISL_PIX_COMMAND takes
    uint16_t flags;
    uint32_t args[4];
} MislPixieCmd;

typedef struct {
    uint16_t tileTypeID; // slot in the layer's bitmap pool; past tileCount = not drawn
    uint8_t flags;       // MISL_TILE_CELL_* bitmask
    int8_t offsetX;      // draw-time nudge in pixels
    int8_t offsetY;
    uint8_t palette;      // which of the 256 16-color palettes
    uint8_t transparency; // 0 invisible .. 255 opaque
    uint8_t showzero;     // nonzero: palette index 0 draws; zero: it is transparent
    uint32_t tag;         // yours; the engine never reads it
} MislTileCell;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(MislPixieListStatus) == 32u, "Pixie result layout changed");
_Static_assert(sizeof(MislStatus) <= MISL_STATUS_BYTES, "Status exceeds its page");
_Static_assert(sizeof(MislPixieCmd) == MISL_PIXIE_CMD_BYTES,
               "MislPixieCmd must stay 20 bytes; the MMIO map states this width");
_Static_assert(MISL_PIXIE_LIST_BYTES <= MISL_MMIO_STRIDE, "the pixie display list no longer fits its region");
_Static_assert(MISL_PIXIE_RAM_BYTES <= MISL_PIXIE_RAM_SLOT,
               "pixie RAM has outgrown its slot; raise MISL_PIXIE_RAM_SLOT, or move it");
_Static_assert(sizeof(MislTileCell) == MISL_TILE_CELL_BYTES,
               "MislTileCell must stay 12 bytes; the MMIO map states this width");
_Static_assert(MISL_TILE_MAP_BYTES <= MISL_TILE_MAP_STRIDE, "a tile map no longer fits its bank stride");
_Static_assert((uint64_t)MISL_TILE_MAP_ADDR + MISL_TILE_MAP_SPAN == MISL_MMIO_BASE,
               "the tile cell maps must end exactly at the mapped window");
_Static_assert(MISL_TILE_MAP_ADDR >= MISL_MEM_SIZE, "the tile cell maps overlap program memory");
#endif
