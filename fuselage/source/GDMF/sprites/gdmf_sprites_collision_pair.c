// Explicit pair testing. SEE: (GDMF_SPRITES.md, Collision results)
#include "gdmf_sprites_collision_private.h"
#include "gdmf_sprites_probe.h"
#include <limits.h>

unsigned char gdmf_sprites_collision_pair(int first_sprite, int second_sprite, unsigned char types,
                                          const gdmf_sprite_bounds* first_bounds,
                                          const gdmf_sprite_bounds* second_bounds) {
    if (!gdmf_sprites_bounds_finite(first_bounds) || !gdmf_sprites_bounds_finite(second_bounds) ||
        first_bounds->minimum_x >= second_bounds->maximum_x ||
        first_bounds->maximum_x <= second_bounds->minimum_x ||
        first_bounds->minimum_y >= second_bounds->maximum_y ||
        first_bounds->maximum_y <= second_bounds->minimum_y) {
        return 0;
    }
    unsigned char matched = types & COLLISION_TYPE_BOUNDING_BOX;
    unsigned char pixel_types = types & (COLLISION_TYPE_ANY_COLOR | COLLISION_TYPE_COLLIDABLE_COLORS);
    if (!pixel_types) {
        return matched;
    }
    gdmf_sprite_probe first_probe, second_probe;
    gdmf_sprite_probe_init(first_sprite, &first_probe);
    gdmf_sprite_probe_init(second_sprite, &second_probe);
    if (!first_probe.bitmap || !second_probe.bitmap || !first_probe.invertible || !second_probe.invertible) {
        return matched;
    }
    float minimum_x = fmaxf(first_bounds->minimum_x, second_bounds->minimum_x);
    float minimum_y = fmaxf(first_bounds->minimum_y, second_bounds->minimum_y);
    float maximum_x = fminf(first_bounds->maximum_x, second_bounds->maximum_x);
    float maximum_y = fminf(first_bounds->maximum_y, second_bounds->maximum_y);
    // Keep truncation for ordinary queries; refuse unrepresentable walk endpoints.
    if ((double)minimum_x < INT_MIN || (double)minimum_y < INT_MIN || (double)maximum_x > INT_MAX ||
        (double)maximum_y > INT_MAX) {
        return matched;
    }
    int first_x = (int)minimum_x, first_y = (int)minimum_y;
    int end_x = (int)maximum_x, end_y = (int)maximum_y;
    bool need_any = (pixel_types & COLLISION_TYPE_ANY_COLOR) != 0;
    bool need_collidable = (pixel_types & COLLISION_TYPE_COLLIDABLE_COLORS) != 0;
    const Sprite* first = gdmf_sprites_state_get(first_sprite);
    const Sprite* second = gdmf_sprites_state_get(second_sprite);

    for (int world_y = first_y; world_y < end_y && (need_any || need_collidable); ++world_y) {
        int first_start, first_end, second_start, second_end;
        if (!gdmf_sprite_probe_row_span(&first_probe, world_y, &first_start, &first_end) ||
            !gdmf_sprite_probe_row_span(&second_probe, world_y, &second_start, &second_end)) {
            continue;
        }
        int row_start = first_x, row_end = end_x;
        if (first_start > row_start) {
            row_start = first_start;
        }
        if (second_start > row_start) {
            row_start = second_start;
        }
        if (first_end < row_end) {
            row_end = first_end;
        }
        if (second_end < row_end) {
            row_end = second_end;
        }
        for (int world_x = row_start; world_x < row_end && (need_any || need_collidable); ++world_x) {
            int local_first_x, local_first_y, local_second_x, local_second_y;
            if (!gdmf_sprite_probe_local(&first_probe, (float)world_x, (float)world_y, &local_first_x,
                                         &local_first_y)) {
                continue;
            }
            unsigned char first_color = gdmf_sprite_probe_sample(&first_probe, local_first_x, local_first_y);
            if (!(need_any && first_color != 0) &&
                !(need_collidable && (first->collidableColors & (1 << first_color)))) {
                continue;
            }
            if (!gdmf_sprite_probe_local(&second_probe, (float)world_x, (float)world_y, &local_second_x,
                                         &local_second_y)) {
                continue;
            }
            unsigned char second_color =
                gdmf_sprite_probe_sample(&second_probe, local_second_x, local_second_y);
            if (need_any && first_color != 0 && second_color != 0) {
                matched |= COLLISION_TYPE_ANY_COLOR;
                need_any = false;
            }
            if (need_collidable && (first->collidableColors & (1 << first_color)) &&
                (second->collidableColors & (1 << second_color))) {
                matched |= COLLISION_TYPE_COLLIDABLE_COLORS;
                need_collidable = false;
            }
        }
    }
    return matched;
}

unsigned char CheckSpritePairCollision(int first_sprite, int second_sprite, unsigned char types_to_test) {
    if (!gdmf_sprites_index_valid(first_sprite) || !gdmf_sprites_index_valid(second_sprite) ||
        first_sprite == second_sprite) {
        return 0;
    }
    return gdmf_sprites_collision_pair(first_sprite, second_sprite, types_to_test,
                                       gdmf_sprites_bounds_get(first_sprite),
                                       gdmf_sprites_bounds_get(second_sprite));
}
