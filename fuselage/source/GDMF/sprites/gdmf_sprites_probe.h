#ifndef GDMF_SPRITES_PROBE_H
#define GDMF_SPRITES_PROBE_H

#include "gdmf_sprites.h"
#include <stdbool.h>

// Internal transform snapshot with borrowed packed bitmap storage. Use within
// one serialized query; upload/reset can invalidate its contents.
// SEE: (GDMF_SPRITES.md, Point sampling)
typedef struct {
    const unsigned char* bitmap;
    float pivot_x, pivot_y;
    float center_offset_x, center_offset_y;
    float cosine, sine;
    float skew_x, skew_y, determinant;
    float scale_x, scale_y;
    bool invertible;
    bool flip_x, flip_y;
} gdmf_sprite_probe;

// False for invalid index/NULL output. A valid index can produce a noninvertible probe.
bool gdmf_sprite_probe_init(int sprite_index, gdmf_sprite_probe* probe);
// The per-pixel half is inline: collision walks in other files call it for every
// covered pixel. SEE: (GDMF_INTERACTIONS.md, Tile collisions)

// False leaves outputs untouched. All pointers must be non-NULL.
static inline bool gdmf_sprite_probe_local(const gdmf_sprite_probe* probe, float world_x, float world_y,
                                           int* local_x, int* local_y) {
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
// Requires registered bitmap and valid local coordinates; no transform is applied.
static inline unsigned char gdmf_sprite_probe_sample(const gdmf_sprite_probe* probe, int local_x, int local_y) {
    unsigned char packed = probe->bitmap[(local_y * SPRITE_WIDTH + local_x) / 2];
    return (local_x % 2 == 0) ? ((packed >> 4) & 0x0F) : (packed & 0x0F);
}
// Palette index 0..15 or -1 when sampling is unavailable.
static inline int gdmf_sprite_probe_pixel(const gdmf_sprite_probe* probe, float world_x, float world_y) {
    if (!probe->bitmap) {
        return -1;
    }
    int local_x, local_y;
    if (!gdmf_sprite_probe_local(probe, world_x, world_y, &local_x, &local_y)) {
        return -1;
    }
    return (int)gdmf_sprite_probe_sample(probe, local_x, local_y);
}
// Candidate integer x range [first_x, end_x) for one world row. May be wider than
// accepted samples; not an exact footprint. Requires initialized probe/non-NULL outputs.
// Invalid/noninvertible probes or empty ranges return false without output writes.
bool gdmf_sprite_probe_row_span(const gdmf_sprite_probe* probe, int world_y, int* first_x, int* end_x);

#endif
