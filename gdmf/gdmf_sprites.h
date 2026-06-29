#pragma once

// GDMF Sprites - BUTTOCKS sprite subsystem.
// Bitmaps (raw 4-bit packed shape data) and sprite instances are decoupled:
// a bitmap is registered once into an atlas slot via UploadSpriteBitmap, and
// any number of sprite instances may reference that same slot. Color is never
// baked in -- each instance's palette is applied at render time in the
// fragment shader, so changing a sprite's appearance is just a field write.

#ifndef FUSELAGE_SPRITES_H
#define FUSELAGE_SPRITES_H

#include <stdint.h>
#include <stdbool.h>

#define GDMF_SPRITES_VERSION "0.2.2026062901 BUTTOCKS"

#define SPRITE_WIDTH        64
#define SPRITE_HEIGHT       64
#define MAX_SPRITE_BITMAPS  1024   // atlas slot count -- distinct registered shapes
#define MAX_SPRITES         640    // sprite instance count
#define SPRITE_BITMAP_NONE  -1     // sentinel: no bitmap assigned
#define SPRITE_TEST_PATTERN_BITMAP_ID 0  // bitmap slot reserved for the generated checkerboard test pattern (see InitSprites)

// Index into the bitmap atlas. Comfortably covers MAX_SPRITE_BITMAPS.
typedef int16_t SpriteBitmapID;

// Which collision metric(s) a sprite wants tested -- bitflags, since a
// sprite may want more than one active at once. Tested in full
// post-transform space (scale + rotation + skew all accounted for).
typedef enum {
    COLLISION_TYPE_NONE              = 0,
    COLLISION_TYPE_BOUNDING_BOX      = 1 << 0,  // rotated/skewed AABB overlap only, no per-pixel test
    COLLISION_TYPE_ANY_COLOR         = 1 << 1,  // full silhouette: any palette index != 0 on both sprites
    COLLISION_TYPE_COLLIDABLE_COLORS = 1 << 2,  // collidableColors bitmask, both sprites' own masks
} SpriteCollisionType;

// Mirrors a sprite's bitmap along one or both axes at render/collision time
// -- no atlas changes, no second copy of the bitmap. Lets one uploaded
// bitmap (e.g. a character) serve both orientations (e.g. facing left and
// right) instead of needing a pre-mirrored second atlas slot. Applied to
// UV generation in gdmf_sprites_prepare() and to local pixel sampling in
// WorldPixelToLocalBitmapPixel -- both paths read the same flip bits, so
// what's drawn and what's hit-tested always agree.
typedef enum {
    SPRITE_FLIP_NONE = 0,
    SPRITE_FLIP_X    = 1 << 0,  // mirror horizontally
    SPRITE_FLIP_Y    = 1 << 1,  // mirror vertically
} SpriteFlip;

// One match of one type against one other sprite. A sprite that matches
// two types against the same other sprite in the same frame gets two of
// these, never one entry with two bits set -- keeps "how many" and "which
// types" both answerable by just counting/iterating this list.
typedef struct {
    int                 otherSprite;
    SpriteCollisionType type;
} SpriteCollisionInfo;

#define MAX_COLLISIONS_PER_SPRITE 32  // per-sprite, per-frame cap; excess matches are silently dropped

// Sprite instance. Lives in a private array inside the subsystem --
// always go through the functions below, never reach into it directly.
typedef struct {
    float x;                          // x coordinate
    float y;                          // y coordinate
    float scale;                      // uniform scale factor
    float rotation;                   // rotation angle in degrees
    float skewX;                      // horizontal shear
    float skewY;                      // vertical shear
    unsigned char transparency;       // overall alpha (0-255)
    unsigned char priority;           // depth value -- lower draws further back
    unsigned char palette;            // which of the 256 16-color palettes to apply at render time
    SpriteBitmapID bitmapID;          // atlas slot this instance displays, or SPRITE_BITMAP_NONE
    bool enabled;                     // is the sprite active for logic/collision/rendering at all?
    bool visible;                     // should an enabled sprite be rendered? (ignored while disabled)
    bool showzero;                    // should palette index 0 render, or stay transparent?
    unsigned short collidableColors;  // bitmask of which palette indices (0-15) are collidable
    unsigned char collisionTypes;     // bitmask of SpriteCollisionType this sprite wants reported for itself
    unsigned char flip;               // bitmask of SpriteFlip -- mirrors the bitmap, no atlas cost
} Sprite;

// Lifecycle
int  InitSprites(void);
void ShutdownSprites(void);

// Bitmap atlas -- registers/overwrites the shape at a specific slot.
// bitmap is the caller's raw packed 4-bit indexed source data
// (SPRITE_WIDTH * SPRITE_HEIGHT pixels, 2 pixels per byte). The upload
// unconditionally overwrites the entire slot; no prior content can leak through.
bool UploadSpriteBitmap(SpriteBitmapID bitmapID, const unsigned char* bitmap);

// Sprite instance management -- the only way to touch sprite state from outside.
bool           AssignSprite(int spriteIndex, SpriteBitmapID bitmapID);
bool           AssignSpriteBitmapFromSprite(int spriteSource, int spriteDestination);
SpriteBitmapID GetSpriteBitmapID(int spriteIndex);

// Matches ANUS's ClearSprite as closely as the architecture allows:
// disables the sprite and unassigns its bitmap, leaving every other field
// (position, scale, rotation, skew, flip, transparency, priority, palette,
// visible, showzero, collidableColors, collisionTypes) untouched, same as
// ANUS left them. ANUS additionally re-blanked its per-sprite atlas region
// (its atlas was indexed per sprite instance) -- BUTTOCKS' atlas is
// indexed per bitmap and shared across instances, so there is no
// equivalent region to clear; unassigning the bitmap reference already
// makes the sprite skip rendering entirely (same visual effect) without
// risking erasing a bitmap slot other sprites may still be using.
void ClearSprite(int spriteIndex);

// Matches ANUS's SpriteTestPattern: assigns the generated checkerboard
// bitmap to spriteIndex, nothing else -- enabled/visible/showzero are
// left for the caller to set, same as ANUS did (it left those same three
// calls commented out in its own implementation).
void SpriteTestPattern(int spriteIndex);

// Disabled means inert: no collision participation (see RunSpriteCollisions)
// and no rendering, regardless of the visible flag below. Re-enabling a
// sprite picks its visible flag back up as-is -- nothing about visible is
// touched by toggling enabled.
bool GetSpriteEnabled(int spriteIndex);
bool SetSpriteEnabled(int spriteIndex, bool enabled);
bool ToggleSpriteEnabled(int spriteIndex);

// Pure render toggle, but only takes effect while the sprite is also
// enabled -- a disabled sprite stays unrendered no matter what visible is
// set to. Logic/collision are untouched by this flag either way.
bool GetSpriteVisible(int spriteIndex);
bool SetSpriteVisible(int spriteIndex, bool visible);
bool ToggleSpriteVisible(int spriteIndex);

// Both are ungated -- neither requires the sprite to be enabled/visible,
// so a position (absolute or relative) set while a sprite is inactive is
// still honored whenever it's activated later. SetSpritePosition sets an
// absolute position; UpdateSpritePosition adds a delta to the current one.
void  SetSpritePosition(int spriteIndex, float x, float y);
void  UpdateSpritePosition(int spriteIndex, float deltaX, float deltaY);
float GetSpriteX(int spriteIndex);
float GetSpriteY(int spriteIndex);

void  SetSpriteScale(int spriteIndex, float scale);
float GetSpriteScale(int spriteIndex);
float ChangeSpriteScale(int spriteIndex, float delta);

void  SetSpriteRotation(int spriteIndex, float rotation);
float GetSpriteRotation(int spriteIndex);
float ChangeSpriteRotation(int spriteIndex, float delta);

void SetSpriteSkew(int spriteIndex, float skewX, float skewY);
void GetSpriteSkew(int spriteIndex, float* skewX, float* skewY);

// Bitmask of SpriteFlip (SPRITE_FLIP_X / SPRITE_FLIP_Y, OR'd together for
// both axes). Mirrors the bitmap at render time with no atlas cost --
// applied consistently to both the render path and pixel collision.
unsigned char SetSpriteFlip(int spriteIndex, unsigned char flipMask);
unsigned char GetSpriteFlip(int spriteIndex);

void          SetSpritePriority(int spriteIndex, unsigned char priority);
unsigned char GetSpritePriority(int spriteIndex);

bool          SetSpriteColorPalette(int spriteIndex, unsigned char palette);
unsigned char GetSpriteColorPalette(int spriteIndex);

void          SetSpriteTransparency(int spriteIndex, unsigned char transparency);
unsigned char GetSpriteTransparency(int spriteIndex);

void SpriteShowZero(int spriteIndex, bool showzero);

unsigned short SetSpriteCollidableColors(int spriteIndex, unsigned short mask);
unsigned short GetSpriteCollidableColors(int spriteIndex);

unsigned char SetSpriteCollisionTypes(int spriteIndex, unsigned char typeMask);
unsigned char GetSpriteCollisionTypes(int spriteIndex);

// Collision -- pure CPU, reads each sprite's raw bitmap from a private
// CPU-side mirror of the atlas (kept in step by UploadSpriteBitmap).
// Runs automatically once per frame (see gdmf_sprites_prepare) against
// every pair of enabled sprites, fully accounting for scale, rotation,
// and skew -- not just a bounding box. Which types get reported for a
// given sprite is governed entirely by that sprite's own collisionTypes
// mask: sprite A can be flagged to detect a collision with sprite B even
// if B itself never opted into anything, or cares about different types
// entirely. Results are valid until the next frame's pass overwrites them.
bool                       SpriteHasCollision(int spriteIndex);
int                        GetSpriteCollisionCount(int spriteIndex);
const SpriteCollisionInfo* GetSpriteCollisions(int spriteIndex, int* outCount);

// Explicit, on-demand test between two specific sprites, independent of
// the automatic per-frame pass above: ignores both sprites' `enabled` flag
// and `collisionTypes` field entirely (the caller says exactly what to
// test via typesToTest), and never touches the per-frame result arrays --
// calling this has no effect on SpriteHasCollision/GetSpriteCollisions for
// either sprite, and vice versa. Returns a bitmask of which requested
// types actually matched (0 if none, or if either index is invalid).
unsigned char CheckSpritePairCollision(int spriteIndexA, int spriteIndexB, unsigned char typesToTest);

// Geometry-only containment test: is world point (worldX,worldY) within
// spriteIndex's actual transformed footprint (accounting for position,
// scale, rotation, and shear -- not flip, which never changes geometry),
// rather than just its axis-aligned bounding box? Use this where you want
// to know "is this point really inside the visible shape" without
// depending on per-pixel color/flip correctness -- e.g. hit-testing a
// click against a sprite whose flip/rotation accuracy is itself what's
// being tested.
bool WorldPointOnSprite(int spriteIndex, float worldX, float worldY);

// Atlas debug view -- lays out every currently-uploaded bitmap slot in a
// grid that fills the screen (scaled up to 2x if there's room to spare,
// scaled down to fit if there isn't), rendered as raw grayscale since the
// atlas itself stores no color -- not via any palette, intrinsic to the
// draw itself. Uses a reserved block of sprite indices at the top of the
// MAX_SPRITES range, so it never disturbs whatever sprites the game is
// already using.
void ToggleSpriteAtlasView(void);
bool GetSpriteAtlasViewActive(void);

#endif // FUSELAGE_SPRITES_H
