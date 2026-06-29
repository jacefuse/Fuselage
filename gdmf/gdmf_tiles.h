#pragma once

// GDMF Tile Layer
//
// Up to MAX_TILE_LAYERS independent tile maps. Each layer has its own bitmap
// atlas, scroll state, viewport, and map data. Tile types (shape, palette,
// transparency) are registered once; map cells reference a type by ID and add
// per-cell flip and collision opt-in. Colors are never baked in -- palette is
// resolved at render time in the fragment shader from the same Colors buffer
// the sprite layer uploads every frame.
//
// Collision defaults to fully off: nothing at the layer level is collidable,
// and no cell has its collision flag set. Opt in explicitly via
// SetTileLayerCollidableColors and SetTileCellCollision.

#ifndef GDMF_TILES_H
#define GDMF_TILES_H

#include <stdbool.h>
#include <stdint.h>

#define GDMF_TILES_VERSION "0.2.2026062901 BUTTOCKS"

#define MAX_TILE_LAYERS  16    // independent layers
#define MAX_TILES        1024  // tile type slots per layer
#define MAX_MAP_WIDTH    512   // map dimensions in tiles
#define MAX_MAP_HEIGHT   512   // max dimensions may increase
#define MAX_TILE_WIDTH   32    // tile dimensions in pixels
#define MAX_TILE_HEIGHT  32

// Per-cell flags -- packed into TileLocation.flags
#define TILE_FLAG_COLLISION (1u << 0)  // cell participates in collision detection
#define TILE_FLAG_HFLIP     (1u << 1)  // render this cell's tile mirrored horizontally
#define TILE_FLAG_VFLIP     (1u << 2)  // render this cell's tile mirrored vertically

// One tile type: shape and appearance. Registered once per layer via
// UploadTileBitmap; any number of cells may reference the same type by ID.
// Palette and transparency live at type level -- if you need the same shape
// at different transparency levels, register two tile types. This is cheaper
// than 512x512 per-cell transparency arrays (especially as MAX_TILE_LAYERS
// grows to 16).
typedef struct {
    uint8_t palette;       // which of the 256 16-color palettes to apply at render time
    uint8_t transparency;  // overall alpha: 0 = invisible, 255 = fully opaque
    bool    showzero;      // if true, palette index 0 renders; if false, it is transparent
} TileType;

// One cell in the map. Kept small -- this struct lives in a 512x512 array
// per layer. Collision and appearance decisions are not duplicated here; they
// belong to the tile type (appearance) and the layer's collidableColors mask
// (which palette indices count), with only a single opt-in bit per cell.
typedef struct {
    uint16_t tileTypeID;  // index into the layer's tile type table
    uint8_t  flags;       // TILE_FLAG_* bitmask
    int8_t   offsetX;     // world-pixel nudge applied at render time (±127 px)
    int8_t   offsetY;     // world-pixel nudge applied at render time (±127 px)
    void*    metadata;    // optional game-side payload; NULL if unused
} TileLocation;           // stays 16 bytes: offsetX/Y consume padding after flags

// Per-layer map state. Read-only from game code -- always write through the
// API functions so the GPU-side view buffer stays consistent with this state.
typedef struct {
    bool     initialized;
    bool     visible;          // rendered only when also enabled; skipped at draw time otherwise
    bool     enabled;          // when false: visible-change functions fail, and the layer never renders
    bool     wrapX;
    bool     wrapY;
    double   mapOffsetX;       // scroll offset in pixels (fractional, for sub-tile smoothness)
    double   mapOffsetY;
    uint16_t width;            // map width in tiles
    uint16_t height;           // map height in tiles
    uint16_t tileWidth;        // tile width in pixels -- fixed at InitTileLayer, never changes
    uint16_t tileHeight;       // tile height in pixels -- fixed at InitTileLayer, never changes
    uint16_t viewportX;        // viewport top-left on screen, in pixels
    uint16_t viewportY;
    uint16_t viewportWidth;    // viewport size on screen, in pixels
    uint16_t viewportHeight;
    float    scale;
    uint16_t collidableColors; // which palette indices (0-15) count for collision on this layer
    TileLocation location[MAX_MAP_WIDTH][MAX_MAP_HEIGHT];
} TileMap;

// Lifecycle

// Initialize one tile layer. Creates the bitmap atlas, sets all cells to type 0
// with no collision, and sets the viewport to cover the full reference canvas
// at 1x scale. tileCount is the number of tile type slots to support (capped
// at MAX_TILES). Multiple layers may be initialized independently.
bool InitTileLayer(uint8_t layer, uint16_t mapWidth, uint16_t mapHeight,
                   uint16_t tileWidth, uint16_t tileHeight, uint16_t tileCount,
                   float scale);

// Tears down all active layers and releases all GPU resources.
void ShutdownTiles(void);

// Tears down a single tile layer and releases its GPU resources (atlas,
// per-frame vertex/palette buffers), freeing it up for reallocation via a
// later InitTileLayer call with different dimensions or attributes. The
// shared pipeline and other layers are left untouched. Returns false if
// layer is out of range or not currently initialized.
// Use of this function is actually not recommended. Instead, pre-allocating
// all layers at program startup and reusing them through runtime is preferred.
// Tile maps un-needed durring different stages of the program's lifetime
// can simply be disabled/hidden.
bool ReleaseTileLayer(uint8_t layer);

// Bitmap registration
// Upload a tile bitmap to the layer's atlas. bitmap is raw 4-bit packed data
// (tileWidth * tileHeight pixels, 2 per byte, high nibble = left pixel) --
// same packing convention as the sprite atlas. Pass NULL to assign the built-in
// test pattern for that layer. The upload overwrites any previous content at
// tileID. The caller retains ownership of bitmap; it is not referenced after
// this call returns.
bool UploadTileBitmap(uint8_t layer, int tileID, const unsigned char* bitmap);

// Tile type appearance. tileID must have been uploaded before calling these.
// All three return false (no-op) if layer is disabled -- see SetTileLayerEnabled.
bool SetTilePalette(uint8_t layer, int tileID, uint8_t palette);
bool SetTileTransparency(uint8_t layer, int tileID, uint8_t transparency);
bool SetTileShowZero(uint8_t layer, int tileID, bool showzero);

// Visibility: a render-time toggle that only takes effect while the layer is
// also enabled -- see SetTileLayerEnabled below. An invisible-but-enabled
// layer keeps scrolling, accepting placements, uploading bitmaps, etc.
// exactly as normal; it's just skipped at draw time, so none of it reaches
// the final output. Toggling this while the layer is disabled has no visible
// effect (it stays hidden either way), but is remembered and takes effect
// again as soon as the layer is re-enabled. Defaults to true at InitTileLayer.
bool GetTileLayerVisible(uint8_t layer);
bool SetTileLayerVisible(uint8_t layer, bool visible);
bool ToggleTileLayerVisible(uint8_t layer);

// Enablement: gates functions that would otherwise produce a visible change
// on this layer (PlaceTile, SetTileFlip, SetTileOffset, ScrollTileMap,
// SetTileMapOffset, SetTileLayerScale, SetTileViewport, SetTileMapWrapping,
// SetTilePalette, SetTileTransparency, SetTileShowZero) -- each of those
// returns false and leaves layer state untouched while disabled. Functions
// with no visible effect (UploadTileBitmap, SetTileCellCollision,
// SetTileLayerCollidableColors) are never gated and keep succeeding while
// disabled, so the map/bitmaps can still be edited behind the scenes. A
// disabled layer also never renders, regardless of SetTileLayerVisible --
// it freezes in place and reappears looking exactly as it did when disabled,
// once re-enabled. Defaults to true at InitTileLayer.
bool GetTileLayerEnabled(uint8_t layer);
bool SetTileLayerEnabled(uint8_t layer, bool enabled);
bool ToggleTileLayerEnabled(uint8_t layer);

// Map placement
// Set which tile type occupies a map cell. Returns false (no-op) if layer is
// disabled -- see SetTileLayerEnabled.
bool PlaceTile(uint8_t layer, uint16_t x, uint16_t y, uint16_t tileID);

// Per-cell flip flags. Both default to off. Flip is applied purely to UV
// coordinates at render time -- no atlas changes, no second copy of the bitmap.
// Returns false (no-op) if layer is disabled -- see SetTileLayerEnabled.
bool SetTileFlip(uint8_t layer, uint16_t x, uint16_t y, bool hflip, bool vflip);

// Per-cell collision opt-in. Default off. Whether a cell's colors actually
// trigger collision depends on the layer's collidableColors mask; this flag
// just controls whether that cell participates at all. Collision has no
// visible effect, so this is never gated by SetTileLayerEnabled.
void SetTileCellCollision(uint8_t layer, uint16_t x, uint16_t y, bool enabled);

// Layer-level settings. All return false (no-op) if layer is disabled --
// see SetTileLayerEnabled.
bool SetTileMapWrapping(uint8_t layer, bool wrapX, bool wrapY);
bool SetTileLayerScale(uint8_t layer, float scale);
bool SetTileViewport(uint8_t layer, uint16_t x, uint16_t y, uint16_t width, uint16_t height);

// Scroll by a delta in unscaled pixels. Scale is applied internally, so a
// deltaX of 1.0 always moves one source-resolution pixel regardless of the
// layer's current scale factor. Clamped to map bounds unless wrapping is on.
// Returns false (no-op) if layer is disabled -- see SetTileLayerEnabled.
bool ScrollTileMap(uint8_t layer, double deltaX, double deltaY);

// Set the scroll offset to an absolute position in unscaled pixels.
// Returns false (no-op) if layer is disabled -- see SetTileLayerEnabled.
bool SetTileMapOffset(uint8_t layer, double x, double y);

// Per-cell render nudge: displaces a single tile from its grid position by
// (offsetX, offsetY) world/canvas pixels at draw time. Useful for scattering
// sparse layers so tiles don't appear rigidly grid-aligned. Returns false
// (no-op) if layer is disabled -- see SetTileLayerEnabled.
bool SetTileOffset(uint8_t layer, uint16_t mapX, uint16_t mapY, int8_t offsetX, int8_t offsetY);

// Collidable colors: which palette indices (0-15) count for collision across
// this whole layer. Individual cells still need TILE_FLAG_COLLISION set.
// Defaults to 0 (nothing collidable). Use a bitmask: bit N set means palette
// index N is collidable.
void     SetTileLayerCollidableColors(uint8_t layer, uint16_t mask);
uint16_t GetTileLayerCollidableColors(uint8_t layer);

// Debug / test
// Assigns the built-in test pattern bitmap to tileID on the given layer.
// Equivalent to UploadTileBitmap(layer, tileID, NULL).
void TileTestPattern(uint8_t layer, int tileID);

// Generates a two-color box bitmap: palette index 1 forms a 2-pixel-wide border
// on all four edges; palette index 0 fills the interior. showzero controls
// whether the interior is transparent (false) or uses palette color 0 (true).
// Uses the layer's registered tile dimensions.
void TileBoxPattern(uint8_t layer, int tileID);

// Atlas debug view (not yet implemented; stubs present for API completeness).
void ToggleTileAtlasView(uint8_t layer);
bool GetTileAtlasViewActive(uint8_t layer);

#endif // GDMF_TILES_H
