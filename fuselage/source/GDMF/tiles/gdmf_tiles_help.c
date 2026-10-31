#include "gdmf_tiles_help.h"

bool ScrollTileMap(uint8_t layer, double offset_x, double offset_y) {
    double current_x, current_y;
    GetTileMapOffset(layer, &current_x, &current_y);
    return SetTileMapOffset(layer, current_x + offset_x, current_y + offset_y);
}
