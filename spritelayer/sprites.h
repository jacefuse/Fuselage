#include <stdio.h>
#include <stdbool.h>
#include <raylib.h>
#include <math.h>

//#include "textlayer.h"

#ifndef FUSELAGE_SPRITELAYER_H
#define FUSELAGE_SPRITELAYER_H
#define FUSELAGE_SPRITELAYER_VERSION "20250111"


#ifdef __cplusplus
extern "C" {
#endif

// Maximum number of sprites
#define MAX_SPRITES 640
#define SPRITE_WIDTH 64
#define SPRITE_HEIGHT 64

// Sprite structure
    typedef struct {
        float x;                                // x coordinate
        float y;                                // y coordinate
        float scale;                            // Scale factor
        float rotation;                         // Rotation angle in degrees
        // Currently uses as alpha for transparency - will probably change to 2 bytes for 16 bitfields
        // Will probably change to 2 bytes for 16 bits to determine transparency per color register.
        volatile unsigned char transparency;             // Transparancy (0-255, higher = more visible)
        unsigned char priority;                 // Render priority (0-255, lower = earlier)
        unsigned char palette;                   // Color palette to use
        unsigned char* bitmap;                           // Pointer to a raw bitmap (used to populate the atlas)
        bool enabled;                           // Is the sprite active?
        bool visible;                           // Should the sprite be rendered?
        bool showzero;                          // Should color 0 be shown?
        unsigned short collidableColors;        // Bitmask for collidable color registers (0-15)

    } Sprite;

    // Function prototypes
    int InitSprites(void);
    void ShutdownSprites(void);
    void RenderSpritesToLayers(void);
    short GetRenderedSpriteCount();
    unsigned char GetSpriteColorPalette(int spriteIndex);
    bool UpdateSpritePosition(int spriteIndex, float deltaX, float deltaY);
    float GetSpriteX(int spriteIndex);
    float GetSpriteY(int spriteIndex);
    void SetSpriteTransparency(int spriteIndex, unsigned char transparency);
    unsigned char GetSpriteTransparency(int spriteIndex);
    bool GetSpriteEnabled(int spriteIndex);
    bool ToggleSpriteEnabled(int spriteIndex);
    bool GetSpriteVisible(int spriteIndex);
    bool SetSpriteColorPalette(int spriteIndex, unsigned char palette);
    bool SetSpriteEnabled(int spriteIndex, bool enabled);
    bool ToggleSpriteVisible(int spriteIndex);
    bool SetSpriteVisible(int spriteIndex, bool visible);
    void SetSpriteScale(int spriteIndex, float scale);
    float GetSpriteScale(int SpriteIndex);
    float ChangeSpriteScale(int SpriteIndex, float scale);
    void SetSpriteRotation(int spriteIndex, float rotation);
    float GetSpriteRotation(int SpriteIndex);
    float ChangeSpriteRotation(int SpriteIndex, float rotation);
    unsigned char* GetSpriteBitmap(int spriteIndex);
    bool AssignSprite(int spriteIndex, unsigned char* bitmap);
    bool AssignSpriteBitmapFromSprite(int spriteSource, int priteDestination);
    bool CheckSpriteCollision(int spriteIndex1, int spriteIndex2);
    bool CheckPixelCollision(int spriteIndex1, int spriteIndex2);
    int DetectCollisions();
    void DisplaySprites(void);
    void ClearSprite(int spriteIndex);
    void SpriteTestPattern(int spriteIndex);
    void SpriteShowZero(int spriteIndex, bool showzero);

    void displaySpriteAtlas();
    bool updateSprite(int spriteIndex);
    short drawSprites(void);   
    bool showSpriteFromAtlas(int spriteIndex, int x, int y);
    bool updateAtlas(int spriteIndex);
    
    // Probably going away
    void toggleAtlas(void);
    void toggleLayer(void);

#ifdef __cplusplus
}
#endif

#endif // FUSELAGE_SPRITELAYER_H
