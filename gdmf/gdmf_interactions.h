// GDMF Interactions - cross-system utility layer.
// Built on top of the sprite/tile public APIs only, never their internals --
// this is what keeps gdmf_sprites.c and gdmf_tiles.c decoupled from each
// other while still letting game code ask cross-cutting questions ("how far
// apart are these two sprites") without hand-rolling the geometry itself.

#ifndef GDMF_INTERACTIONS_H
#define GDMF_INTERACTIONS_H

#include <stdbool.h>

#define GDMF_INTERACTIONS_VERSION "0.4.2026071601 DERRIERE"

// Straight-line distance between two sprites' centers (position + half the
// bitmap's scaled extent, the same center GDMF itself pivots
// rotation/scale/shear around) -- not their raw x/y, which is each
// sprite's top-left corner and would misrepresent distance between
// differently-scaled sprites. 0.0f if either index is out of range (same
// permissive-sentinel convention GetSpriteX/GetSpriteY already use).
float DistanceBetweenSprites(int spriteA, int spriteB);

// Angle in degrees from spriteA's center to spriteB's center, atan2
// convention (0 = spriteB directly to spriteA's +x side, increasing
// counter-clockwise), independent of either sprite's own rotation field --
// this is the direction between them in world space, not either sprite's
// facing. 0.0f if either index is out of range.
float DirectionBetweenSprites(int spriteA, int spriteB);

// Converts a local bitmap-pixel coordinate (0..SPRITE_WIDTH-1,
// 0..SPRITE_HEIGHT-1 -- the same space PLOT/bitmap data is authored in) to
// where that pixel currently lands in world/canvas space, given
// spriteIndex's live position/scale/rotation/skew/flip. The exact forward
// transform gdmf_sprites.c's internal WorldPixelToLocalBitmapPixel inverts
// to do hit-testing/collision the other direction -- this is that chain
// run forwards, built entirely from GetSprite*() calls, so a shape hidden
// inside a rotated/skewed/flipped sprite (e.g. "where's this character's
// hand right now") can be tracked in world space without reaching into
// gdmf_sprites.c at all. Returns false (outWorldX/Y left untouched) if
// spriteIndex or the local coordinate is out of range.
bool LocalBitmapPixelToWorldPixel(int spriteIndex, int localX, int localY,
                                   float* outWorldX, float* outWorldY);

#endif // GDMF_INTERACTIONS_H
