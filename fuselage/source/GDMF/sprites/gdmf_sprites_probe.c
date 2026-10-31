// Inverse transforms and packed sampling. SEE: (GDMF_SPRITES.md, Point sampling)
#include "gdmf_sprites_probe.h"
#include "gdmf_sprites_state_private.h"
#include <limits.h>
#include <math.h>

static void gdmf_sprite_probe_from_state(const Sprite* sprite, gdmf_sprite_probe* probe) {
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
    probe->cosine = cosf(angle);
    probe->sine = sinf(angle);
    probe->pivot_x = pivot_x;
    probe->pivot_y = pivot_y;
    probe->center_offset_x = pivot_x - center_x;
    probe->center_offset_y = pivot_y - center_y;
    probe->skew_x = sprite->skewX;
    probe->skew_y = sprite->skewY;
    probe->determinant = 1.0f - sprite->skewX * sprite->skewY;
    probe->scale_x = scale_x;
    probe->scale_y = scale_y;
    probe->flip_x = (sprite->flip & SPRITE_FLIP_X) != 0;
    probe->flip_y = (sprite->flip & SPRITE_FLIP_Y) != 0;
    probe->bitmap = gdmf_sprites_bitmap_data(sprite->bitmapID);
    probe->invertible = !(fabsf(probe->determinant) < 1e-6f) && isfinite(probe->determinant) &&
                        isfinite(probe->cosine) && isfinite(probe->sine) && isfinite(pivot_x) &&
                        isfinite(pivot_y) && isfinite(probe->center_offset_x) &&
                        isfinite(probe->center_offset_y) && isfinite(scale_x) && isfinite(scale_y) &&
                        scale_x > 0 && scale_y > 0;
}

bool gdmf_sprite_probe_init(int sprite_index, gdmf_sprite_probe* probe) {
    const Sprite* sprite = gdmf_sprites_state_get(sprite_index);
    if (!sprite || !probe) {
        return false;
    }
    gdmf_sprite_probe_from_state(sprite, probe);
    return true;
}

bool gdmf_sprite_probe_local(const gdmf_sprite_probe* probe, float world_x, float world_y, int* local_x,
                             int* local_y) {
    if (!probe->invertible) {
        return false;
    }
    float delta_x = world_x - probe->pivot_x;
    float delta_y = world_y - probe->pivot_y;
    float sheared_x = (probe->cosine * delta_x + probe->sine * delta_y) + probe->center_offset_x;
    float sheared_y = (-probe->sine * delta_x + probe->cosine * delta_y) + probe->center_offset_y;
    float unscaled_x = (sheared_x - probe->skew_x * sheared_y) / probe->determinant;
    float unscaled_y = (-probe->skew_y * sheared_x + sheared_y) / probe->determinant;
    float bitmap_x = unscaled_x / probe->scale_x + SPRITE_WIDTH * 0.5f;
    float bitmap_y = unscaled_y / probe->scale_y + SPRITE_HEIGHT * 0.5f;

    // Reject before conversion, retaining the existing truncation toward zero.
    if (!(bitmap_x > -1.0f && bitmap_x < SPRITE_WIDTH && bitmap_y > -1.0f && bitmap_y < SPRITE_HEIGHT)) {
        return false;
    }
    int pixel_x = (int)bitmap_x;
    int pixel_y = (int)bitmap_y;
    if (probe->flip_x) {
        pixel_x = SPRITE_WIDTH - 1 - pixel_x;
    }
    if (probe->flip_y) {
        pixel_y = SPRITE_HEIGHT - 1 - pixel_y;
    }
    *local_x = pixel_x;
    *local_y = pixel_y;
    return true;
}

unsigned char gdmf_sprite_probe_sample(const gdmf_sprite_probe* probe, int local_x, int local_y) {
    unsigned char packed = probe->bitmap[(local_y * SPRITE_WIDTH + local_x) / 2];
    return (local_x % 2 == 0) ? ((packed >> 4) & 0x0F) : (packed & 0x0F);
}

int gdmf_sprite_probe_pixel(const gdmf_sprite_probe* probe, float world_x, float world_y) {
    if (!probe->bitmap) {
        return -1;
    }
    int local_x, local_y;
    if (!gdmf_sprite_probe_local(probe, world_x, world_y, &local_x, &local_y)) {
        return -1;
    }
    return (int)gdmf_sprite_probe_sample(probe, local_x, local_y);
}

// Intersect a row's affine local coordinate with the accepted truncation band.
static bool gdmf_sprite_probe_axis_span(double slope, double intercept, double size, double* minimum,
                                        double* maximum) {
    const double lower = -1.0 - 0.01, upper = size + 0.01;
    if (fabs(slope) < 1e-12) {
        if (intercept <= lower || intercept >= upper) {
            return false;
        }
        *minimum = -HUGE_VAL;
        *maximum = HUGE_VAL;
        return true;
    }
    double first = (lower - intercept) / slope, last = (upper - intercept) / slope;
    if (first > last) {
        double temporary = first;
        first = last;
        last = temporary;
    }
    *minimum = first;
    *maximum = last;
    return true;
}

bool gdmf_sprite_probe_row_span(const gdmf_sprite_probe* probe, int world_y, int* first_x, int* end_x) {
    if (!probe->invertible) {
        return false;
    }
    const double determinant = probe->determinant, scale_x = probe->scale_x, scale_y = probe->scale_y;
    const double cosine = probe->cosine, sine = probe->sine;
    const double skew_x = probe->skew_x, skew_y = probe->skew_y;
    if (!(fabs(determinant * scale_x) > 1e-12) || !(fabs(determinant * scale_y) > 1e-12)) {
        *first_x = INT_MIN / 2;
        *end_x = INT_MAX / 2;
        return true;
    }
    const double delta_y = (double)world_y - probe->pivot_y;
    const double sheared_origin_x = -cosine * probe->pivot_x + sine * delta_y + probe->center_offset_x;
    const double sheared_origin_y = sine * probe->pivot_x + cosine * delta_y + probe->center_offset_y;
    const double slope_x = (cosine + skew_x * sine) / (determinant * scale_x);
    const double intercept_x =
        (sheared_origin_x - skew_x * sheared_origin_y) / (determinant * scale_x) + SPRITE_WIDTH * 0.5;
    const double slope_y = (-skew_y * cosine - sine) / (determinant * scale_y);
    const double intercept_y =
        (-skew_y * sheared_origin_x + sheared_origin_y) / (determinant * scale_y) + SPRITE_HEIGHT * 0.5;
    double minimum_x, maximum_x, minimum_y, maximum_y;
    if (!gdmf_sprite_probe_axis_span(slope_x, intercept_x, SPRITE_WIDTH, &minimum_x, &maximum_x) ||
        !gdmf_sprite_probe_axis_span(slope_y, intercept_y, SPRITE_HEIGHT, &minimum_y, &maximum_y)) {
        return false;
    }
    double minimum = fmax(minimum_x, minimum_y), maximum = fmin(maximum_x, maximum_y);
    if (!(minimum <= maximum)) {
        return false;
    }
    minimum = floor(minimum) - 2.0;
    maximum = ceil(maximum) + 3.0;
    // Clamp both ends before converting, including entirely off-range spans.
    minimum = fmax(INT_MIN / 2, fmin(INT_MAX / 2, minimum));
    maximum = fmax(INT_MIN / 2, fmin(INT_MAX / 2, maximum));
    if (!(minimum < maximum)) {
        return false;
    }
    *first_x = (int)minimum;
    *end_x = (int)maximum;
    return true;
}

bool IsSpriteWorldPointInside(int sprite_index, float world_x, float world_y) {
    gdmf_sprite_probe probe;
    int local_x, local_y;
    return gdmf_sprite_probe_init(sprite_index, &probe) &&
           gdmf_sprite_probe_local(&probe, world_x, world_y, &local_x, &local_y);
}

int GetSpritePixelAtWorldPoint(int sprite_index, float world_x, float world_y) {
    gdmf_sprite_probe probe;
    return gdmf_sprite_probe_init(sprite_index, &probe) ? gdmf_sprite_probe_pixel(&probe, world_x, world_y)
                                                        : -1;
}
