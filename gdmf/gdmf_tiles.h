// GDMF Tile Layer
//
// Up to MAX_TILE_LAYERS independent tile maps. Each layer has its own bitmap
// atlas, scroll state, viewport, and map data. Tile shapes are registered
// once via UploadTileBitmap; map cells reference a shape by ID (tileTypeID)
// and add per-cell flip, palette, transparency, showzero, and collision
// opt-in. Colors are never baked in -- palette is resolved at render time in
// the fragment shader from the same Colors buffer the sprite layer uploads
// every frame.
//
// Collision defaults to fully off: nothing at the layer level is collidable,
// and no cell has its collision flag set. Opt in explicitly via
// SetTileLayerCollidableColors and SetTileCellCollision.

#ifndef GDMF_TILES_H
#define GDMF_TILES_H

#include <stdbool.h>
#include <stdint.h>

#define GDMF_TILES_VERSION "0.3.2026071602 COLON"

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

// One cell in the map. Kept small -- this struct lives in a 512x512 array
// per layer. tileTypeID is purely a shape reference (index into the atlas
// registered via UploadTileBitmap); appearance -- palette, transparency,
// showzero -- is entirely a per-cell choice, same reasoning for all three:
// wanting the same shape rendered differently from cell to cell (the same
// glyph tile normal in one spot and inverted in another, say) is far more
// common than wanting a second registration of identical bitmap data just to
// get a different color/alpha/zero-handling. The layer's collidableColors
// mask decides which palette indices count for collision.
// Placing a new tileTypeID at a cell (PlaceTile) never resets palette,
// transparency, showzero, flip, or offset -- they're independent of shape.
typedef struct {
    uint16_t tileTypeID;    // index into the layer's tile bitmap atlas
    uint8_t  flags;         // TILE_FLAG_* bitmask
    int8_t   offsetX;       // world-pixel nudge applied at render time (int8: -128 to 127)
    int8_t   offsetY;       // world-pixel nudge applied at render time (int8: -128 to 127)
    uint8_t  palette;       // which of the 256 16-color palettes to apply at render time
    uint8_t  transparency;  // overall alpha: 0 = invisible, 255 = fully opaque
    bool     showzero;      // if true, palette index 0 renders; if false, it is transparent
    uint32_t tag;           // free 32-bit game-defined value; the engine never reads it (SetTileCellTag)
} TileLocation;             // 12 bytes: the byte fields pad up to tag's 4-byte alignment

// Per-layer map state. Read-only from game code -- always write through the
// API functions so the GPU-side view buffer stays consistent with this state.
typedef struct {
    bool     initialized;
    bool     visible;          // rendered only when also enabled; skipped at draw time otherwise
    bool     enabled;          // when false: visible-change functions fail, and the layer never renders
    bool     wrapX;
    bool     wrapY;
    double   mapOffsetX;       // scroll offset in source pixels (fractional, for sub-tile smoothness)
    double   mapOffsetY;
    uint16_t width;            // map width in tiles
    uint16_t height;           // map height in tiles
    uint16_t tileWidth;        // tile width in pixels -- fixed at InitTileLayer, never changes
    uint16_t tileHeight;       // tile height in pixels -- fixed at InitTileLayer, never changes
    int      viewportX;        // viewport top-left on screen, in pixels -- SIGNED: may be
    int      viewportY;        // negative so a layer can sit partly (or wholly) off-screen,
                               // e.g. scrolling a strip off the left/top edge.
    int      viewportWidth;    // viewport size on screen, in pixels (clamped to >= 0)
    int      viewportHeight;
    float    scale;
    uint8_t  priority;         // render priority 0-255; higher draws further back, lower draws in front.
                               // Shared 256-level space with sprites and pixies -- see SetTileLayerPriority.
                               // Defaults to the layer index at InitTileLayer.
    uint16_t collidableColors; // which palette indices (0-15) count for collision on this layer
    TileLocation location[MAX_MAP_WIDTH][MAX_MAP_HEIGHT];
} TileMap;

// API return convention: every function that performs an action returns bool
// -- true on success, false on any failure (an invalid layer index or cell
// coordinate, a disabled/gated layer, or a failing subsystem). A bad index is
// reported, never silently swallowed; the caller decides whether to check.
// Getters are the exception: they return their value (or write it through an
// out-pointer), with a documented default when the layer/coordinate is invalid.

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
// Tile maps unneeded during different stages of the program's lifetime can
// simply be disabled or hidden.
bool ReleaseTileLayer(uint8_t layer);

// Bitmap registration
// Upload a tile bitmap to the layer's atlas. bitmap is raw 4-bit packed data
// (tileWidth * tileHeight pixels, 2 per byte, high nibble = left pixel) --
// same packing convention as the sprite atlas. Pass NULL to assign the built-in
// test pattern for that layer. The upload overwrites any previous content at
// tileID. The caller retains ownership of bitmap; it is not referenced after
// this call returns.
bool UploadTileBitmap(uint8_t layer, int tileID, const unsigned char* bitmap);

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
// on this layer (PlaceTile, FillTileLayer, SetTileCellPalette,
// SetTileLayerPalette, SetTileCellTransparency, SetTileLayerTransparency,
// SetTileCellShowZero, SetTileLayerShowZero, SetTileFlip, SetTileOffset,
// SetTileMapOffset, SetTileLayerScale, SetTileLayerPriority, SetTileViewport,
// SetTileMapWrapping) --
// each of those returns false and leaves layer state untouched while disabled.
// Functions with no visible effect (UploadTileBitmap, SetTileCellCollision,
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
// disabled or the coordinate is out of range -- see SetTileLayerEnabled.
bool PlaceTile(uint8_t layer, uint16_t x, uint16_t y, uint16_t tileID);

// Sets every cell in the map to tileID in one call -- the bulk form of
// PlaceTile, for clearing a map (tileID 0) or filling it with a single tile.
// Leaves palette/transparency/showzero/flip/offset/collision on every cell
// untouched, same as PlaceTile leaves them untouched on the one cell it
// touches. Returns false (no-op) if layer is disabled -- see
// SetTileLayerEnabled.
bool FillTileLayer(uint8_t layer, uint16_t tileID);

// Read which tile type occupies a cell. Returns 0 if layer/coordinate is
// invalid -- indistinguishable from a cell genuinely holding tile type 0.
uint16_t GetTile(uint8_t layer, uint16_t x, uint16_t y);

// Per-cell palette: which of the 256 16-color palettes this cell's tile
// renders with. Defaults to 0. Independent of tileTypeID -- placing a
// different tile at this cell does not reset it, same as flip and offset
// below, so the same shape can be reused at a different color per cell
// without registering a second tile type. Returns false (no-op) if layer is
// disabled or the coordinate is out of range -- see SetTileLayerEnabled.
bool    SetTileCellPalette(uint8_t layer, uint16_t x, uint16_t y, uint8_t palette);
uint8_t GetTileCellPalette(uint8_t layer, uint16_t x, uint16_t y);  // 0 if invalid

// Sets every cell in the map to the same palette in one call -- the bulk
// form of SetTileCellPalette. Returns false (no-op) if layer is disabled --
// see SetTileLayerEnabled.
bool SetTileLayerPalette(uint8_t layer, uint8_t palette);

// Per-cell transparency: overall alpha (0 = invisible, 255 = fully opaque).
// Defaults to 255. Independent of tileTypeID -- placing a different tile at
// this cell does not reset it, same as palette/flip/offset. Returns false
// (no-op) if layer is disabled or the coordinate is out of range -- see
// SetTileLayerEnabled.
bool    SetTileCellTransparency(uint8_t layer, uint16_t x, uint16_t y, uint8_t transparency);
uint8_t GetTileCellTransparency(uint8_t layer, uint16_t x, uint16_t y);  // 0 if invalid

// Sets every cell in the map to the same transparency in one call -- the
// bulk form of SetTileCellTransparency. Returns false (no-op) if layer is
// disabled -- see SetTileLayerEnabled.
bool SetTileLayerTransparency(uint8_t layer, uint8_t transparency);

// Per-cell showzero: if true, palette index 0 renders instead of being
// treated as transparent. Defaults to false. Independent of tileTypeID, same
// as transparency above. Returns false (no-op) if layer is disabled or the
// coordinate is out of range -- see SetTileLayerEnabled.
bool SetTileCellShowZero(uint8_t layer, uint16_t x, uint16_t y, bool showzero);
bool GetTileCellShowZero(uint8_t layer, uint16_t x, uint16_t y);  // false if invalid

// Sets every cell in the map to the same showzero in one call -- the bulk
// form of SetTileCellShowZero. Returns false (no-op) if layer is disabled --
// see SetTileLayerEnabled.
bool SetTileLayerShowZero(uint8_t layer, bool showzero);

// Per-cell flip flags. Both default to off. Flip is applied purely to UV
// coordinates at render time -- no atlas changes, no second copy of the bitmap.
// Returns false (no-op) if layer is disabled or the coordinate is out of
// range -- see SetTileLayerEnabled. The getter writes both flags through its
// out-pointers (either may be NULL); on an invalid layer/coordinate it writes
// false to both.
bool SetTileFlip(uint8_t layer, uint16_t x, uint16_t y, bool hflip, bool vflip);
void GetTileCellFlip(uint8_t layer, uint16_t x, uint16_t y, bool* hflip, bool* vflip);

// Per-cell collision opt-in. Default off. Whether a cell's colors actually
// trigger collision depends on the layer's collidableColors mask; this flag
// just controls whether that cell participates at all. Collision has no
// visible effect, so this is never gated by SetTileLayerEnabled -- but it
// still returns false on an invalid layer/coordinate.
bool SetTileCellCollision(uint8_t layer, uint16_t x, uint16_t y, bool enabled);
bool GetTileCellCollision(uint8_t layer, uint16_t x, uint16_t y);  // false if invalid

// Per-cell game tag. A free 32-bit value stored on each cell that the engine
// never interprets -- it's the game's to define. Use it for whatever per-cell
// facts your game reacts to: pack it as a bitfield (solid | door | trigger |
// vanishes-when-X | ...), or store an index into your own table. This replaced
// a void* "metadata" pointer: 32 bits carry the same intent, a game can read
// and write them from anywhere (including a future MISL guest, which a host
// pointer could never mean anything to), and it costs no dangling reference.
// Default 0. Collision-free and purely game-side, so never gated by
// SetTileLayerEnabled; still returns false / 0 on an invalid layer or cell.
bool     SetTileCellTag(uint8_t layer, uint16_t x, uint16_t y, uint32_t tag);
uint32_t GetTileCellTag(uint8_t layer, uint16_t x, uint16_t y);  // 0 if invalid

// Layer-level settings. All return false (no-op) if layer is disabled --
// see SetTileLayerEnabled. The matching getters read regardless of enablement
// and write through their out-pointers (any may be NULL), writing 0/false on
// an invalid layer.
bool SetTileMapWrapping(uint8_t layer, bool wrapX, bool wrapY);
void GetTileMapWrapping(uint8_t layer, bool* wrapX, bool* wrapY);
bool  SetTileLayerScale(uint8_t layer, float scale);
float GetTileLayerScale(uint8_t layer);  // 0 if invalid

// Render priority for this whole layer, on the same 0-255 scale sprites and
// pixies use. Higher priority draws further back; lower draws in front (so a
// layer at priority 0 is frontmost). Items are drawn highest priority down to
// lowest; where a tile layer ties a sprite or pixie on priority, the type
// order is pixies (back), then tiles, then sprites (front). Two tile layers
// may share a priority; the lower layer index draws in front. Defaults to the
// layer index (0-15) at InitTileLayer. A visible change, so gated by
// SetTileLayerEnabled -- returns false (no-op) if layer is disabled or invalid.
bool    SetTileLayerPriority(uint8_t layer, uint8_t priority);
uint8_t GetTileLayerPriority(uint8_t layer);  // 0 if invalid
// Viewport rect in SCREEN pixels. x/y are signed -- a negative x/y (or one past
// the canvas edge) is legal and places the layer partly or wholly off-screen,
// so a strip can scroll off any edge; the visible part is clipped to the canvas.
// width/height are extents and are clamped to >= 0.
bool SetTileViewport(uint8_t layer, int x, int y, int width, int height);
void GetTileViewport(uint8_t layer, int* x, int* y, int* width, int* height);

// Set the scroll offset to an absolute position in source (unscaled) pixels:
// a value of tileWidth moves the map by exactly one tile regardless of the
// layer's scale. Clamped to map bounds unless wrapping is on. Returns false
// (no-op) if layer is disabled -- see SetTileLayerEnabled. The map is treated
// as an absolute-state register; a relative scroll is the caller's to compute
// (new = current + delta) via GetTileMapOffset and apply through this setter.
bool SetTileMapOffset(uint8_t layer, double x, double y);
void GetTileMapOffset(uint8_t layer, double* x, double* y);  // source pixels; 0 if invalid

// Per-cell render nudge: displaces a single tile from its grid position by
// (offsetX, offsetY) world/canvas pixels at draw time. Useful for scattering
// sparse layers so tiles don't appear rigidly grid-aligned. Returns false
// (no-op) if layer is disabled or the coordinate is out of range -- see
// SetTileLayerEnabled.
bool SetTileOffset(uint8_t layer, uint16_t mapX, uint16_t mapY, int8_t offsetX, int8_t offsetY);
void GetTileOffset(uint8_t layer, uint16_t mapX, uint16_t mapY, int8_t* offsetX, int8_t* offsetY);

// Collidable colors: which palette indices (0-15) count for collision across
// this whole layer. Individual cells still need TILE_FLAG_COLLISION set.
// Defaults to 0 (nothing collidable). Use a bitmask: bit N set means palette
// index N is collidable. Never gated (collision has no visible effect), but
// still returns false on an invalid layer.
bool     SetTileLayerCollidableColors(uint8_t layer, uint16_t mask);
uint16_t GetTileLayerCollidableColors(uint8_t layer);

// Debug / test
// Uploads the built-in test pattern bitmap to tileID on the given layer.
// Equivalent to UploadTileBitmap(layer, tileID, NULL). Returns true on
// success, false on an invalid layer/tileID or a failing upload.
bool UploadTileTestPattern(uint8_t layer, int tileID);

// Generates a two-color box bitmap and uploads it to tileID: palette index 1
// forms a 2-pixel-wide border on all four edges; palette index 0 fills the
// interior (transparent per cell unless that cell's showzero is set). Uses the
// layer's registered tile dimensions. Returns true on success, false on an
// invalid layer/tileID or a failing upload.
bool UploadTileBoxPattern(uint8_t layer, int tileID);

// Atlas debug view (deferred; stubs present for API completeness -- will
// return as a full-screen quad shared with the sprite layer).
void ToggleTileAtlasView(uint8_t layer);
bool GetTileAtlasViewActive(uint8_t layer);

#endif // GDMF_TILES_H