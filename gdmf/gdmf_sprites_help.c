#include "gdmf_sprites_help.h"

// Each helper is caller-side "current + delta" expressed through the absolute
// setter. Validation composes for free: GetSprite* returns a harmless default
// on a bad index and SetSprite* no-ops (returning false) on a bad index, so the
// relative op fails cleanly without a separate bounds check here.
bool MoveSpriteBy(int spriteIndex, float dx, float dy) {
    return SetSpritePosition(spriteIndex, GetSpriteX(spriteIndex) + dx,
                                          GetSpriteY(spriteIndex) + dy);
}

bool ScaleSpriteBy(int spriteIndex, float delta) {
    return SetSpriteScale(spriteIndex, GetSpriteScale(spriteIndex) + delta);
}

bool RotateSpriteBy(int spriteIndex, float delta) {
    return SetSpriteRotation(spriteIndex, GetSpriteRotation(spriteIndex) + delta);
}