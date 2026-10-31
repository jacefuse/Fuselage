// Host lifetime and defaults. SEE: (GDMF_SPRITES.md, Lifecycle)
#include "gdmf_sprites_state_private.h"
#include "gdmf_sprites_backend.h"
#include "gdmf_sprites_collision_private.h"
#include <stdio.h>

static Sprite gdmf_sprites_instances[MAX_SPRITES];

Sprite* gdmf_sprites_state_get(int sprite_index) {
    return gdmf_sprites_index_valid(sprite_index) ? &gdmf_sprites_instances[sprite_index] : NULL;
}

int InitSprites(void) {
    const Sprite defaults = {
        .scale = 1.0f,
        .scaleX = 1.0f,
        .scaleY = 1.0f,
        .transparency = 255,
        .bitmapID = SPRITE_BITMAP_NONE,
        .collidableColors = 0xFFFE,
    };
    for (int sprite_index = 0; sprite_index < MAX_SPRITES; ++sprite_index) {
        gdmf_sprites_instances[sprite_index] = defaults;
        gdmf_sprites_geometry_invalidate(sprite_index);
    }
    gdmf_sprites_bitmaps_reset();

    unsigned char test_bitmap[GDMF_SPRITE_BITMAP_BYTES];
    for (int y = 0; y < SPRITE_HEIGHT; ++y) {
        for (int x = 0; x < SPRITE_WIDTH; x += 2) {
            int color_index = (y / 16) * 4 + x / 16;
            test_bitmap[(y * SPRITE_WIDTH + x) / 2] =
                (unsigned char)((color_index << 4) | color_index);
        }
    }
    UploadSpriteBitmap(SPRITE_TEST_PATTERN_BITMAP_ID, test_bitmap);
    gdmf_sprites_instances[0].bitmapID = SPRITE_TEST_PATTERN_BITMAP_ID;

    if (!gdmf_sprites_backend_initialize()) {
        fprintf(stderr, "[Sprites] Renderer setup unavailable; host state initialized\n");
    }
    return MAX_SPRITES;
}

void ShutdownSprites(void) {
    if (!gdmf_sprites_backend_shutdown()) {
        fprintf(stderr, "[Sprites] Shutdown retained backend resources\n");
        return;
    }
    gdmf_sprites_collision_search_shutdown();
}
