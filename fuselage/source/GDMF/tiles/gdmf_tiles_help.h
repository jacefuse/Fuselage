#ifndef GDMF_TILES_HELP_H
#define GDMF_TILES_HELP_H

#include "gdmf_tiles.h"

// Adds source-pixel offsets; returns SetTileMapOffset's success result.
// SEE: (GDMF_TILES.md, Scrolling helper)
bool ScrollTileMap(uint8_t layer, double offset_x, double offset_y);

#endif
