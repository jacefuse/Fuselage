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
// The same lookup resolved once for a stable query: count is 0 when the layer has
// no usable bank. Slot n starts at pixels + n * GDMF_TILE_BITMAP_PIXELS.
typedef struct {
    const bool* registered;
    const unsigned char* pixels;
    uint16_t count;
} gdmf_tiles_bitmap_view;
void gdmf_tiles_bitmap_view_get(uint8_t layer, gdmf_tiles_bitmap_view* view);
static inline const unsigned char* gdmf_tiles_bitmap_view_data(const gdmf_tiles_bitmap_view* view,
                                                               int tile_id) {
    return tile_id >= 0 && tile_id < view->count && view->registered[tile_id]
               ? view->pixels + (size_t)tile_id * GDMF_TILE_BITMAP_PIXELS
               : NULL;
}
bool gdmf_tiles_bitmap_dirty(uint8_t layer, int tile_id);
// Clear only after recording a full copy; this does not prove submission.
void gdmf_tiles_bitmap_mark_uploaded(uint8_t layer, int tile_id);
void gdmf_tiles_bitmaps_mark_dirty(uint8_t layer);

#endif
