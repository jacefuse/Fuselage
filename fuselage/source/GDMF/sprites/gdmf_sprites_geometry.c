// Forward geometry and cached bounds. SEE: (GDMF_SPRITES.md, Transforms)
#include "gdmf_sprites_geometry.h"
#include <math.h>
#include <stddef.h>

static gdmf_sprite_bounds gdmf_sprites_cached_bounds[MAX_SPRITES];
static bool gdmf_sprites_bounds_valid[MAX_SPRITES];

void gdmf_sprites_geometry_invalidate(int sprite_index) {
    if (gdmf_sprites_index_valid(sprite_index)) {
        gdmf_sprites_bounds_valid[sprite_index] = false;
    }
}

void gdmf_sprites_world_quad(const Sprite* sprite, float output_x[4], float output_y[4]) {
    const float scale_x = sprite->scale * sprite->scaleX;
    const float scale_y = sprite->scale * sprite->scaleY;
    float half_width = (SPRITE_WIDTH * scale_x) * 0.5f;
    float half_height = (SPRITE_HEIGHT * scale_y) * 0.5f;
    float hotspot_x = sprite->hotspotX * scale_x;
    float hotspot_y = sprite->hotspotY * scale_y;
    float center_x = (sprite->x - hotspot_x) + half_width;
    float center_y = (sprite->y - hotspot_y) + half_height;
    float pivot_x = sprite->rotateAroundHotspot ? sprite->x : center_x;
    float pivot_y = sprite->rotateAroundHotspot ? sprite->y : center_y;
    float angle = (float)(sprite->rotation * (3.14159265358979323846 / 180.0));
    float cosine = cosf(angle), sine = sinf(angle);
    float local_x[4] = {-half_width, half_width, -half_width, half_width};
    float local_y[4] = {-half_height, -half_height, half_height, half_height};

    // Preserve operation order shared with the inverse at bitmap edges.
    for (int corner = 0; corner < 4; ++corner) {
        float sheared_x = local_x[corner] + sprite->skewX * local_y[corner];
        float sheared_y = local_y[corner] + sprite->skewY * local_x[corner];
        float delta_x = (center_x + sheared_x) - pivot_x;
        float delta_y = (center_y + sheared_y) - pivot_y;
        float rotated_x = cosine * delta_x - sine * delta_y;
        float rotated_y = sine * delta_x + cosine * delta_y;
        output_x[corner] = pivot_x + rotated_x;
        output_y[corner] = pivot_y + rotated_y;
    }
}

const gdmf_sprite_bounds* gdmf_sprites_bounds_get(int sprite_index) {
    const Sprite* sprite = gdmf_sprites_state_get(sprite_index);
    if (!sprite) {
        return NULL;
    }
    gdmf_sprite_bounds* bounds = &gdmf_sprites_cached_bounds[sprite_index];
    if (!gdmf_sprites_bounds_valid[sprite_index]) {
        float corner_x[4], corner_y[4];
        gdmf_sprites_world_quad(sprite, corner_x, corner_y);
        bounds->minimum_x = fminf(fminf(corner_x[0], corner_x[1]), fminf(corner_x[2], corner_x[3]));
        bounds->maximum_x = fmaxf(fmaxf(corner_x[0], corner_x[1]), fmaxf(corner_x[2], corner_x[3]));
        bounds->minimum_y = fminf(fminf(corner_y[0], corner_y[1]), fminf(corner_y[2], corner_y[3]));
        bounds->maximum_y = fmaxf(fmaxf(corner_y[0], corner_y[1]), fmaxf(corner_y[2], corner_y[3]));
        gdmf_sprites_bounds_valid[sprite_index] = true;
    }
    return bounds;
}

bool GetSpriteBoundingBox(int sprite_index, float* minimum_x, float* minimum_y, float* maximum_x,
                          float* maximum_y) {
    const gdmf_sprite_bounds* bounds = gdmf_sprites_bounds_get(sprite_index);
    if (!bounds) {
        return false;
    }
    if (minimum_x) {
        *minimum_x = bounds->minimum_x;
    }
    if (minimum_y) {
        *minimum_y = bounds->minimum_y;
    }
    if (maximum_x) {
        *maximum_x = bounds->maximum_x;
    }
    if (maximum_y) {
        *maximum_y = bounds->maximum_y;
    }
    return true;
}
