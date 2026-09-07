#include "gdmf_tiles_help.h"

// Read the current absolute offset, add the delta, write it back. The old
// (removed) ScrollTileMap did this too but silently multiplied the delta by the
// layer's scale, so it disagreed with SetTileMapOffset about units; this one is
// source-pixels end to end, matching the setter. Validation propagates from
// SetTileMapOffset (false on a disabled layer or an invalid index).

bool ScrollTileMap(uint8_t layer, double dx, double dy) {
    double ox, oy;
    GetTileMapOffset(layer, &ox, &oy);
    return SetTileMapOffset(layer, ox + dx, oy + dy);
}
