// Host slots and bridge access. SEE: (GDMF_TILES.md, Host state ownership)
#include "gdmf_tiles_state_private.h"
#include <string.h>

static TileMap gdmf_tiles_maps[MAX_TILE_LAYERS];

TileMap* gdmf_tiles_state_get(uint8_t layer) {
    return layer < MAX_TILE_LAYERS ? &gdmf_tiles_maps[layer] : NULL;
}
void gdmf_tiles_state_clear(uint8_t layer) {
    TileMap* map = gdmf_tiles_state_get(layer);
    if (map) {
        memset(map, 0, sizeof(*map));
    }
}
bool gdmf_tiles_state_initialize(uint8_t layer, uint16_t map_width, uint16_t map_height, uint16_t tile_width,
                                 uint16_t tile_height, float scale, int canvas_width, int canvas_height) {
    TileMap* map = gdmf_tiles_state_get(layer);
    if (!map || !map_width || map_width > MAX_MAP_WIDTH || !map_height || map_height > MAX_MAP_HEIGHT ||
        !tile_width || tile_width > MAX_TILE_WIDTH || !tile_height || tile_height > MAX_TILE_HEIGHT ||
        scale <= 0.0f) {
        return false;
    }
    memset(map, 0, sizeof(*map));
    map->width = map_width;
    map->height = map_height;
    map->tileWidth = tile_width;
    map->tileHeight = tile_height;
    map->scale = scale;
    map->visible = true;
    map->enabled = true;
    map->viewportWidth = (float)canvas_width;
    map->viewportHeight = (float)canvas_height;
    map->pivotX = map->viewportWidth * 0.5f;
    map->pivotY = map->viewportHeight * 0.5f;
    map->priority = layer;
    for (uint16_t x = 0; x < map_width; ++x) {
        for (uint16_t y = 0; y < map_height; ++y) {
            map->location[x][y].transparency = 255;
        }
    }
    map->initialized = true;
    gdmf_tiles_backend_mark_dirty(layer);
    return true;
}
TileLocation* GetTileLayerCellsPtr(uint8_t layer) {
    return gdmf_tiles_layer_valid(layer) ? &gdmf_tiles_state_get(layer)->location[0][0] : NULL;
}
bool MarkTileLayerDirty(uint8_t layer) {
    if (!gdmf_tiles_layer_valid(layer)) {
        return false;
    }
    gdmf_tiles_backend_mark_dirty(layer);
    return true;
}
