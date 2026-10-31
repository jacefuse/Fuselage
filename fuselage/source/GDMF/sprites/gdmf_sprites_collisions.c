// Automatic reports retain the original per-sprite order and fixed cap.
// SEE: (GDMF_SPRITES.md, Collision results)
#include "gdmf_sprites_collision_private.h"
#include <stddef.h>
#include <string.h>

static SpriteCollisionInfo gdmf_sprites_reports[MAX_SPRITES][MAX_COLLISIONS_PER_SPRITE];
static int gdmf_sprites_report_count[MAX_SPRITES];

static void gdmf_sprites_collision_record(int sprite_index, int other_sprite, SpriteCollisionType type) {
    int count = gdmf_sprites_report_count[sprite_index];
    if (count < MAX_COLLISIONS_PER_SPRITE) {
        gdmf_sprites_reports[sprite_index][count] = (SpriteCollisionInfo){other_sprite, type};
        gdmf_sprites_report_count[sprite_index] = count + 1;
    }
}

void gdmf_sprites_collisions_run(void) {
    memset(gdmf_sprites_report_count, 0, sizeof(gdmf_sprites_report_count));
    int targets[MAX_SPRITES];
    int target_count = 0;
    for (int sprite_index = 0; sprite_index < MAX_SPRITES; ++sprite_index) {
        gdmf_sprites_geometry_invalidate(sprite_index);
        if (gdmf_sprites_state_get(sprite_index)->enabled) {
            gdmf_sprites_bounds_get(sprite_index);
            targets[target_count++] = sprite_index;
        }
    }
    for (int reporter = 0; reporter < target_count; ++reporter) {
        int sprite_index = targets[reporter];
        unsigned char types = gdmf_sprites_state_get(sprite_index)->collisionTypes;
        if (!types) {
            continue;
        }
        const gdmf_sprite_bounds* bounds = gdmf_sprites_bounds_get(sprite_index);
        for (int target = 0; target < target_count; ++target) {
            int other_sprite = targets[target];
            if (sprite_index == other_sprite) {
                continue;
            }
            unsigned char matched = gdmf_sprites_collision_pair(sprite_index, other_sprite, types, bounds,
                                                                gdmf_sprites_bounds_get(other_sprite));
            if (matched & COLLISION_TYPE_BOUNDING_BOX) {
                gdmf_sprites_collision_record(sprite_index, other_sprite, COLLISION_TYPE_BOUNDING_BOX);
            }
            if (matched & COLLISION_TYPE_ANY_COLOR) {
                gdmf_sprites_collision_record(sprite_index, other_sprite, COLLISION_TYPE_ANY_COLOR);
            }
            if (matched & COLLISION_TYPE_COLLIDABLE_COLORS) {
                gdmf_sprites_collision_record(sprite_index, other_sprite, COLLISION_TYPE_COLLIDABLE_COLORS);
            }
        }
    }
}

bool HasSpriteCollision(int sprite_index) {
    return gdmf_sprites_index_valid(sprite_index) && gdmf_sprites_report_count[sprite_index] > 0;
}

int GetSpriteCollisionCount(int sprite_index) {
    return gdmf_sprites_index_valid(sprite_index) ? gdmf_sprites_report_count[sprite_index] : 0;
}

const SpriteCollisionInfo* GetSpriteCollisions(int sprite_index, int* output_count) {
    if (output_count) {
        *output_count = GetSpriteCollisionCount(sprite_index);
    }
    return gdmf_sprites_index_valid(sprite_index) ? gdmf_sprites_reports[sprite_index] : NULL;
}
