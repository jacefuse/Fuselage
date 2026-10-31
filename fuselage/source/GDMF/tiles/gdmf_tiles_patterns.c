// Packed built-in patterns. SEE: (GDMF_TILES.md, Bitmap registration)
#include "gdmf_tiles_bitmaps.h"
#include <string.h>

bool UploadTileTestPattern(uint8_t layer, int tile_id) {
    return UploadTileBitmap(layer, tile_id, NULL);
}
bool UploadTileBoxPattern(uint8_t layer, int tile_id) {
    uint16_t tile_count = gdmf_tiles_bitmap_count(layer);
    if (tile_id < 0 || tile_id >= tile_count) {
        return false;
    }
    const TileMap* map = gdmf_tiles_state_get(layer);
    unsigned width = map->tileWidth, height = map->tileHeight;
    unsigned char packed[GDMF_TILE_BITMAP_PACKED_BYTES];
    memset(packed, 0, sizeof(packed));
    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            if (x < 2 || width - x <= 2 || y < 2 || height - y <= 2) {
                unsigned pixel_index = y * width + x;
                packed[pixel_index / 2] |= (unsigned char)(1u << (pixel_index % 2 ? 0 : 4));
            }
        }
    }
    return UploadTileBitmap(layer, tile_id, packed);
}
