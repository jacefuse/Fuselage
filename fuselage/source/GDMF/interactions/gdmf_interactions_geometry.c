// Sprite geometry queries. SEE: (GDMF_INTERACTIONS.md, Geometry)
#include "../gdmf_interactions.h"
#include "../sprites/gdmf_sprites.h"
#include <math.h>
#define GDMF_INTERACTIONS_PI 3.14159265358979323846
static void gdmf_interactions_sprite_center(int sprite_index, float* output_x, float* output_y) {

    const float uniform_scale = GetSpriteScale(sprite_index);
    const float scale_x = uniform_scale * GetSpriteScaleX(sprite_index);
    const float scale_y = uniform_scale * GetSpriteScaleY(sprite_index);
    const float half_width = (SPRITE_WIDTH * scale_x) * 0.5f;
    const float half_height = (SPRITE_HEIGHT * scale_y) * 0.5f;
    const float hotspot_x = GetSpriteHotspotX(sprite_index);
    const float hotspot_y = GetSpriteHotspotY(sprite_index);

    *output_x = (GetSpriteX(sprite_index) - hotspot_x * scale_x) + half_width;
    *output_y = (GetSpriteY(sprite_index) - hotspot_y * scale_y) + half_height;

    return;
}

float GetInteractionsSpriteDistance(int first_sprite, int second_sprite) {
    if (first_sprite < 0 || first_sprite >= MAX_SPRITES || second_sprite < 0 ||
        second_sprite >= MAX_SPRITES) {
        return 0.0f;
    }
    float first_x, first_y, second_x, second_y;
    gdmf_interactions_sprite_center(first_sprite, &first_x, &first_y);
    gdmf_interactions_sprite_center(second_sprite, &second_x, &second_y);

    float delta_x = second_x - first_x;
    float delta_y = second_y - first_y;

    return sqrtf(delta_x * delta_x + delta_y * delta_y);
}

float GetInteractionsSpriteDirection(int first_sprite, int second_sprite) {
    if (first_sprite < 0 || first_sprite >= MAX_SPRITES || second_sprite < 0 ||
        second_sprite >= MAX_SPRITES) {
        return 0.0f;
    }
    float first_x, first_y, second_x, second_y;
    gdmf_interactions_sprite_center(first_sprite, &first_x, &first_y);
    gdmf_interactions_sprite_center(second_sprite, &second_x, &second_y);

    float direction_radians = atan2f(second_y - first_y, second_x - first_x);

    return direction_radians * (180.0f / (float)GDMF_INTERACTIONS_PI);
}

bool GetInteractionsSpritePixelWorldPosition(int sprite_index, int local_x, int local_y, float* world_x,
                                             float* world_y) {
    if (sprite_index < 0 || sprite_index >= MAX_SPRITES) {
        return false;
    }
    if (local_x < 0 || local_x >= SPRITE_WIDTH || local_y < 0 || local_y >= SPRITE_HEIGHT) {
        return false;
    }
    if (!world_x || !world_y) {
        return false;
    }

    int geometric_x = local_x, geometric_y = local_y;
    if (GetSpriteFlipX(sprite_index)) {
        geometric_x = SPRITE_WIDTH - 1 - geometric_x;
    }
    if (GetSpriteFlipY(sprite_index)) {
        geometric_y = SPRITE_HEIGHT - 1 - geometric_y;
    }

    const float uniform_scale = GetSpriteScale(sprite_index);
    float scaled_x =
        ((float)geometric_x - SPRITE_WIDTH * 0.5f) * (uniform_scale * GetSpriteScaleX(sprite_index));
    float scaled_y =
        ((float)geometric_y - SPRITE_HEIGHT * 0.5f) * (uniform_scale * GetSpriteScaleY(sprite_index));

    float skew_x = GetSpriteSkewX(sprite_index);
    float skew_y = GetSpriteSkewY(sprite_index);
    float sheared_x = scaled_x + skew_x * scaled_y;
    float sheared_y = skew_y * scaled_x + scaled_y;

    float rotation_radians = GetSpriteRotation(sprite_index) * ((float)GDMF_INTERACTIONS_PI / 180.0f);
    float cosine = cosf(rotation_radians), sine = sinf(rotation_radians);

    float center_x, center_y;
    gdmf_interactions_sprite_center(sprite_index, &center_x, &center_y);

    float pivot_x = center_x, pivot_y = center_y;
    if (GetSpriteRotateAroundHotspot(sprite_index)) {
        pivot_x = GetSpriteX(sprite_index);
        pivot_y = GetSpriteY(sprite_index);
    }

    float delta_x = (center_x + sheared_x) - pivot_x;
    float delta_y = (center_y + sheared_y) - pivot_y;

    *world_x = pivot_x + (cosine * delta_x - sine * delta_y);
    *world_y = pivot_y + (sine * delta_x + cosine * delta_y);

    return true;
}
