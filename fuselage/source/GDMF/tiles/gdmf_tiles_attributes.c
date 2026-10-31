// Host attributes. SEE: (GDMF_TILES.md, Host state ownership)
#include "gdmf_tiles_state_private.h"

bool SetTileMapWrapX(uint8_t layer, bool wrap) {
    if (!gdmf_tiles_layer_writable(layer)) {
        return false;
    }
    gdmf_tiles_state_get(layer)->wrapX = wrap;
    gdmf_tiles_backend_mark_dirty(layer);

    return true;
}

bool SetTileMapWrapY(uint8_t layer, bool wrap) {
    if (!gdmf_tiles_layer_writable(layer)) {
        return false;
    }
    gdmf_tiles_state_get(layer)->wrapY = wrap;
    gdmf_tiles_backend_mark_dirty(layer);

    return true;
}

bool GetTileMapWrapX(uint8_t layer) {
    if (!gdmf_tiles_layer_valid(layer)) {
        return false;
    }

    return gdmf_tiles_state_get(layer)->wrapX;
}

bool GetTileMapWrapY(uint8_t layer) {
    if (!gdmf_tiles_layer_valid(layer)) {
        return false;
    }

    return gdmf_tiles_state_get(layer)->wrapY;
}

bool SetTileLayerScale(uint8_t layer, float scale) {
    if (!gdmf_tiles_layer_writable(layer) || scale <= 0.0f) {
        return false;
    }
    gdmf_tiles_state_get(layer)->scale = scale;
    gdmf_tiles_backend_mark_dirty(layer);

    return true;
}

float GetTileLayerScale(uint8_t layer) {
    if (!gdmf_tiles_layer_valid(layer)) {
        return 0.0f;
    }

    return gdmf_tiles_state_get(layer)->scale;
}

uint16_t GetTileLayerTileWidth(uint8_t layer) {
    if (!gdmf_tiles_layer_valid(layer)) {
        return 0;
    }

    return gdmf_tiles_state_get(layer)->tileWidth;
}

uint16_t GetTileLayerTileHeight(uint8_t layer) {
    if (!gdmf_tiles_layer_valid(layer)) {
        return 0;
    }

    return gdmf_tiles_state_get(layer)->tileHeight;
}

bool SetTileLayerPriority(uint8_t layer, uint8_t priority) {
    if (!gdmf_tiles_layer_writable(layer)) {
        return false;
    }

    // Priority is captured per frame and does not change vertex geometry.
    gdmf_tiles_state_get(layer)->priority = priority;

    return true;
}

uint8_t GetTileLayerPriority(uint8_t layer) {
    if (!gdmf_tiles_layer_valid(layer)) {
        return 0;
    }

    return gdmf_tiles_state_get(layer)->priority;
}

bool SetTileViewport(uint8_t layer, float x, float y, float width, float height) {
    if (!gdmf_tiles_layer_writable(layer)) {
        return false;
    }

    gdmf_tiles_state_get(layer)->viewportX = x;
    gdmf_tiles_state_get(layer)->viewportY = y;
    gdmf_tiles_state_get(layer)->viewportWidth = width < 0.0f ? 0.0f : width;
    gdmf_tiles_state_get(layer)->viewportHeight = height < 0.0f ? 0.0f : height;
    gdmf_tiles_backend_mark_dirty(layer);

    return true;
}

void GetTileViewport(uint8_t layer, float* x, float* y, float* width, float* height) {
    float viewport_x = 0.0f, viewport_y = 0.0f, viewport_width = 0.0f, viewport_height = 0.0f;

    if (gdmf_tiles_layer_valid(layer)) {
        viewport_x = gdmf_tiles_state_get(layer)->viewportX;
        viewport_y = gdmf_tiles_state_get(layer)->viewportY;
        viewport_width = gdmf_tiles_state_get(layer)->viewportWidth;
        viewport_height = gdmf_tiles_state_get(layer)->viewportHeight;
    }
    if (x) {
        *x = viewport_x;
    }
    if (y) {
        *y = viewport_y;
    }
    if (width) {
        *width = viewport_width;
    }
    if (height) {
        *height = viewport_height;
    }

    return;
}

bool SetTileMapRotation(uint8_t layer, float radians) {
    if (!gdmf_tiles_layer_writable(layer)) {
        return false;
    }
    gdmf_tiles_state_get(layer)->rotation = radians;
    gdmf_tiles_backend_mark_dirty(layer);

    return true;
}

float GetTileMapRotation(uint8_t layer) {
    return gdmf_tiles_layer_valid(layer) ? gdmf_tiles_state_get(layer)->rotation : 0.0f;
}

bool SetTileMapPivot(uint8_t layer, float x, float y) {
    if (!gdmf_tiles_layer_writable(layer)) {
        return false;
    }
    gdmf_tiles_state_get(layer)->pivotX = x;
    gdmf_tiles_state_get(layer)->pivotY = y;

    gdmf_tiles_backend_mark_dirty(layer);

    return true;
}

void GetTileMapPivot(uint8_t layer, float* x, float* y) {
    float pivot_x = 0.0f, pivot_y = 0.0f;

    if (gdmf_tiles_layer_valid(layer)) {
        pivot_x = gdmf_tiles_state_get(layer)->pivotX;
        pivot_y = gdmf_tiles_state_get(layer)->pivotY;
    }
    if (x) {
        *x = pivot_x;
    }
    if (y) {
        *y = pivot_y;
    }

    return;
}

bool SetTileLayerCollidableColors(uint8_t layer, uint16_t mask) {
    if (!gdmf_tiles_layer_valid(layer)) {
        return false;
    }
    gdmf_tiles_state_get(layer)->collidableColors = mask;

    return true;
}

uint16_t GetTileLayerCollidableColors(uint8_t layer) {
    if (!gdmf_tiles_layer_valid(layer)) {
        return 0;
    }

    return gdmf_tiles_state_get(layer)->collidableColors;
}

bool GetTileLayerVisible(uint8_t layer) {
    if (!gdmf_tiles_layer_valid(layer)) {
        return false;
    }

    return gdmf_tiles_state_get(layer)->visible;
}

bool SetTileLayerVisible(uint8_t layer, bool visible) {
    if (!gdmf_tiles_layer_valid(layer)) {
        return false;
    }
    gdmf_tiles_state_get(layer)->visible = visible;

    return gdmf_tiles_state_get(layer)->visible;
}

bool ToggleTileLayerVisible(uint8_t layer) {
    if (!gdmf_tiles_layer_valid(layer)) {
        return false;
    }
    gdmf_tiles_state_get(layer)->visible = !gdmf_tiles_state_get(layer)->visible;

    return gdmf_tiles_state_get(layer)->visible;
}

bool GetTileLayerEnabled(uint8_t layer) {
    if (!gdmf_tiles_layer_valid(layer)) {
        return false;
    }

    return gdmf_tiles_state_get(layer)->enabled;
}

bool SetTileLayerEnabled(uint8_t layer, bool enabled) {
    if (!gdmf_tiles_layer_valid(layer)) {
        return false;
    }
    gdmf_tiles_state_get(layer)->enabled = enabled;

    return gdmf_tiles_state_get(layer)->enabled;
}

bool ToggleTileLayerEnabled(uint8_t layer) {
    if (!gdmf_tiles_layer_valid(layer)) {
        return false;
    }
    gdmf_tiles_state_get(layer)->enabled = !gdmf_tiles_state_get(layer)->enabled;

    return gdmf_tiles_state_get(layer)->enabled;
}
