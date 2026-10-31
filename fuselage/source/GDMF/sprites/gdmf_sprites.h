#ifndef GDMF_SPRITES_H
#define GDMF_SPRITES_H

// Indexed bitmaps shared by independently placed sprites. SEE: (GDMF_SPRITES.md, Bitmaps)
#include <stdbool.h>
#include <stdint.h>

#define GDMF_SPRITES_VERSION "0.5.2026102701 ENTERON"
#define SPRITE_WIDTH 64
#define SPRITE_HEIGHT 64
#define MAX_SPRITE_BITMAPS 1024
#define MAX_SPRITES 4096
#define SPRITE_BITMAP_NONE -1
#define SPRITE_TEST_PATTERN_BITMAP_ID 0
#define MAX_COLLISIONS_PER_SPRITE 32

typedef int16_t SpriteBitmapID;

typedef enum {
    COLLISION_TYPE_NONE = 0,
    COLLISION_TYPE_BOUNDING_BOX = 1 << 0,
    COLLISION_TYPE_ANY_COLOR = 1 << 1,
    COLLISION_TYPE_COLLIDABLE_COLORS = 1 << 2,
} SpriteCollisionType;

typedef enum {
    SPRITE_FLIP_NONE = 0,
    SPRITE_FLIP_X = 1 << 0,
    SPRITE_FLIP_Y = 1 << 1,
} SpriteFlip;

// One entry per other sprite and matched type, not a combined type mask.
typedef struct {
    int otherSprite;
    SpriteCollisionType type;
} SpriteCollisionInfo;

// State description; live instances are private. Use the functions below.
// SEE: (GDMF_SPRITES.md, Attributes)
typedef struct {
    float x;
    float y;
    float scale;
    float scaleX;
    float scaleY;
    float rotation;
    float skewX;
    float skewY;
    float hotspotX;
    float hotspotY;
    bool rotateAroundHotspot;
    unsigned char transparency;
    unsigned char priority;
    unsigned char palette;
    SpriteBitmapID bitmapID;
    bool enabled;
    bool visible;
    bool showzero;
    unsigned short collidableColors;
    unsigned char collisionTypes;
    unsigned char flip;
} Sprite;

// Initializes host slots and attempts GPU setup. Returns the slot count, not GPU readiness.
// SEE: (GDMF_SPRITES.md, Lifecycle)
int InitSprites(void);
void ShutdownSprites(void);

// Copies 2048 packed bytes, high nibble first. True means accepted for upload.
// False for an out-of-range bitmap ID or NULL data; no length argument is provided.
bool UploadSpriteBitmap(SpriteBitmapID bitmap_id, const unsigned char* bitmap);
// An unuploaded slot may be assigned; SPRITE_BITMAP_NONE unassigns it.
bool AssignSprite(int sprite_index, SpriteBitmapID bitmap_id);
// Copies the bitmap reference AND showzero, leaving all other attributes unchanged.
bool AssignSpriteBitmapFromSprite(int source_sprite, int destination_sprite);
// SPRITE_BITMAP_NONE on an invalid index.
SpriteBitmapID GetSpriteBitmapID(int sprite_index);
// Disables and unassigns; preserves other attributes and shared bitmap contents.
bool ClearSprite(int sprite_index);
// Assigns slot zero only; does not enable or show the sprite.
bool AssignSpriteTestPattern(int sprite_index);

// Bool setters/toggles return success, not the new state; invalid indices return false.
// Invalid bool getters return false. SEE: (GDMF_SPRITES.md, Return values)
bool GetSpriteEnabled(int sprite_index);
bool SetSpriteEnabled(int sprite_index, bool enabled);
bool ToggleSpriteEnabled(int sprite_index);
bool GetSpriteVisible(int sprite_index);
bool SetSpriteVisible(int sprite_index, bool visible);
bool ToggleSpriteVisible(int sprite_index);

// Canvas coordinates place the hotspot. Attributes can be set while disabled.
bool SetSpritePosition(int sprite_index, float x, float y);
float GetSpriteX(int sprite_index);
float GetSpriteY(int sprite_index);
// Rejects values <= 0; getters return 0 for uniform scale, 1 for axis scale on invalid IDs.
// SEE: (GDMF_SPRITES.md, Transforms)
bool SetSpriteScale(int sprite_index, float scale);
float GetSpriteScale(int sprite_index);
bool SetSpriteScaleX(int sprite_index, float scale_x);
bool SetSpriteScaleY(int sprite_index, float scale_y);
float GetSpriteScaleX(int sprite_index);
float GetSpriteScaleY(int sprite_index);
// Rotation is in degrees. Other float getters below return 0 on invalid indices.
bool SetSpriteRotation(int sprite_index, float rotation_degrees);
float GetSpriteRotation(int sprite_index);
bool SetSpriteSkewX(int sprite_index, float skew_x);
bool SetSpriteSkewY(int sprite_index, float skew_y);
float GetSpriteSkewX(int sprite_index);
float GetSpriteSkewY(int sprite_index);
// Unscaled local coordinates; a hotspot may lie outside the bitmap.
bool SetSpriteHotspot(int sprite_index, float hotspot_x, float hotspot_y);
bool SetSpriteHotspotX(int sprite_index, float hotspot_x);
bool SetSpriteHotspotY(int sprite_index, float hotspot_y);
float GetSpriteHotspotX(int sprite_index);
float GetSpriteHotspotY(int sprite_index);
// False selects the center as rotation pivot; true selects the hotspot.
bool SetSpriteRotateAroundHotspot(int sprite_index, bool enabled);
bool GetSpriteRotateAroundHotspot(int sprite_index);
bool SetSpriteFlipX(int sprite_index, bool mirrored);
bool SetSpriteFlipY(int sprite_index, bool mirrored);
bool GetSpriteFlipX(int sprite_index);
bool GetSpriteFlipY(int sprite_index);

// Lower priority draws later. Palette selects one of the 256 Colors palettes.
bool SetSpritePriority(int sprite_index, unsigned char priority);
unsigned char GetSpritePriority(int sprite_index);
bool SetSpriteColorPalette(int sprite_index, unsigned char palette);
unsigned char GetSpriteColorPalette(int sprite_index);
// Despite the name, this is opacity: 0 invisible, 255 fully opaque.
bool SetSpriteTransparency(int sprite_index, unsigned char opacity);
unsigned char GetSpriteTransparency(int sprite_index);
bool SetSpriteShowZero(int sprite_index, bool show_zero);
bool GetSpriteShowZero(int sprite_index);
// One bit per palette index for COLLIDABLE_COLORS tests.
bool SetSpriteCollidableColors(int sprite_index, unsigned short color_mask);
unsigned short GetSpriteCollidableColors(int sprite_index);
bool SetSpriteCollisionTypes(int sprite_index, unsigned char type_mask);
unsigned char GetSpriteCollisionTypes(int sprite_index);

// Last automatic preparation pass; up to 32 entries per sprite, excess silently dropped.
// SEE: (GDMF_SPRITES.md, Collision results)
bool HasSpriteCollision(int sprite_index);
int GetSpriteCollisionCount(int sprite_index);
// Borrowed array; optional count receives 0 on invalid IDs, which return NULL.
const SpriteCollisionInfo* GetSpriteCollisions(int sprite_index, int* output_count);

// Ignores enabled/visible and reporting masks. Returns matched requested bits,
// or 0 for no match, invalid indices or a self-pair. Does not update automatic results.
// Numeric limits can suppress pixel tests. SEE: (GDMF_SPRITES.md, Collision results)
unsigned char CheckSpritePairCollision(int first_sprite, int second_sprite, unsigned char types_to_test);
typedef struct {
    short a, b;
} SpritePairRef;
// Writes count bytes; returns matched-pair count. NULL arrays/count <= 0 return 0 without writes.
// Sprite state must remain unchanged until return. SEE: (GDMF_SPRITES.md, Batch queries)
int CheckSpritePairCollisionBatch(const SpritePairRef* pairs, int count,
                                 unsigned char types_to_test, unsigned char* results);
// Returns total matches, possibly beyond capacity; -1 on scratch allocation failure, no outputs written.
// NULL inputs/count <= 0 return 0. Output arrays are optional; capacity <= 0 counts only.
int FindSpriteCollisionPairs(const short* sprite_indices, int count, unsigned char types_to_test,
                            SpritePairRef* output_pairs, int capacity, unsigned char* output_types);

// Tests transformed local pixel bounds with truncation toward zero, not bitmap coverage.
// Ignores enabled/visible. SEE: (GDMF_SPRITES.md, Point sampling)
bool IsSpriteWorldPointInside(int sprite_index, float world_x, float world_y);
// False on invalid index, leaving outputs alone. Each output pointer is optional.
bool GetSpriteBoundingBox(int sprite_index, float* minimum_x, float* minimum_y,
                          float* maximum_x, float* maximum_y);
// Palette index 0..15, or -1 for invalid ID, missing bitmap, or outside the bitmap.
int GetSpritePixelAtWorldPoint(int sprite_index, float world_x, float world_y);

// Unimplemented compatibility interface: toggle does nothing; getter returns false.
void ToggleSpriteAtlasView(void);
bool GetSpriteAtlasViewActive(void);
// Most recently prepared draw-list count, not confirmed presentation. Initially 0.
int GetSpriteRenderedCount(void);

#endif // GDMF_SPRITES_H
