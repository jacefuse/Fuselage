#ifndef GDMF_TILES_H
#define GDMF_TILES_H

// Independent indexed tile maps. SEE: (GDMF_TILES.md, Layers and cells)
#include <stdbool.h>
#include <stdint.h>

#define GDMF_TILES_VERSION "0.5.2026103001 ENTERON"

#define MAX_TILE_LAYERS 16
#define MAX_TILES 1024
#define MAX_MAP_WIDTH 512
#define MAX_MAP_HEIGHT 512
#define MAX_TILE_WIDTH 32
#define MAX_TILE_HEIGHT 32

#define TILE_FLAG_COLLISION (1u << 0)
#define TILE_FLAG_HFLIP (1u << 1)
#define TILE_FLAG_VFLIP (1u << 2)
#define TILE_FLAG_ROT90 (1u << 3)

// Public layout retained for bridge consumers; use setters for native state.
typedef struct {
    uint16_t tileTypeID;
    uint8_t flags;
    int8_t offsetX;
    int8_t offsetY;
    uint8_t palette;
    uint8_t transparency;
    bool showzero;
    uint32_t tag;
} TileLocation; // Cell storage is indexed [map_x][map_y].

typedef struct {
    bool initialized;
    bool visible;
    bool enabled;
    bool wrapX;
    bool wrapY;
    double mapOffsetX;
    double mapOffsetY;
    uint16_t width;
    uint16_t height;
    uint16_t tileWidth;
    uint16_t tileHeight;
    float viewportX;
    float viewportY;

    float viewportWidth;
    float viewportHeight;
    float scale;
    float rotation; // Clockwise radians.

    float pivotX;
    float pivotY;

    uint8_t priority;

    uint16_t collidableColors;
    TileLocation location[MAX_MAP_WIDTH][MAX_MAP_HEIGHT];
} TileMap;

// One layer; false for invalid limits, atlas setup or checked lifecycle failure.
// SEE: (GDMF_TILES.md, Lifecycle)
bool InitTileLayer(uint8_t layer, uint16_t map_width, uint16_t map_height, uint16_t tile_width,
                   uint16_t tile_height, uint16_t tile_count, float scale);

// Engine-coordinated teardown; retains resources if quiescence is unconfirmed.
void ShutdownTiles(void);

// False for invalid/uninitialized layers or failed retirement; no reuse on failure.
bool ReleaseTileLayer(uint8_t layer);

// Borrowed writable guest-bridge storage, NULL if invalid/uninitialized.
// Native applications use setters. SEE: (GDMF_TILES.md, Guest cell mapping)
TileLocation* GetTileLayerCellsPtr(uint8_t layer);

// Marks geometry for rebuild after mapped writes; false if layer is invalid.
bool MarkTileLayerDirty(uint8_t layer);

// Copies packed pixels, high nibble first; NULL selects the test pattern.
// True means host acceptance. SEE: (GDMF_TILES.md, Bitmap registration)
bool UploadTileBitmap(uint8_t layer, int tile_id, const unsigned char* bitmap);

// Visibility/enablement setters and toggles return the resulting state,
// not success. Visibility changes are allowed while disabled.
// SEE: (GDMF_TILES.md, Return values and gating)
bool GetTileLayerVisible(uint8_t layer);
bool SetTileLayerVisible(uint8_t layer, bool visible);
bool ToggleTileLayerVisible(uint8_t layer);

bool GetTileLayerEnabled(uint8_t layer);
bool SetTileLayerEnabled(uint8_t layer, bool enabled);
bool ToggleTileLayerEnabled(uint8_t layer);

// Placement and fills change only tile IDs; other cell attributes remain.
// Nonempty regions must fit completely. Tile IDs need not be registered.
// SEE: (GDMF_TILES.md, Cells)
bool PlaceTile(uint8_t layer, uint16_t x, uint16_t y, uint16_t tile_id);

bool FillTileLayer(uint8_t layer, uint16_t tile_id);

bool FillTileRow(uint8_t layer, uint16_t map_y, uint16_t tile_id);
bool FillTileColumn(uint8_t layer, uint16_t map_x, uint16_t tile_id);
bool FillTileRegion(uint8_t layer, uint16_t map_x, uint16_t map_y, uint16_t width, uint16_t height,
                    uint16_t tile_id);

// Zero for an invalid layer/cell, indistinguishable from tile ID zero.
uint16_t GetTile(uint8_t layer, uint16_t x, uint16_t y);

// Visible cell/layer changes below require an enabled layer.
// Getters return zero/false for invalid layers/cells.
// SEE: (GDMF_TILES.md, Return values and gating)
bool SetTileCellPalette(uint8_t layer, uint16_t x, uint16_t y, uint8_t palette);
uint8_t GetTileCellPalette(uint8_t layer, uint16_t x, uint16_t y);

bool SetTileLayerPalette(uint8_t layer, uint8_t palette);

// Despite the name, this is opacity: 0 invisible, 255 fully opaque.
bool SetTileCellTransparency(uint8_t layer, uint16_t x, uint16_t y, uint8_t opacity);
uint8_t GetTileCellTransparency(uint8_t layer, uint16_t x, uint16_t y);

bool SetTileLayerTransparency(uint8_t layer, uint8_t opacity);

bool SetTileCellShowZero(uint8_t layer, uint16_t x, uint16_t y, bool show_zero);
bool GetTileCellShowZero(uint8_t layer, uint16_t x, uint16_t y);

bool SetTileLayerShowZero(uint8_t layer, bool show_zero);

// Mirrors apply before the clockwise quarter turn.
// SEE: (GDMF_TILES.md, Orientation and transforms)
bool SetTileCellFlipX(uint8_t layer, uint16_t x, uint16_t y, bool mirrored);
bool SetTileCellFlipY(uint8_t layer, uint16_t x, uint16_t y, bool mirrored);
bool GetTileCellFlipX(uint8_t layer, uint16_t x, uint16_t y);
bool GetTileCellFlipY(uint8_t layer, uint16_t x, uint16_t y);

// A clockwise quarter turn; nonsquare tiles swap extents about the center.
bool SetTileCellRotation(uint8_t layer, uint16_t x, uint16_t y, bool quarter_turn);

bool GetTileCellRotation(uint8_t layer, uint16_t x, uint16_t y);

// Collision and tag changes also work while disabled; invalid cells fail.
bool SetTileCellCollision(uint8_t layer, uint16_t x, uint16_t y, bool enabled);
bool GetTileCellCollision(uint8_t layer, uint16_t x, uint16_t y);

typedef struct {
    bool onMap;
    uint16_t mapX, mapY;
    bool collidable;
    bool onTile;
    uint8_t paletteIndex;
} TilePointInfo;
// True when the point resolves to a map cell, even if onTile is false.
// NULL output fails; other failures clear the result. Unregistered bitmaps
// report onTile false. SEE: (GDMF_TILES.md, Point queries)
bool GetTilePointInfo(uint8_t layer, float canvas_x, float canvas_y, TilePointInfo* output);

// Requires a collidable cell and a sampled palette index in the layer mask.
bool CheckTilePointCollision(uint8_t layer, float canvas_x, float canvas_y);

bool SetTileCellTag(uint8_t layer, uint16_t x, uint16_t y, uint32_t tag);
uint32_t GetTileCellTag(uint8_t layer, uint16_t x, uint16_t y);

// Layer transform changes require enablement; scalar getters return zero/false
// for invalid layers. Optional output pointers receive zero.
// SEE: (GDMF_TILES.md, Orientation and transforms)
bool SetTileMapWrapX(uint8_t layer, bool wrap);
bool SetTileMapWrapY(uint8_t layer, bool wrap);
bool GetTileMapWrapX(uint8_t layer);
bool GetTileMapWrapY(uint8_t layer);
// Rejects scale <= 0; nonfinite input validation remains incomplete.
bool SetTileLayerScale(uint8_t layer, float scale);
float GetTileLayerScale(uint8_t layer);

// Fixed dimensions selected at initialization; zero for invalid layers.
uint16_t GetTileLayerTileWidth(uint8_t layer);
uint16_t GetTileLayerTileHeight(uint8_t layer);

// Lower values draw later; equal-priority tile layers draw lower indices last.
bool SetTileLayerPriority(uint8_t layer, uint8_t priority);
uint8_t GetTileLayerPriority(uint8_t layer);

// Canvas coordinates; signed position, negative extents clamped to zero.
// Output pointers in GetTileViewport are optional.
bool SetTileViewport(uint8_t layer, float x, float y, float width, float height);
void GetTileViewport(uint8_t layer, float* x, float* y, float* width, float* height);

// Absolute source-pixel scroll, wrapped or clamped per axis.
// GetTileMapOffset output pointers are optional.
bool SetTileMapOffset(uint8_t layer, double x, double y);
void GetTileMapOffset(uint8_t layer, double* x, double* y);

// Clockwise RADIANS. Sprites use degrees; no shared angular unit is implied.
bool SetTileMapRotation(uint8_t layer, float radians);
float GetTileMapRotation(uint8_t layer);

// Canvas-pixel pivot relative to the viewport top-left; resizing does not recenter.
bool SetTileMapPivot(uint8_t layer, float x, float y);
void GetTileMapPivot(uint8_t layer, float* x, float* y);

// Signed canvas-pixel nudge per cell; output pointers are optional.
bool SetTileCellOffset(uint8_t layer, uint16_t map_x, uint16_t map_y, int8_t offset_x, int8_t offset_y);
void GetTileCellOffset(uint8_t layer, uint16_t map_x, uint16_t map_y, int8_t* offset_x, int8_t* offset_y);

// One bit per palette index; zero disables color matches. Works while disabled.
bool SetTileLayerCollidableColors(uint8_t layer, uint16_t mask);
uint16_t GetTileLayerCollidableColors(uint8_t layer);

// Equivalent to UploadTileBitmap with a NULL bitmap.
bool UploadTileTestPattern(uint8_t layer, int tile_id);

// Two-pixel index-one border, index-zero interior.
// Supports every configured tile size, including one-pixel dimensions.
// SEE: (GDMF_TILES.md, Bitmap registration)
bool UploadTileBoxPattern(uint8_t layer, int tile_id);

// Diagnostic state flag only; no atlas preview is rendered.
// Valid indices can toggle even when the layer is uninitialized.
void ToggleTileAtlasView(uint8_t layer);
bool GetTileAtlasViewActive(uint8_t layer);

#endif
