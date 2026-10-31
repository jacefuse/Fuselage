#ifndef GDMF_TILES_BITMAPS_H
#define GDMF_TILES_BITMAPS_H

#include "gdmf_tiles_state_private.h"

#define GDMF_TILE_BITMAP_PIXELS (MAX_TILE_WIDTH * MAX_TILE_HEIGHT)
#define GDMF_TILE_BITMAP_PACKED_BYTES ((GDMF_TILE_BITMAP_PIXELS + 1) / 2)

// Host bitmap ownership, serialized with upload/probe/render users.
// Reset after adopting host dimensions; false preserves existing registration.
// No GPU work or resource release. SEE: (GDMF_TILES.md, Bitmap ownership)
bool gdmf_tiles_bitmaps_reset(uint8_t layer, uint16_t tile_count);
void gdmf_tiles_bitmaps_clear(uint8_t layer);
uint16_t gdmf_tiles_bitmap_count(uint8_t layer);
// Borrowed unpacked pixels, or NULL if invalid/unregistered. Read only the
// configured tileWidth * tileHeight pixels; the fixed slot stride is internal.
const unsigned char* gdmf_tiles_bitmap_data(uint8_t layer, int tile_id);
bool gdmf_tiles_bitmap_dirty(uint8_t layer, int tile_id);
// Clear only after recording a full copy; this does not prove submission.
void gdmf_tiles_bitmap_mark_uploaded(uint8_t layer, int tile_id);
void gdmf_tiles_bitmaps_mark_dirty(uint8_t layer);

#endif
