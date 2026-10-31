#pragma once
#include "misl_guest_memory.h"

// Tile ecalls and mapped bitmap/cell addressing.
// SEE: (MISL_GUEST.md, Sprites and tiles)

static inline unsigned char* GetTileBitmapSlot(uint8_t layer, int bitmap_id) {
    unsigned tile_width = (unsigned)MISL_GUEST_CALL(MISL_TILE_GET_TILE_WIDTH, layer, 0, 0, 0, 0, 0, 0);
    unsigned tile_height = (unsigned)MISL_GUEST_CALL(MISL_TILE_GET_TILE_HEIGHT, layer, 0, 0, 0, 0, 0, 0);
    unsigned slot_bytes = (tile_width * tile_height + 1u) / 2u;
    if (!slot_bytes) {
        return (unsigned char*)0;
    }
    return (unsigned char*)(MISL_TILE_LAYER_ADDR(layer) + (unsigned)bitmap_id * slot_bytes);
}

static inline MislTileCell (*GetTileCells(uint8_t layer))
    [MISL_TILE_MAP_HEIGHT] { return (MislTileCell(*)[MISL_TILE_MAP_HEIGHT])MISL_TILE_MAP_OF(layer); }

static inline bool InitTileLayer(uint8_t layer, uint16_t map_width, uint16_t map_height, uint16_t tile_width,
                                 uint16_t tile_height, uint16_t tile_count, float scale) {
    return MISL_GUEST_CALL(MISL_TILE_INIT, layer, map_width, map_height, tile_width, tile_height, tile_count,
                           misl_guest_float_bits(scale)) != 0;
}

static inline bool ReleaseTileLayer(uint8_t layer) {
    return MISL_GUEST_CALL(MISL_TILE_RELEASE, layer, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool UploadTileBitmap(uint8_t layer, int tile_id, const unsigned char* bitmap) {
    return MISL_GUEST_CALL(MISL_TILE_UPLOAD_BITMAP, layer, tile_id, misl_guest_address(bitmap), 0, 0, 0, 0) !=
           0;
}

static inline bool GetTileLayerVisible(uint8_t layer) {
    return MISL_GUEST_CALL(MISL_TILE_GET_VISIBLE, layer, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool SetTileLayerVisible(uint8_t layer, bool visible) {
    return MISL_GUEST_CALL(MISL_TILE_SET_VISIBLE, layer, visible, 0, 0, 0, 0, 0) != 0;
}

static inline bool ToggleTileLayerVisible(uint8_t layer) {
    return MISL_GUEST_CALL(MISL_TILE_TOGGLE_VISIBLE, layer, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool GetTileLayerEnabled(uint8_t layer) {
    return MISL_GUEST_CALL(MISL_TILE_GET_ENABLED, layer, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool SetTileLayerEnabled(uint8_t layer, bool enabled) {
    return MISL_GUEST_CALL(MISL_TILE_SET_ENABLED, layer, enabled, 0, 0, 0, 0, 0) != 0;
}

static inline bool ToggleTileLayerEnabled(uint8_t layer) {
    return MISL_GUEST_CALL(MISL_TILE_TOGGLE_ENABLED, layer, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool PlaceTile(uint8_t layer, uint16_t x, uint16_t y, uint16_t tile_id) {
    return MISL_GUEST_CALL(MISL_TILE_PLACE, layer, x, y, tile_id, 0, 0, 0) != 0;
}

static inline bool FillTileLayer(uint8_t layer, uint16_t tile_id) {
    return MISL_GUEST_CALL(MISL_TILE_FILL, layer, tile_id, 0, 0, 0, 0, 0) != 0;
}

static inline bool FillTileRow(uint8_t layer, uint16_t map_y, uint16_t tile_id) {
    return MISL_GUEST_CALL(MISL_TILE_FILL_ROW, layer, map_y, tile_id, 0, 0, 0, 0) != 0;
}

static inline bool FillTileColumn(uint8_t layer, uint16_t map_x, uint16_t tile_id) {
    return MISL_GUEST_CALL(MISL_TILE_FILL_COLUMN, layer, map_x, tile_id, 0, 0, 0, 0) != 0;
}

static inline bool FillTileRegion(uint8_t layer, uint16_t map_x, uint16_t map_y, uint16_t width,
                                  uint16_t height, uint16_t tile_id) {
    return MISL_GUEST_CALL(MISL_TILE_FILL_REGION, layer, map_x, map_y, width, height, tile_id, 0) != 0;
}

static inline uint16_t GetTileLayerTileWidth(uint8_t layer) {
    return (uint16_t)MISL_GUEST_CALL(MISL_TILE_GET_TILE_WIDTH, layer, 0, 0, 0, 0, 0, 0);
}

static inline uint16_t GetTileLayerTileHeight(uint8_t layer) {
    return (uint16_t)MISL_GUEST_CALL(MISL_TILE_GET_TILE_HEIGHT, layer, 0, 0, 0, 0, 0, 0);
}

static inline bool CheckTilePointCollision(uint8_t layer, float canvas_x, float canvas_y) {
    return MISL_GUEST_CALL(MISL_TILE_CHECK_POINT_COLLISION, layer, misl_guest_float_bits(canvas_x),
                           misl_guest_float_bits(canvas_y), 0, 0, 0, 0) != 0;
}

static inline bool SetTileMapRotation(uint8_t layer, float radians) {
    return MISL_GUEST_CALL(MISL_TILE_SET_MAP_ROTATION, layer, misl_guest_float_bits(radians), 0, 0, 0, 0,
                           0) != 0;
}

static inline float GetTileMapRotation(uint8_t layer) {
    return misl_guest_bits_float(MISL_GUEST_CALL(MISL_TILE_GET_MAP_ROTATION, layer, 0, 0, 0, 0, 0, 0));
}

static inline bool SetTileMapPivot(uint8_t layer, float x, float y) {
    return MISL_GUEST_CALL(MISL_TILE_SET_MAP_PIVOT, layer, misl_guest_float_bits(x), misl_guest_float_bits(y),
                           0, 0, 0, 0) != 0;
}

static inline void GetTileMapPivot(uint8_t layer, float* x, float* y) {
    if (x) {
        *x = misl_guest_bits_float(MISL_GUEST_CALL(MISL_TILE_GET_MAP_PIVOT_X, layer, 0, 0, 0, 0, 0, 0));
    }
    if (y) {
        *y = misl_guest_bits_float(MISL_GUEST_CALL(MISL_TILE_GET_MAP_PIVOT_Y, layer, 0, 0, 0, 0, 0, 0));
    }
}

static inline bool SetTileCellPalette(uint8_t layer, uint16_t x, uint16_t y, uint8_t palette) {
    return MISL_GUEST_CALL(MISL_TILE_SET_CELL_PALETTE, layer, x, y, palette, 0, 0, 0) != 0;
}

static inline bool SetTileLayerPalette(uint8_t layer, uint8_t palette) {
    return MISL_GUEST_CALL(MISL_TILE_SET_PALETTE, layer, palette, 0, 0, 0, 0, 0) != 0;
}

static inline bool SetTileCellTransparency(uint8_t layer, uint16_t x, uint16_t y, uint8_t opacity) {
    return MISL_GUEST_CALL(MISL_TILE_SET_CELL_TRANSP, layer, x, y, opacity, 0, 0, 0) != 0;
}

static inline bool SetTileLayerTransparency(uint8_t layer, uint8_t opacity) {
    return MISL_GUEST_CALL(MISL_TILE_SET_TRANSP, layer, opacity, 0, 0, 0, 0, 0) != 0;
}

static inline bool SetTileCellShowZero(uint8_t layer, uint16_t x, uint16_t y, bool show_zero) {
    return MISL_GUEST_CALL(MISL_TILE_SET_CELL_SHOWZERO, layer, x, y, show_zero, 0, 0, 0) != 0;
}

static inline bool SetTileLayerShowZero(uint8_t layer, bool show_zero) {
    return MISL_GUEST_CALL(MISL_TILE_SET_SHOWZERO, layer, show_zero, 0, 0, 0, 0, 0) != 0;
}

static inline bool SetTileCellFlipX(uint8_t layer, uint16_t x, uint16_t y, bool mirrored) {
    return MISL_GUEST_CALL(MISL_TILE_SET_CELL_FLIP_X, layer, x, y, mirrored, 0, 0, 0) != 0;
}

static inline bool SetTileCellFlipY(uint8_t layer, uint16_t x, uint16_t y, bool mirrored) {
    return MISL_GUEST_CALL(MISL_TILE_SET_CELL_FLIP_Y, layer, x, y, mirrored, 0, 0, 0) != 0;
}

static inline bool SetTileCellRotation(uint8_t layer, uint16_t x, uint16_t y, bool quarter_turn) {
    return MISL_GUEST_CALL(MISL_TILE_SET_ROTATION, layer, x, y, quarter_turn, 0, 0, 0) != 0;
}

static inline bool GetTileCellRotation(uint8_t layer, uint16_t x, uint16_t y) {
    return MISL_GUEST_CALL(MISL_TILE_GET_ROTATION, layer, x, y, 0, 0, 0, 0) != 0;
}

static inline bool SetTileCellCollision(uint8_t layer, uint16_t x, uint16_t y, bool enabled) {
    return MISL_GUEST_CALL(MISL_TILE_SET_CELL_COLLISION, layer, x, y, enabled, 0, 0, 0) != 0;
}

static inline bool SetTileMapWrapX(uint8_t layer, bool wrap) {
    return MISL_GUEST_CALL(MISL_TILE_SET_WRAP_X, layer, wrap, 0, 0, 0, 0, 0) != 0;
}

static inline bool SetTileMapWrapY(uint8_t layer, bool wrap) {
    return MISL_GUEST_CALL(MISL_TILE_SET_WRAP_Y, layer, wrap, 0, 0, 0, 0, 0) != 0;
}

static inline bool SetTileLayerScale(uint8_t layer, float scale) {
    return MISL_GUEST_CALL(MISL_TILE_SET_SCALE, layer, misl_guest_float_bits(scale), 0, 0, 0, 0, 0) != 0;
}

static inline bool SetTileViewport(uint8_t layer, float x, float y, float width, float height) {
    return MISL_GUEST_CALL(MISL_TILE_SET_VIEWPORT, layer, misl_guest_float_bits(x), misl_guest_float_bits(y),
                           misl_guest_float_bits(width), misl_guest_float_bits(height), 0, 0) != 0;
}

static inline bool SetTileLayerPriority(uint8_t layer, unsigned char priority) {
    return MISL_GUEST_CALL(MISL_TILE_SET_PRIORITY, layer, priority, 0, 0, 0, 0, 0) != 0;
}

static inline uint8_t GetTileLayerPriority(uint8_t layer) {
    return (uint8_t)MISL_GUEST_CALL(MISL_TILE_GET_PRIORITY, layer, 0, 0, 0, 0, 0, 0);
}

static inline uint16_t GetTile(uint8_t layer, uint16_t x, uint16_t y) {
    return (uint16_t)MISL_GUEST_CALL(MISL_TILE_GET_TILE, layer, x, y, 0, 0, 0, 0);
}

static inline uint8_t GetTileCellPalette(uint8_t layer, uint16_t x, uint16_t y) {
    return (uint8_t)MISL_GUEST_CALL(MISL_TILE_GET_CELL_PALETTE, layer, x, y, 0, 0, 0, 0);
}

static inline uint8_t GetTileCellTransparency(uint8_t layer, uint16_t x, uint16_t y) {
    return (uint8_t)MISL_GUEST_CALL(MISL_TILE_GET_CELL_TRANSP, layer, x, y, 0, 0, 0, 0);
}

static inline bool GetTileCellShowZero(uint8_t layer, uint16_t x, uint16_t y) {
    return MISL_GUEST_CALL(MISL_TILE_GET_CELL_SHOWZERO, layer, x, y, 0, 0, 0, 0) != 0;
}

static inline bool GetTileCellCollision(uint8_t layer, uint16_t x, uint16_t y) {
    return MISL_GUEST_CALL(MISL_TILE_GET_CELL_COLLIS, layer, x, y, 0, 0, 0, 0) != 0;
}

static inline bool SetTileCellTag(uint8_t layer, uint16_t x, uint16_t y, uint32_t tag) {
    return MISL_GUEST_CALL(MISL_TILE_SET_CELL_TAG, layer, x, y, tag, 0, 0, 0) != 0;
}

static inline uint32_t GetTileCellTag(uint8_t layer, uint16_t x, uint16_t y) {
    return (uint32_t)MISL_GUEST_CALL(MISL_TILE_GET_CELL_TAG, layer, x, y, 0, 0, 0, 0);
}

static inline float GetTileLayerScale(uint8_t layer) {
    return misl_guest_bits_float(MISL_GUEST_CALL(MISL_TILE_GET_SCALE, layer, 0, 0, 0, 0, 0, 0));
}

static inline bool GetTileCellFlipX(uint8_t layer, uint16_t x, uint16_t y) {
    return MISL_GUEST_CALL(MISL_TILE_GET_CELL_FLIP_X, layer, x, y, 0, 0, 0, 0) != 0;
}

static inline bool GetTileCellFlipY(uint8_t layer, uint16_t x, uint16_t y) {
    return MISL_GUEST_CALL(MISL_TILE_GET_CELL_FLIP_Y, layer, x, y, 0, 0, 0, 0) != 0;
}

static inline bool GetTileMapWrapX(uint8_t layer) {
    return MISL_GUEST_CALL(MISL_TILE_GET_WRAP_X, layer, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool GetTileMapWrapY(uint8_t layer) {
    return MISL_GUEST_CALL(MISL_TILE_GET_WRAP_Y, layer, 0, 0, 0, 0, 0, 0) != 0;
}

static inline void GetTileViewport(uint8_t layer, float* x, float* y, float* width, float* height) {
    MISL_GUEST_CALL(MISL_TILE_GET_VIEWPORT, layer, misl_guest_address(x), misl_guest_address(y),
                    misl_guest_address(width), misl_guest_address(height), 0, 0);
}

static inline void GetTileMapOffset(uint8_t layer, double* x, double* y) {
    MISL_GUEST_CALL(MISL_TILE_GET_MAP_OFFSET, layer, misl_guest_address(x), misl_guest_address(y), 0, 0, 0,
                    0);
}

static inline void GetTileCellOffset(uint8_t layer, uint16_t map_x, uint16_t map_y, int8_t* offset_x,
                                     int8_t* offset_y) {
    MISL_GUEST_CALL(MISL_TILE_GET_CELL_OFFSET, layer, map_x, map_y, misl_guest_address(offset_x),
                    misl_guest_address(offset_y), 0, 0);
}

static inline bool ScrollTileMap(uint8_t layer, double offset_x, double offset_y) {
    return MISL_GUEST_CALL(MISL_TILE_SCROLL, layer, misl_guest_double_low(offset_x),
                           misl_guest_double_high(offset_x), misl_guest_double_low(offset_y),
                           misl_guest_double_high(offset_y), 0, 0) != 0;
}

static inline bool SetTileMapOffset(uint8_t layer, double x, double y) {
    return MISL_GUEST_CALL(MISL_TILE_SET_MAP_OFFSET, layer, misl_guest_double_low(x),
                           misl_guest_double_high(x), misl_guest_double_low(y), misl_guest_double_high(y), 0,
                           0) != 0;
}

static inline bool SetTileCellOffset(uint8_t layer, uint16_t map_x, uint16_t map_y, int8_t offset_x,
                                     int8_t offset_y) {
    return MISL_GUEST_CALL(MISL_TILE_SET_CELL_OFFSET, layer, map_x, map_y, offset_x, offset_y, 0, 0) != 0;
}

static inline bool SetTileLayerCollidableColors(uint8_t layer, uint16_t mask) {
    return MISL_GUEST_CALL(MISL_TILE_SET_COLLIDABLE, layer, mask, 0, 0, 0, 0, 0) != 0;
}

static inline uint16_t GetTileLayerCollidableColors(uint8_t layer) {
    return (uint16_t)MISL_GUEST_CALL(MISL_TILE_GET_COLLIDABLE, layer, 0, 0, 0, 0, 0, 0);
}

static inline bool UploadTileTestPattern(uint8_t layer, int tile_id) {
    return MISL_GUEST_CALL(MISL_TILE_TEST_PATTERN, layer, tile_id, 0, 0, 0, 0, 0) != 0;
}

static inline bool UploadTileBoxPattern(uint8_t layer, int tile_id) {
    return MISL_GUEST_CALL(MISL_TILE_BOX_PATTERN, layer, tile_id, 0, 0, 0, 0, 0) != 0;
}

static inline void ToggleTileAtlasView(uint8_t layer) {
    MISL_GUEST_CALL(MISL_TILE_TOGGLE_ATLAS, layer, 0, 0, 0, 0, 0, 0);
}

static inline bool GetTileAtlasViewActive(uint8_t layer) {
    return MISL_GUEST_CALL(MISL_TILE_GET_ATLAS_ACTIVE, layer, 0, 0, 0, 0, 0, 0) != 0;
}
