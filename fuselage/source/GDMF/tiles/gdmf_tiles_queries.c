// Public point queries. SEE: (GDMF_TILES.md, Point queries)
#include "gdmf_tiles_probe.h"
#include <string.h>

bool GetTilePointInfo(uint8_t layer, float canvas_x, float canvas_y, TilePointInfo* output) {
    if (!output) {
        return false;
    }
    memset(output, 0, sizeof(*output));
    gdmf_tile_probe probe;
    return gdmf_tile_probe_init(layer, &probe) && gdmf_tile_probe_point(&probe, canvas_x, canvas_y, output);
}
bool CheckTilePointCollision(uint8_t layer, float canvas_x, float canvas_y) {
    TilePointInfo point;
    return GetTilePointInfo(layer, canvas_x, canvas_y, &point) && point.collidable && point.onTile &&
           (gdmf_tiles_state_get(layer)->collidableColors & (1u << point.paletteIndex)) != 0;
}
