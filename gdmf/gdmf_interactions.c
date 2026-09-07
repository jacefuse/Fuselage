// GDMF Interactions - see gdmf_interactions.h for the public contract.
// Built entirely on gdmf_sprites.h's public API -- no access to Sprite's
// private fields, no #include of gdmf_sprites.c or its internal header.

#include "gdmf_interactions.h"
#include "gdmf_sprites.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Both direction/distance functions pivot on each sprite's geometric center.
// x/y is where the sprite's HOTSPOT lands, not its top-left -- so the center is
// x/y shifted back by the (scaled) hotspot, then forward by a half-extent. With
// the default hotspot (0,0) that's just x/y + half, the old top-left behavior.
// This matches the same centerX/centerY gdmf_sprites.c computes before any
// rotation/scale (see WorldPixelToLocalBitmapPixel). Kept a local helper rather
// than exposed publicly since "sprite center" is a derived quantity, not stored
// state -- GetSpriteX/Y already answer "where is this sprite" for anything that
// isn't specifically about geometry relative to other sprites.
static void SpriteCenter(int spriteIndex, float* outX, float* outY) {
    float scale = GetSpriteScale(spriteIndex);
    float halfW = (SPRITE_WIDTH  * scale) * 0.5f;
    float halfH = (SPRITE_HEIGHT * scale) * 0.5f;
    float hotX = 0.0f, hotY = 0.0f;
    GetSpriteHotspot(spriteIndex, &hotX, &hotY);

    *outX = (GetSpriteX(spriteIndex) - hotX * scale) + halfW;
    *outY = (GetSpriteY(spriteIndex) - hotY * scale) + halfH;

    return;
}

float DistanceBetweenSprites(int spriteA, int spriteB) {
    float ax, ay, bx, by;
    SpriteCenter(spriteA, &ax, &ay);
    SpriteCenter(spriteB, &bx, &by);

    float dx = bx - ax;
    float dy = by - ay;

    return sqrtf(dx * dx + dy * dy);
}

float DirectionBetweenSprites(int spriteA, int spriteB) {
    float ax, ay, bx, by;
    SpriteCenter(spriteA, &ax, &ay);
    SpriteCenter(spriteB, &bx, &by);

    float radians = atan2f(by - ay, bx - ax);

    return radians * (180.0f / (float)M_PI);
}

bool LocalBitmapPixelToWorldPixel(int spriteIndex, int localX, int localY,
                                   float* outWorldX, float* outWorldY) {
    if (spriteIndex < 0 || spriteIndex >= MAX_SPRITES) { return false; }
    if (localX < 0 || localX >= SPRITE_WIDTH || localY < 0 || localY >= SPRITE_HEIGHT) { return false; }
    if (!outWorldX || !outWorldY) { return false; }

    // Un-flip first -- WorldPixelToLocalBitmapPixel's returned index has
    // already had flip applied as its last step, so recovering the
    // pre-flip "geometric" index the rest of this chain operates on means
    // applying the same (self-inverse) mirror again.
    unsigned char flip = GetSpriteFlip(spriteIndex);
    int geomX = localX, geomY = localY;
    if (flip & SPRITE_FLIP_X) { geomX = SPRITE_WIDTH  - 1 - geomX; }
    if (flip & SPRITE_FLIP_Y) { geomY = SPRITE_HEIGHT - 1 - geomY; }

    // Re-center to bitmap-origin and apply scale -- forward of
    // WorldPixelToLocalBitmapPixel's "undo scale, recenter" step.
    float scale = GetSpriteScale(spriteIndex);
    float lx = ((float)geomX - SPRITE_WIDTH  * 0.5f) * scale;
    float ly = ((float)geomY - SPRITE_HEIGHT * 0.5f) * scale;

    // Apply shear -- forward of the matrix WorldPixelToLocalBitmapPixel
    // inverts (that function's det/divide-by-zero guard is a reverse-only
    // concern; matrix multiplication forward is always well-defined).
    float skewX, skewY;
    GetSpriteSkew(spriteIndex, &skewX, &skewY);
    float sx = lx + skewX * ly;
    float sy = skewY * lx + ly;

    // Rotate about the pivot -- forward of the transpose
    // WorldPixelToLocalBitmapPixel uses to undo it. The pivot is the sprite
    // center by default, or the hotspot (the world point x/y) when the sprite
    // opts into rotate-around-hotspot -- same choice ComputeSpriteWorldQuad
    // makes, so this stays consistent with what's drawn. sx/sy are the corner's
    // offset from the center; with a center pivot dx/dy reduce to them and this
    // collapses to the old center-rotation.
    float angle = GetSpriteRotation(spriteIndex) * ((float)M_PI / 180.0f);
    float cosA = cosf(angle), sinA = sinf(angle);

    float centerX, centerY;
    SpriteCenter(spriteIndex, &centerX, &centerY);

    float pivotX = centerX, pivotY = centerY;
    if (GetSpriteRotateAroundHotspot(spriteIndex)) {
        pivotX = GetSpriteX(spriteIndex);   // the hotspot's world point
        pivotY = GetSpriteY(spriteIndex);
    }

    float dx = (centerX + sx) - pivotX;
    float dy = (centerY + sy) - pivotY;

    *outWorldX = pivotX + (cosA * dx - sinA * dy);
    *outWorldY = pivotY + (sinA * dx + cosA * dy);

    return true;
}
