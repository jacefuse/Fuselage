// GDMF Tiles helpers -- relative (delta) conveniences over the absolute tile
// API in gdmf_tiles.h.
//
// Tile layers are absolute-state registers (see the design rationale in
// gdmf_tiles.h): you set the scroll offset to a value, you don't ask it to
// change "by" some amount. This wrapper restores the common per-frame scroll
// convenience by reading the current offset and adding a delta. Offsets are in
// source (unscaled) pixels, so a given delta scrolls the same distance
// regardless of the layer's scale.

#ifndef GDMF_TILES_HELP_H
#define GDMF_TILES_HELP_H

#include "gdmf_tiles.h"

// Relative scroll: adds (dx, dy), in source (unscaled) pixels, to the layer's
// current scroll offset. Clamped or wrapped per the layer's wrapping mode,
// exactly like SetTileMapOffset. Returns true on success, false if the layer is
// disabled or the index is invalid (propagated from SetTileMapOffset).
bool ScrollTileMap(uint8_t layer, double dx, double dy);

#endif // GDMF_TILES_HELP_H