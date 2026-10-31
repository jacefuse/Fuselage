// Shared packed bitmap mirror and pending uploads. SEE: (GDMF_SPRITES.md, Bitmaps)
#include "gdmf_sprites_state_private.h"
#include <stdio.h>
#include <string.h>

static unsigned char gdmf_sprites_bitmap_bytes[MAX_SPRITE_BITMAPS][GDMF_SPRITE_BITMAP_BYTES];
static bool gdmf_sprites_bitmap_registered[MAX_SPRITE_BITMAPS];
static bool gdmf_sprites_bitmap_pending[MAX_SPRITE_BITMAPS];

void gdmf_sprites_bitmaps_reset(void) {
    memset(gdmf_sprites_bitmap_registered, 0, sizeof(gdmf_sprites_bitmap_registered));
    memset(gdmf_sprites_bitmap_pending, 0, sizeof(gdmf_sprites_bitmap_pending));
}

const unsigned char* gdmf_sprites_bitmap_data(SpriteBitmapID bitmap_id) {
    if (!gdmf_sprites_bitmap_id_valid(bitmap_id) || !gdmf_sprites_bitmap_registered[bitmap_id]) {
        return NULL;
    }
    return gdmf_sprites_bitmap_bytes[bitmap_id];
}

bool gdmf_sprites_bitmap_dirty(SpriteBitmapID bitmap_id) {
    return gdmf_sprites_bitmap_id_valid(bitmap_id) && gdmf_sprites_bitmap_registered[bitmap_id] &&
           gdmf_sprites_bitmap_pending[bitmap_id];
}

void gdmf_sprites_bitmap_mark_uploaded(SpriteBitmapID bitmap_id) {
    if (gdmf_sprites_bitmap_id_valid(bitmap_id)) {
        gdmf_sprites_bitmap_pending[bitmap_id] = false;
    }
}

void gdmf_sprites_bitmaps_mark_dirty(void) {
    for (int bitmap_id = 0; bitmap_id < MAX_SPRITE_BITMAPS; ++bitmap_id) {
        gdmf_sprites_bitmap_pending[bitmap_id] = gdmf_sprites_bitmap_registered[bitmap_id];
    }
}

bool UploadSpriteBitmap(SpriteBitmapID bitmap_id, const unsigned char* bitmap) {
    if (!gdmf_sprites_bitmap_id_valid(bitmap_id) || !bitmap) {
        fprintf(stderr, "[Sprites] Bitmap upload requires a valid slot and source\n");
        return false;
    }
    memcpy(gdmf_sprites_bitmap_bytes[bitmap_id], bitmap, GDMF_SPRITE_BITMAP_BYTES);
    gdmf_sprites_bitmap_registered[bitmap_id] = true;
    gdmf_sprites_bitmap_pending[bitmap_id] = true;
    return true;
}
