// Host bitmap mirror. SEE: (GDMF_TILES.md, Bitmap ownership)
#include "gdmf_tiles_bitmaps.h"
#include <string.h>

typedef struct {
    uint16_t width, height, count;
    unsigned char test_pattern[GDMF_TILE_BITMAP_PACKED_BYTES];
    bool registered[MAX_TILES], dirty[MAX_TILES];
    unsigned char pixels[MAX_TILES][GDMF_TILE_BITMAP_PIXELS];
} gdmf_tiles_bitmap_bank;

static gdmf_tiles_bitmap_bank gdmf_tiles_bitmap_banks[MAX_TILE_LAYERS];

static gdmf_tiles_bitmap_bank* gdmf_tiles_bitmap_bank_get(uint8_t layer) {
    const TileMap* map = gdmf_tiles_state_get(layer);
    if (!map || !map->initialized) {
        return NULL;
    }
    gdmf_tiles_bitmap_bank* bank = &gdmf_tiles_bitmap_banks[layer];
    return bank->count && bank->width == map->tileWidth && bank->height == map->tileHeight ? bank : NULL;
}
static bool gdmf_tiles_bitmap_id_valid(const gdmf_tiles_bitmap_bank* bank, int tile_id) {
    return bank && tile_id >= 0 && tile_id < bank->count;
}
bool gdmf_tiles_bitmaps_reset(uint8_t layer, uint16_t tile_count) {
    const TileMap* map = gdmf_tiles_state_get(layer);
    if (!map || !map->initialized || !tile_count || tile_count > MAX_TILES || !map->tileWidth ||
        map->tileWidth > MAX_TILE_WIDTH || !map->tileHeight || map->tileHeight > MAX_TILE_HEIGHT) {
        return false;
    }
    gdmf_tiles_bitmap_bank* bank = &gdmf_tiles_bitmap_banks[layer];
    bank->width = map->tileWidth;
    bank->height = map->tileHeight;
    bank->count = tile_count;
    memset(bank->registered, 0, sizeof(bank->registered));
    memset(bank->dirty, 0, sizeof(bank->dirty));
    memset(bank->test_pattern, 0, sizeof(bank->test_pattern));
    for (unsigned y = 0; y < bank->height; ++y) {
        for (unsigned x = 0; x < bank->width; ++x) {
            unsigned color_index = ((y / 4) * (bank->width / 4) + x / 4) & 15;
            unsigned pixel_index = y * bank->width + x;
            bank->test_pattern[pixel_index / 2] |= (unsigned char)(color_index << (pixel_index % 2 ? 0 : 4));
        }
    }
    return true;
}
void gdmf_tiles_bitmaps_clear(uint8_t layer) {
    if (layer < MAX_TILE_LAYERS) {
        gdmf_tiles_bitmap_bank* bank = &gdmf_tiles_bitmap_banks[layer];
        bank->width = bank->height = bank->count = 0;
        memset(bank->registered, 0, sizeof(bank->registered));
        memset(bank->dirty, 0, sizeof(bank->dirty));
    }
}
uint16_t gdmf_tiles_bitmap_count(uint8_t layer) {
    const gdmf_tiles_bitmap_bank* bank = gdmf_tiles_bitmap_bank_get(layer);
    return bank ? bank->count : 0;
}
const unsigned char* gdmf_tiles_bitmap_data(uint8_t layer, int tile_id) {
    const gdmf_tiles_bitmap_bank* bank = gdmf_tiles_bitmap_bank_get(layer);
    return gdmf_tiles_bitmap_id_valid(bank, tile_id) && bank->registered[tile_id] ? bank->pixels[tile_id]
                                                                                  : NULL;
}
void gdmf_tiles_bitmap_view_get(uint8_t layer, gdmf_tiles_bitmap_view* view) {
    const gdmf_tiles_bitmap_bank* bank = gdmf_tiles_bitmap_bank_get(layer);
    *view = bank ? (gdmf_tiles_bitmap_view){bank->registered, &bank->pixels[0][0], bank->count}
                 : (gdmf_tiles_bitmap_view){0};
}
bool gdmf_tiles_bitmap_dirty(uint8_t layer, int tile_id) {
    const gdmf_tiles_bitmap_bank* bank = gdmf_tiles_bitmap_bank_get(layer);
    return gdmf_tiles_bitmap_id_valid(bank, tile_id) && bank->registered[tile_id] && bank->dirty[tile_id];
}
void gdmf_tiles_bitmap_mark_uploaded(uint8_t layer, int tile_id) {
    gdmf_tiles_bitmap_bank* bank = gdmf_tiles_bitmap_bank_get(layer);
    if (gdmf_tiles_bitmap_id_valid(bank, tile_id) && bank->registered[tile_id]) {
        bank->dirty[tile_id] = false;
    }
}
void gdmf_tiles_bitmaps_mark_dirty(uint8_t layer) {
    gdmf_tiles_bitmap_bank* bank = gdmf_tiles_bitmap_bank_get(layer);
    if (bank) {
        for (unsigned tile_id = 0; tile_id < bank->count; ++tile_id) {
            bank->dirty[tile_id] = bank->registered[tile_id];
        }
    }
}
bool UploadTileBitmap(uint8_t layer, int tile_id, const unsigned char* bitmap) {
    gdmf_tiles_bitmap_bank* bank = gdmf_tiles_bitmap_bank_get(layer);
    if (!gdmf_tiles_bitmap_id_valid(bank, tile_id)) {
        return false;
    }
    const unsigned char* packed = bitmap ? bitmap : bank->test_pattern;
    unsigned pixel_count = bank->width * bank->height;
    for (unsigned pixel_index = 0; pixel_index < pixel_count; ++pixel_index) {
        bank->pixels[tile_id][pixel_index] = (packed[pixel_index / 2] >> (pixel_index % 2 ? 0 : 4)) & 15;
    }
    bool newly_registered = !bank->registered[tile_id];
    bank->registered[tile_id] = true;
    bank->dirty[tile_id] = true;
    if (newly_registered) {
        // A frame that previously omitted this bitmap needs new geometry.
        gdmf_tiles_backend_mark_dirty(layer);
    }
    return true;
}
