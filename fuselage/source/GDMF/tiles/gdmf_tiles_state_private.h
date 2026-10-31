#ifndef GDMF_TILES_STATE_PRIVATE_H
#define GDMF_TILES_STATE_PRIVATE_H

#include "gdmf_tiles.h"
#include <stddef.h>

// Borrowed slots, including uninitialized ones; NULL for an invalid index.
// State access is serialized with renderer/probe/bridge users.
TileMap* gdmf_tiles_state_get(uint8_t layer);
static inline bool gdmf_tiles_layer_valid(uint8_t layer) {
    const TileMap* map = gdmf_tiles_state_get(layer);
    return map && map->initialized;
}
static inline bool gdmf_tiles_layer_writable(uint8_t layer) {
    const TileMap* map = gdmf_tiles_state_get(layer);
    return map && map->initialized && map->enabled;
}
static inline bool gdmf_tiles_coord_valid(uint8_t layer, uint16_t x, uint16_t y) {
    const TileMap* map = gdmf_tiles_state_get(layer);
    return map && map->initialized && x < map->width && y < map->height;
}

// Backend notification. Successful visible-state changes
// invalidate every relevant image slot. SEE: (GDMF_TILES.md, Host state ownership)
void gdmf_tiles_backend_mark_dirty(uint8_t layer);

// Internal host reset, not public InitTileLayer. The lifecycle owner must resolve
// old GPU references and adopt replacement resources before publishing a layer.
// False leaves the slot unchanged. Canvas dimensions are captured together.
bool gdmf_tiles_state_initialize(uint8_t layer, uint16_t map_width, uint16_t map_height, uint16_t tile_width,
                                 uint16_t tile_height, float scale, int canvas_width, int canvas_height);
// Excluded users; clears host state only, without releasing GPU resources.
void gdmf_tiles_state_clear(uint8_t layer);

#endif
