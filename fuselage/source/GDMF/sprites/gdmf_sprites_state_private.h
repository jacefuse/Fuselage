#ifndef GDMF_SPRITES_STATE_PRIVATE_H
#define GDMF_SPRITES_STATE_PRIVATE_H

// Borrowed host state, serialized with preparation and collision queries.
#include "gdmf_sprites.h"

#define GDMF_SPRITE_BITMAP_BYTES (SPRITE_WIDTH * SPRITE_HEIGHT / 2)

static inline bool gdmf_sprites_index_valid(int sprite_index) {
    return sprite_index >= 0 && sprite_index < MAX_SPRITES;
}

static inline bool gdmf_sprites_bitmap_id_valid(SpriteBitmapID bitmap_id) {
    return bitmap_id >= 0 && bitmap_id < MAX_SPRITE_BITMAPS;
}

// Valid indices are accessible even before InitSprites, matching the original API.
Sprite* gdmf_sprites_state_get(int sprite_index);
// Implemented by the geometry owner; successful geometry setters invalidate its cache.
void gdmf_sprites_geometry_invalidate(int sprite_index);

void gdmf_sprites_bitmaps_reset(void);
// NULL for an invalid or unregistered bitmap. Read-only borrowed packed bytes.
const unsigned char* gdmf_sprites_bitmap_data(SpriteBitmapID bitmap_id);
bool gdmf_sprites_bitmap_dirty(SpriteBitmapID bitmap_id);
// Renderer bookkeeping under the same state lock; recording is not completion.
void gdmf_sprites_bitmap_mark_uploaded(SpriteBitmapID bitmap_id);
void gdmf_sprites_bitmaps_mark_dirty(void);

#endif
