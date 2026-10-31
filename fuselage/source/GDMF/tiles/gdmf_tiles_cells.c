// Host cells. SEE: (GDMF_TILES.md, Host state ownership)
#include "gdmf_tiles_state_private.h"

bool PlaceTile(uint8_t layer, uint16_t x, uint16_t y, uint16_t tile_id) {
    if (!gdmf_tiles_layer_writable(layer) || !gdmf_tiles_coord_valid(layer, x, y)) {
        return false;
    }
    gdmf_tiles_state_get(layer)->location[x][y].tileTypeID = tile_id;
    gdmf_tiles_backend_mark_dirty(layer);

    return true;
}

bool FillTileLayer(uint8_t layer, uint16_t tile_id) {
    if (!gdmf_tiles_layer_writable(layer)) {
        return false;
    }
    TileMap* map = gdmf_tiles_state_get(layer);

    for (uint16_t x = 0; x < map->width; x++) {
        for (uint16_t y = 0; y < map->height; y++) {
            map->location[x][y].tileTypeID = tile_id;
        }
    }
    gdmf_tiles_backend_mark_dirty(layer);

    return true;
}

bool FillTileRegion(uint8_t layer, uint16_t map_x, uint16_t map_y, uint16_t width, uint16_t height,
                    uint16_t tile_id) {
    if (!gdmf_tiles_layer_writable(layer)) {
        return false;
    }
    TileMap* map = gdmf_tiles_state_get(layer);
    if (width == 0 || height == 0) {
        return false;
    }
    if ((uint32_t)map_x + width > map->width || (uint32_t)map_y + height > map->height) {
        return false;
    }

    for (uint16_t x = map_x; x < map_x + width; x++) {
        for (uint16_t y = map_y; y < map_y + height; y++) {
            map->location[x][y].tileTypeID = tile_id;
        }
    }
    gdmf_tiles_backend_mark_dirty(layer);

    return true;
}

bool FillTileRow(uint8_t layer, uint16_t map_y, uint16_t tile_id) {
    if (!gdmf_tiles_layer_writable(layer)) {
        return false;
    }

    return FillTileRegion(layer, 0, map_y, gdmf_tiles_state_get(layer)->width, 1, tile_id);
}

bool FillTileColumn(uint8_t layer, uint16_t map_x, uint16_t tile_id) {
    if (!gdmf_tiles_layer_writable(layer)) {
        return false;
    }

    return FillTileRegion(layer, map_x, 0, 1, gdmf_tiles_state_get(layer)->height, tile_id);
}

uint16_t GetTile(uint8_t layer, uint16_t x, uint16_t y) {
    if (!gdmf_tiles_layer_valid(layer) || !gdmf_tiles_coord_valid(layer, x, y)) {
        return 0;
    }

    return gdmf_tiles_state_get(layer)->location[x][y].tileTypeID;
}

bool SetTileCellPalette(uint8_t layer, uint16_t x, uint16_t y, uint8_t palette) {
    if (!gdmf_tiles_layer_writable(layer) || !gdmf_tiles_coord_valid(layer, x, y)) {
        return false;
    }
    gdmf_tiles_state_get(layer)->location[x][y].palette = palette;
    gdmf_tiles_backend_mark_dirty(layer);

    return true;
}

uint8_t GetTileCellPalette(uint8_t layer, uint16_t x, uint16_t y) {
    if (!gdmf_tiles_layer_valid(layer) || !gdmf_tiles_coord_valid(layer, x, y)) {
        return 0;
    }

    return gdmf_tiles_state_get(layer)->location[x][y].palette;
}

bool SetTileLayerPalette(uint8_t layer, uint8_t palette) {
    if (!gdmf_tiles_layer_writable(layer)) {
        return false;
    }
    TileMap* map = gdmf_tiles_state_get(layer);

    for (uint16_t x = 0; x < map->width; x++) {
        for (uint16_t y = 0; y < map->height; y++) {
            map->location[x][y].palette = palette;
        }
    }
    gdmf_tiles_backend_mark_dirty(layer);

    return true;
}

bool SetTileCellTransparency(uint8_t layer, uint16_t x, uint16_t y, uint8_t opacity) {
    if (!gdmf_tiles_layer_writable(layer) || !gdmf_tiles_coord_valid(layer, x, y)) {
        return false;
    }
    gdmf_tiles_state_get(layer)->location[x][y].transparency = opacity;
    gdmf_tiles_backend_mark_dirty(layer);

    return true;
}

uint8_t GetTileCellTransparency(uint8_t layer, uint16_t x, uint16_t y) {
    if (!gdmf_tiles_layer_valid(layer) || !gdmf_tiles_coord_valid(layer, x, y)) {
        return 0;
    }

    return gdmf_tiles_state_get(layer)->location[x][y].transparency;
}

bool SetTileLayerTransparency(uint8_t layer, uint8_t opacity) {
    if (!gdmf_tiles_layer_writable(layer)) {
        return false;
    }
    TileMap* map = gdmf_tiles_state_get(layer);

    for (uint16_t x = 0; x < map->width; x++) {
        for (uint16_t y = 0; y < map->height; y++) {
            map->location[x][y].transparency = opacity;
        }
    }
    gdmf_tiles_backend_mark_dirty(layer);

    return true;
}

bool SetTileCellShowZero(uint8_t layer, uint16_t x, uint16_t y, bool show_zero) {
    if (!gdmf_tiles_layer_writable(layer) || !gdmf_tiles_coord_valid(layer, x, y)) {
        return false;
    }
    gdmf_tiles_state_get(layer)->location[x][y].showzero = show_zero;
    gdmf_tiles_backend_mark_dirty(layer);

    return true;
}

bool GetTileCellShowZero(uint8_t layer, uint16_t x, uint16_t y) {
    if (!gdmf_tiles_layer_valid(layer) || !gdmf_tiles_coord_valid(layer, x, y)) {
        return false;
    }

    return gdmf_tiles_state_get(layer)->location[x][y].showzero;
}

bool SetTileLayerShowZero(uint8_t layer, bool show_zero) {
    if (!gdmf_tiles_layer_writable(layer)) {
        return false;
    }
    TileMap* map = gdmf_tiles_state_get(layer);

    for (uint16_t x = 0; x < map->width; x++) {
        for (uint16_t y = 0; y < map->height; y++) {
            map->location[x][y].showzero = show_zero;
        }
    }
    gdmf_tiles_backend_mark_dirty(layer);

    return true;
}

bool SetTileCellFlipX(uint8_t layer, uint16_t x, uint16_t y, bool mirrored) {
    if (!gdmf_tiles_layer_writable(layer) || !gdmf_tiles_coord_valid(layer, x, y)) {
        return false;
    }
    uint8_t* flags = &gdmf_tiles_state_get(layer)->location[x][y].flags;
    if (mirrored) {
        *flags |= (uint8_t)TILE_FLAG_HFLIP;
    } else {
        *flags &= (uint8_t)~TILE_FLAG_HFLIP;
    }
    gdmf_tiles_backend_mark_dirty(layer);

    return true;
}

bool SetTileCellFlipY(uint8_t layer, uint16_t x, uint16_t y, bool mirrored) {
    if (!gdmf_tiles_layer_writable(layer) || !gdmf_tiles_coord_valid(layer, x, y)) {
        return false;
    }
    uint8_t* flags = &gdmf_tiles_state_get(layer)->location[x][y].flags;
    if (mirrored) {
        *flags |= (uint8_t)TILE_FLAG_VFLIP;
    } else {
        *flags &= (uint8_t)~TILE_FLAG_VFLIP;
    }
    gdmf_tiles_backend_mark_dirty(layer);

    return true;
}

bool SetTileCellRotation(uint8_t layer, uint16_t x, uint16_t y, bool quarter_turn) {
    if (!gdmf_tiles_layer_writable(layer) || !gdmf_tiles_coord_valid(layer, x, y)) {
        return false;
    }
    uint8_t* flags = &gdmf_tiles_state_get(layer)->location[x][y].flags;
    if (quarter_turn) {
        *flags |= TILE_FLAG_ROT90;
    } else {
        *flags &= ~TILE_FLAG_ROT90;
    }
    gdmf_tiles_backend_mark_dirty(layer);

    return true;
}

bool GetTileCellRotation(uint8_t layer, uint16_t x, uint16_t y) {
    if (!gdmf_tiles_layer_valid(layer) || !gdmf_tiles_coord_valid(layer, x, y)) {
        return false;
    }

    return (gdmf_tiles_state_get(layer)->location[x][y].flags & TILE_FLAG_ROT90) != 0;
}

bool GetTileCellFlipX(uint8_t layer, uint16_t x, uint16_t y) {
    if (!gdmf_tiles_layer_valid(layer) || !gdmf_tiles_coord_valid(layer, x, y)) {
        return false;
    }

    return (gdmf_tiles_state_get(layer)->location[x][y].flags & TILE_FLAG_HFLIP) != 0;
}

bool GetTileCellFlipY(uint8_t layer, uint16_t x, uint16_t y) {
    if (!gdmf_tiles_layer_valid(layer) || !gdmf_tiles_coord_valid(layer, x, y)) {
        return false;
    }

    return (gdmf_tiles_state_get(layer)->location[x][y].flags & TILE_FLAG_VFLIP) != 0;
}

bool SetTileCellCollision(uint8_t layer, uint16_t x, uint16_t y, bool enabled) {
    if (!gdmf_tiles_layer_valid(layer) || !gdmf_tiles_coord_valid(layer, x, y)) {
        return false;
    }
    if (enabled) {
        gdmf_tiles_state_get(layer)->location[x][y].flags |= TILE_FLAG_COLLISION;
    } else {
        gdmf_tiles_state_get(layer)->location[x][y].flags &= ~TILE_FLAG_COLLISION;
    }

    return true;
}

bool GetTileCellCollision(uint8_t layer, uint16_t x, uint16_t y) {
    if (!gdmf_tiles_layer_valid(layer) || !gdmf_tiles_coord_valid(layer, x, y)) {
        return false;
    }

    return (gdmf_tiles_state_get(layer)->location[x][y].flags & TILE_FLAG_COLLISION) != 0;
}

bool SetTileCellTag(uint8_t layer, uint16_t x, uint16_t y, uint32_t tag) {
    if (!gdmf_tiles_layer_valid(layer) || !gdmf_tiles_coord_valid(layer, x, y)) {
        return false;
    }
    gdmf_tiles_state_get(layer)->location[x][y].tag = tag;

    return true;
}

uint32_t GetTileCellTag(uint8_t layer, uint16_t x, uint16_t y) {
    if (!gdmf_tiles_layer_valid(layer) || !gdmf_tiles_coord_valid(layer, x, y)) {
        return 0;
    }

    return gdmf_tiles_state_get(layer)->location[x][y].tag;
}

bool SetTileCellOffset(uint8_t layer, uint16_t map_x, uint16_t map_y, int8_t offset_x, int8_t offset_y) {
    if (!gdmf_tiles_layer_writable(layer)) {
        return false;
    }
    TileMap* map = gdmf_tiles_state_get(layer);
    if (map_x >= map->width || map_y >= map->height) {
        return false;
    }
    map->location[map_x][map_y].offsetX = offset_x;
    map->location[map_x][map_y].offsetY = offset_y;
    gdmf_tiles_backend_mark_dirty(layer);

    return true;
}

void GetTileCellOffset(uint8_t layer, uint16_t map_x, uint16_t map_y, int8_t* output_x, int8_t* output_y) {
    int8_t offset_x = 0, offset_y = 0;

    if (gdmf_tiles_layer_valid(layer) && gdmf_tiles_coord_valid(layer, map_x, map_y)) {
        offset_x = gdmf_tiles_state_get(layer)->location[map_x][map_y].offsetX;
        offset_y = gdmf_tiles_state_get(layer)->location[map_x][map_y].offsetY;
    }
    if (output_x) {
        *output_x = offset_x;
    }
    if (output_y) {
        *output_y = offset_y;
    }

    return;
}
