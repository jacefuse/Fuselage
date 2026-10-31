// Host scroll. SEE: (GDMF_TILES.md, Host state ownership)
#include "gdmf_tiles_state_private.h"
#include <math.h>

bool SetTileMapOffset(uint8_t layer, double x, double y) {
    if (!gdmf_tiles_layer_writable(layer)) {
        return false;
    }
    gdmf_tiles_backend_mark_dirty(layer);
    TileMap* map = gdmf_tiles_state_get(layer);

    double scale = (map->scale > 0.0f) ? (double)map->scale : 1.0;
    double map_pixel_width = map->width * (double)map->tileWidth;
    double map_pixel_height = map->height * (double)map->tileHeight;
    double view_width = map->viewportWidth / scale;
    double view_height = map->viewportHeight / scale;

    if (map->rotation != 0.0f) {
        const double cosine_magnitude = fabs(cos((double)map->rotation));
        const double sine_magnitude = fabs(sin((double)map->rotation));
        const double original_width = view_width, original_height = view_height;
        view_width = original_width * cosine_magnitude + original_height * sine_magnitude;
        view_height = original_width * sine_magnitude + original_height * cosine_magnitude;
    }

    if (map->wrapX) {
        map->mapOffsetX = fmod(x, map_pixel_width);
        if (map->mapOffsetX < 0.0) {
            map->mapOffsetX += map_pixel_width;
        }
    } else {
        double maximum_x = map_pixel_width - view_width;
        if (maximum_x < 0.0) {
            maximum_x = 0.0;
        }

        map->mapOffsetX = (x < 0.0) ? 0.0 : (x > maximum_x ? maximum_x : x);
    }

    if (map->wrapY) {
        map->mapOffsetY = fmod(y, map_pixel_height);
        if (map->mapOffsetY < 0.0) {
            map->mapOffsetY += map_pixel_height;
        }
    } else {
        double maximum_y = map_pixel_height - view_height;
        if (maximum_y < 0.0) {
            maximum_y = 0.0;
        }

        map->mapOffsetY = (y < 0.0) ? 0.0 : (y > maximum_y ? maximum_y : y);
    }

    return true;
}

void GetTileMapOffset(uint8_t layer, double* x, double* y) {
    double offset_x = 0.0, offset_y = 0.0;

    if (gdmf_tiles_layer_valid(layer)) {
        offset_x = gdmf_tiles_state_get(layer)->mapOffsetX;
        offset_y = gdmf_tiles_state_get(layer)->mapOffsetY;
    }
    if (x) {
        *x = offset_x;
    }
    if (y) {
        *y = offset_y;
    }

    return;
}
