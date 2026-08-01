// GDMF Sprites - COLON sprite subsystem.
// Bitmaps (raw 4-bit packed shape data) and sprite instances are decoupled:
// a bitmap is registered once into an atlas slot via UploadSpriteBitmap, and
// any number of sprite instances may reference that same slot. Color is never
// baked in -- each instance's palette is applied at render time in the
// fragment shader, so changing a sprite's appearance is just a field write.

#ifndef GDMF_SPRITES_H
#define GDMF_SPRITES_H

#include <stdint.h>
#include <stdbool.h>

#define GDMF_SPRITES_VERSION "0.3.2026071601 COLON"

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
    float hotspotX;                   // anchor point X in unscaled sprite-local pixels (TL origin); MAY lie outside [0,SPRITE_WIDTH]
    float hotspotY;                   // anchor point Y in unscaled sprite-local pixels (TL origin); MAY lie outside [0,SPRITE_HEIGHT]
    bool  rotateAroundHotspot;        // false: rotate about the sprite center (default); true: rotate about the hotspot
    unsigned char transparency;       // overall alpha (0-255)
    unsigned char priority;           // depth value -- lower draws closer to front (drawn last); higher draws further back
    unsigned char palette;            // which of the 256 16-color palettes to apply at render time
    SpriteBitmapID bitmapID;          // atlas slot this instance displays, or SPRITE_BITMAP_NONE
    bool enabled;                     // is the sprite active for logic/collision/rendering at all?
    bool visible;                     // should an enabled sprite be rendered? (ignored while disabled)
    bool showzero;                    // should palette index 0 render, or stay transparent?
    unsigned short collidableColors;  // bitmask of which palette indices (0-15) are collidable
    unsigned char collisionTypes;     // bitmask of SpriteCollisionType this sprite wants reported for itself
    unsigned char flip;               // bitmask of SpriteFlip -- mirrors the bitmap, no atlas cost
} Sprite;

// API return convention: every function that performs an action returns bool
// -- true on success, false on any failure (an invalid sprite index or bitmap
// ID, or a failing subsystem). A bad index is reported, never silently
// swallowed; the caller decides whether to check. Getters are the exception:
// they return their value, with a documented default when the index is invalid.

// Lifecycle
int  InitSprites(void);
void ShutdownSprites(void);

// Bitmap atlas -- registers/overwrites the shape at a specific slot.
// bitmap is the caller's raw packed 4-bit indexed source data
// (SPRITE_WIDTH * SPRITE_HEIGHT pixels, 2 pixels per byte). The upload
// unconditionally overwrites the entire slot; no prior content can leak through.
// Returns true on success, false if the bitmap ID is out of range or data is NULL.
bool UploadSpriteBitmap(SpriteBitmapID bitmapID, const unsigned char* bitmap);

// Sprite instance management -- the only way to touch sprite state from outside.
// Both validate their arguments and return true on success, false on an
// out-of-range sprite index or bitmap ID (these reference the shared atlas,
// so a bad reference is a real, reportable failure -- unlike the plain field
// setters below, which silently no-op an invalid index).
bool           AssignSprite(int spriteIndex, SpriteBitmapID bitmapID);
bool           AssignSpriteBitmapFromSprite(int spriteSource, int spriteDestination);
SpriteBitmapID GetSpriteBitmapID(int spriteIndex);

// Matches ANUS's ClearSprite as closely as the architecture allows:
// disables the sprite and unassigns its bitmap, leaving every other field
// (position, scale, rotation, skew, flip, transparency, priority, palette,
// visible, showzero, collidableColors, collisionTypes) untouched, same as
// ANUS left them. ANUS additionally re-blanked its per-sprite atlas region
// (its atlas was indexed per sprite instance) -- GDMF's atlas is
// indexed per bitmap and shared across instances, so there is no
// equivalent region to clear; unassigning the bitmap reference already
// makes the sprite skip rendering entirely (same visual effect) without
// risking erasing a bitmap slot other sprites may still be using.
bool ClearSprite(int spriteIndex);

// ANUS's SpriteTestPattern, renamed to VSN form: assigns the generated
// checkerboard bitmap to spriteIndex, nothing else -- enabled/visible/showzero
// are left for the caller to set, same as ANUS did (it left those same three
// calls commented out in its own implementation).
bool AssignSpriteTestPattern(int spriteIndex);

// Disabled means inert: no collision participation (see RunSpriteCollisions)
// and no rendering, regardless of the visible flag below. Re-enabling a
// sprite picks its visible flag back up as-is -- nothing about visible is
// touched by toggling enabled. Set/Toggle return success, never the resulting
// state -- read that with the getter.
bool GetSpriteEnabled(int spriteIndex);
bool SetSpriteEnabled(int spriteIndex, bool enabled);
bool ToggleSpriteEnabled(int spriteIndex);

// Pure render toggle, but only takes effect while the sprite is also
// enabled -- a disabled sprite stays unrendered no matter what visible is
// set to. Logic/collision are untouched by this flag either way.
bool GetSpriteVisible(int spriteIndex);
bool SetSpriteVisible(int spriteIndex, bool visible);
bool ToggleSpriteVisible(int spriteIndex);

// Ungated -- does not require the sprite to be enabled/visible, so a position
// set while a sprite is inactive is still honored whenever it's activated
// later. Positions are absolute: sprites are treated as state registers, so a
// relative move is the caller's to compute (new = current + delta) and apply
// through the absolute setter.
bool  SetSpritePosition(int spriteIndex, float x, float y);
float GetSpriteX(int spriteIndex);
float GetSpriteY(int spriteIndex);

// Scale must be > 0 (see the .c): a non-positive scale is rejected as a
// failure, so this also returns false when scale <= 0.
bool  SetSpriteScale(int spriteIndex, float scale);
float GetSpriteScale(int spriteIndex);

bool  SetSpriteRotation(int spriteIndex, float rotation);
float GetSpriteRotation(int spriteIndex);

bool SetSpriteSkew(int spriteIndex, float skewX, float skewY);
void GetSpriteSkew(int spriteIndex, float* skewX, float* skewY);

// Hotspot: the sprite's anchor point, given in unscaled sprite-local pixels
// with the top-left of the bitmap as origin (0,0). Two independent roles:
//   * Position anchor -- SetSpritePosition places the HOTSPOT at (x,y), not the
//     top-left. Default (0,0) keeps (x,y) meaning the top-left, so existing
//     positioning is unchanged until a hotspot is set.
//   * Rotation pivot -- only when SetSpriteRotateAroundHotspot is enabled (see
//     below); otherwise rotation still pivots about the sprite center.
// The hotspot MAY lie outside the sprite's own bounds (negative, or past
// SPRITE_WIDTH/HEIGHT): giving two sprites hotspots that resolve to the same
// world point keeps them moving in unison about a shared off-sprite anchor.
// Values are unscaled -- the engine multiplies by the sprite's scale. Absolute,
// like all sprite state; a relative nudge is the caller's to compute. Ungated.
bool SetSpriteHotspot(int spriteIndex, float hotspotX, float hotspotY);
void GetSpriteHotspot(int spriteIndex, float* hotspotX, float* hotspotY);

// Selects what SetSpriteRotation pivots about: false (default) the sprite
// center, true the hotspot. Independent of the hotspot's position-anchor role
// -- a sprite can anchor by its hotspot yet still spin about its center, or
// vice versa. Set/Get return success/the state; a bad index no-ops / reads false.
bool SetSpriteRotateAroundHotspot(int spriteIndex, bool enabled);
bool GetSpriteRotateAroundHotspot(int spriteIndex);

// Bitmask of SpriteFlip (SPRITE_FLIP_X / SPRITE_FLIP_Y, OR'd together for
// both axes). Mirrors the bitmap at render time with no atlas cost --
// applied consistently to both the render path and pixel collision.
bool          SetSpriteFlip(int spriteIndex, unsigned char flipMask);
unsigned char GetSpriteFlip(int spriteIndex);

bool          SetSpritePriority(int spriteIndex, unsigned char priority);
unsigned char GetSpritePriority(int spriteIndex);

bool          SetSpriteColorPalette(int spriteIndex, unsigned char palette);
unsigned char GetSpriteColorPalette(int spriteIndex);

bool          SetSpriteTransparency(int spriteIndex, unsigned char transparency);
unsigned char GetSpriteTransparency(int spriteIndex);

// Whether palette index 0 renders or stays transparent. Transparent is the
// default; showing zero is a deliberate design choice. The getter is here for
// completeness -- reading it is an edge case, but the state is the designer's
// to set explicitly.
bool SetSpriteShowZero(int spriteIndex, bool showzero);
bool GetSpriteShowZero(int spriteIndex);

bool           SetSpriteCollidableColors(int spriteIndex, unsigned short mask);
unsigned short GetSpriteCollidableColors(int spriteIndex);

bool          SetSpriteCollisionTypes(int spriteIndex, unsigned char typeMask);
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

// Atlas debug view -- SHELVED. Used to lay out every currently-uploaded
// bitmap slot in a grid via a reserved block of sprite indices at the
// top of MAX_SPRITES; that reservation cost every game 256 of MAX_SPRITES'
// 640 slots whether or not the view was ever toggled on, so the whole
// reserved range has been given back. Both functions are no-op stubs
// (ToggleSpriteAtlasView does nothing; GetSpriteAtlasViewActive always
// returns false) until this gets redesigned on top of something that
// doesn't compete with game sprites for the same pool -- likely a
// full-screen quad shared with the tile layer and other systems.
void ToggleSpriteAtlasView(void);
bool GetSpriteAtlasViewActive(void);

// ANUS parity: how many sprites actually drew last frame -- enabled,
// visible, and carrying a valid bitmap (the exact same filter
// gdmf_sprites_prepare uses to build its draw list), not just an
// enabled&&visible scan a caller could already do with GetSpriteEnabled/
// GetSpriteVisible. Reflects the most recently prepared frame; 0 before
// the first frame has been prepared.
int GetRenderedSpriteCount(void);

#endif // GDMF_SPRITES_H