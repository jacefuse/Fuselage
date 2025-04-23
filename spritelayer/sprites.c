#include "sprites.h"
#include "colors.h"

#ifdef __cplusplus
extern "C" {
#endif

Sprite sprites[MAX_SPRITES];
//RenderTexture2D spriteBuffer[MAX_SPRITES]; // No longer using one texture per sprite. Wasteful.
RenderTexture2D spriteBuffer;
RenderTexture2D spriteAtlas;
RenderTexture2D spriteLayer;
float atlasX, atlasY;                           // Coordinates of the sprite on the atlas
//unsigned char blanksprite[SPRITE_WIDTH * SPRITE_HEIGHT] = { 0x0 }; // Test Sprite Temporary
static unsigned char testsprite[SPRITE_WIDTH * SPRITE_HEIGHT] = { 0x0 }; // Test Sprite Temporary
bool useAtlas = true; // Now using the atlas
bool useLayers = false; // <-- not using layer for the moment.
short renderedsprites = 0;

int spritesPerRow;// = GetScreenWidth() / SPRITE_WIDTH; // Calculate sprites per row
int spritesPerCol;// = (MAX_SPRITES + spritesPerRow - 1) / spritesPerRow; 
int atlasWidth;// = spritesPerRow * SPRITE_WIDTH;       // Atlas width in pixels
int atlasHeight;// = (MAX_SPRITES / spritesPerRow + (MAX_SPRITES % spritesPerRow != 0)) * SPRITE_HEIGHT; // Total height of the atlas

Sprite* sprite1;
Sprite* sprite2;
static unsigned char buffer1[SPRITE_WIDTH * SPRITE_HEIGHT];
static unsigned char buffer2[SPRITE_WIDTH * SPRITE_HEIGHT];
static unsigned char rotated1[SPRITE_WIDTH * SPRITE_HEIGHT];
static unsigned char rotated2[SPRITE_WIDTH * SPRITE_HEIGHT];

// Initialize sprites
int InitSprites(void) {
    printf("Initializing all sprites...\n");
    int initcount = 0;

    // Compute rows&cols for the sprite index
    spritesPerRow = (int) sqrt(MAX_SPRITES);// GetScreenWidth() / SPRITE_WIDTH;
    spritesPerCol = (MAX_SPRITES + spritesPerRow - 1) / spritesPerRow;
    //if (MAX_SPRITES % spritesPerRow) spritesPerCol++;

    // Size up atlas
    atlasWidth = spritesPerRow * SPRITE_WIDTH;
    atlasHeight = SPRITE_HEIGHT * spritesPerCol;
    printf("ATLAS:\n%d rows of %d sprites\n%d columns of %d sprites.\n", atlasHeight / SPRITE_WIDTH, spritesPerRow, atlasWidth / SPRITE_HEIGHT, spritesPerCol);

    for (int i = 0; i < MAX_SPRITES; i++) {
        sprites[i].x = 0.0f;
        sprites[i].y = 0.0f;
        sprites[i].scale = 1.0f;                // Default scale to 1
        sprites[i].rotation = 0.0f;             // Default rotation to 0 degrees
        sprites[i].transparency = 255;          // Currently uses as alpha - will probably change to 2 bytes for 16 bitfields
        sprites[i].priority = 0;                // Eventually there will be a sorting routine for priorities of sprites/tiles.
        sprites[i].palette = 0;                 // There are 256 palettes of 16 colors. [pallet][0] currently always transparent.
        sprites[i].bitmap = NULL;               // Pointer to Bitmap used to updateSprite;
        sprites[i].enabled = false;
	    sprites[i].visible = false;
        sprites[i].showzero = false;
        sprites[i].collidableColors = 0xFFFE;   // Defaults to all colors collidable except background
    
        initcount++;
    }

    /*for (int y = 0; y < SPRITE_HEIGHT; ++y) {
        for (int x = 0; x < SPRITE_WIDTH; ++x) {
            blanksprite[y * SPRITE_WIDTH + x] = 0x0;
        }
    }*/

    //printf("Clearing testsprite[]\n.");
    // This code crates a checkerboard of 4 pixels by 4 pixels
    for (int y = 0; y < SPRITE_HEIGHT; ++y) {
        for (int x = 0; x < SPRITE_WIDTH; x += 2) {
            int gridX = x / 16;
            int gridY = y / 16;
            int colorIndex0 = gridY * 4 + gridX;
            int colorIndex1 = colorIndex0;

            unsigned char packedValue = (colorIndex0 << 4) | (colorIndex1 & 0x0F);
            testsprite[(y * SPRITE_WIDTH + x) / 2 ] = packedValue;
            //printf("%u.", packedValue);
        }
        //printf("\n.");
    }

    /*for (int i = 0; i < MAX_SPRITES; i++) {
        spriteBuffer[i] = LoadRenderTexture(SPRITE_WIDTH, SPRITE_HEIGHT);
        if (spriteBuffer[i].texture.id == 0) return -1;
    }*/

    spriteBuffer = LoadRenderTexture(SPRITE_WIDTH, SPRITE_HEIGHT);
    spriteLayer = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    if (spriteLayer.texture.id == 0) return -1;
    spriteAtlas = LoadRenderTexture(atlasWidth, atlasHeight);
    if (spriteAtlas.texture.id == 0) return -1;

    printf("Initialized %d sprites.\n", initcount);
    return initcount;
}

// Set current sprite data to temporary sprite buffer (working stage)
bool updateSprite(int spriteIndex) {
    if (spriteIndex < 0 || spriteIndex >= MAX_SPRITES) {
        printf("UPDATE: Sprite %d is out of range.\n", spriteIndex);
        return false;
    }
    if (sprites[spriteIndex].bitmap == NULL) {
        printf("UPDATE: Sprite %d is missing bitmap.\n", spriteIndex);
        sprites[spriteIndex].bitmap = testsprite;
        //return false;
    }
    if (!sprites[spriteIndex].enabled) {
        printf("UPDATE: Sprite %d is disabled.\n", spriteIndex);
    }

    BeginTextureMode(spriteBuffer);
    ClearBackground(BLANK);

    // Get a local copy of the color palette from the global set
    /*Color tempColors[16];
    for (int i = 0; i < 16; i++) {
        tempColors[i] = Colors[sprites[spriteIndex].palette][i];
    }*/

    //unsigned char transparency = sprites[spriteIndex].transparency;

    for (int y = 0; y < SPRITE_HEIGHT; y++) {
        for (int x = 0; x < SPRITE_WIDTH; x += 2) {
            // Calculate index into the bitmap
            int byteIndex = (y * SPRITE_WIDTH + x) / 2;
            unsigned char packedPixels = sprites[spriteIndex].bitmap[byteIndex];

            // Extract left pixel (high nibble) and right pixel (low nibble)
            unsigned char leftColorIndex = (packedPixels >> 4) & 0x0F;
            unsigned char rightColorIndex = packedPixels & 0x0F;
            //unsigned char transparency = sprites[spriteIndex].transparency;

            // Left pixel
            if (leftColorIndex != 0 || sprites[spriteIndex].showzero) {
                Color c = Colors[sprites[spriteIndex].palette][leftColorIndex];
                //c.a = transparency;
                DrawPixel(x, y, c);
            }
            //else {
            //    DrawPixel(x, y, (Color) { 255, 255, 255, 0 });
            //}

            // Right pixel
            if (rightColorIndex != 0 || sprites[spriteIndex].showzero) {
                Color c = Colors[sprites[spriteIndex].palette][rightColorIndex];
                //c.a = transparency;
                DrawPixel(x + 1, y, c);
            }
            //else {
            //    DrawPixel(x, y, (Color) { 255, 255, 255, 0 });
            //}


        }
    }

    EndTextureMode();

    return true;

}

// Updates the Atlas - Should be merged with UpdateSprite
bool updateAtlas(int spriteIndex) {
    // Validate sprite index
    if (spriteIndex < 0 || spriteIndex >= MAX_SPRITES) {
        printf("Invalid sprite index: %d\n", spriteIndex);
        return false;
    }

    // Ensure the sprite buffer exists
    if (spriteBuffer.texture.id == 0) {
        printf("Sprite buffer for index %d is not initialized.\n", spriteIndex);
        return false;
    }

    int col = spriteIndex % spritesPerRow;
    int row = spriteIndex / spritesPerRow;

    // Invert the row to start from the top of the atlas
    float posX = (float)col * SPRITE_WIDTH;
    float posY = (float)(spritesPerCol - 1 - row) * SPRITE_HEIGHT;

    // Update the specific part of the atlas
    BeginTextureMode(spriteAtlas);
    // Clear only the relevant area(optional, for clean replacement)
    BeginBlendMode(BLEND_CUSTOM);
    Rectangle clearArea = {
        posX,
        posY,
        (float)SPRITE_WIDTH,
        (float)SPRITE_HEIGHT,
    };
    DrawRectangleRec(clearArea, BLANK);
    EndBlendMode();

    if (sprites[spriteIndex].bitmap != NULL) {
        // Draw the updated sprite at the correct position
        DrawTextureEx(
            spriteBuffer.texture,
            (Vector2) {
            posX, posY,
        },
            0.0f,         // rotation
            1.0f,         // scale
            (Color) { 255, 255, 255, 255 } 
        );
    }

    EndTextureMode();

    //printf("Updated atlas entry for sprite index %d.\n", spriteIndex);
    return true;
}

bool CheckSpriteCollision(int spriteIndex1, int spriteIndex2) {
    // Get references to the sprites
    sprite1 = &sprites[spriteIndex1];
    sprite2 = &sprites[spriteIndex2];

    // Early exit: Skip collision checks for non-collidable sprites
    if (!sprite1->enabled || !sprite2->enabled ||
        sprite1->collidableColors == 0x0000 || sprite2->collidableColors == 0x0000) {
        return false;
    }

    // Compute bounding boxes
    float left1 = sprite1->x;
    float right1 = sprite1->x + SPRITE_WIDTH * sprite1->scale;
    float top1 = sprite1->y;
    float bottom1 = sprite1->y + SPRITE_HEIGHT * sprite1->scale;

    float left2 = sprite2->x;
    float right2 = sprite2->x + SPRITE_WIDTH * sprite2->scale;
    float top2 = sprite2->y;
    float bottom2 = sprite2->y + SPRITE_HEIGHT * sprite2->scale;

    // Check for bounding box overlap
    return !(left1 >= right2 || right1 <= left2 || top1 >= bottom2 || bottom1 <= top2);
}

bool CheckPixelCollision(int spriteIndex1, int spriteIndex2) {
    sprite1 = &sprites[spriteIndex1];
    sprite2 = &sprites[spriteIndex2];

    if (sprites[spriteIndex1].bitmap == NULL) {
        //printf("PIXCOL: INVALID COLLISION!\n");
        return false;
    }
    if (sprites[spriteIndex2].bitmap == NULL) {
        //printf("PIXCOL: INVALID COLLISION!\n");
        return false;
    }

    if (spriteIndex1 < 0 || spriteIndex1 >= MAX_SPRITES) {
        //printf("PIXCOL: Sprite %d is out of range.\n", spriteIndex1);
        return false;
    }
    if (!sprites[spriteIndex1].enabled || sprites[spriteIndex1].bitmap == NULL) {
        //printf("PIXCOL: Sprite %d is disabled or missing bitmap.\n", spriteIndex1);
        return false;
    }
    if (spriteIndex2 < 0 || spriteIndex2 >= MAX_SPRITES) {
        //printf("PIXCOL: Sprite %d is out of range.\n", spriteIndex2);
        return false;
    }
    if (!sprites[spriteIndex2].enabled || sprites[spriteIndex2].bitmap == NULL) {
        //printf("PIXCOL: Sprite %d is disabled or missing bitmap.\n", spriteIndex2);
        return false;
    }

    // Ensure bounding box collision succeeded
    if (!CheckSpriteCollision(spriteIndex1, spriteIndex2)) {
        return false;
    }
    //printf("COLLISION: %d <-> %d\n", spriteIndex1, spriteIndex2);
    // Compute bounding box overlap in screen space
    float sprite1Left = sprite1->x;
    float sprite1Top = sprite1->y;
    float sprite1Right = sprite1->x + SPRITE_WIDTH * sprite1->scale;
    float sprite1Bottom = sprite1->y + SPRITE_HEIGHT * sprite1->scale;

    float sprite2Left = sprite2->x;
    float sprite2Top = sprite2->y;
    float sprite2Right = sprite2->x + SPRITE_WIDTH * sprite2->scale;
    float sprite2Bottom = sprite2->y + SPRITE_HEIGHT * sprite2->scale;

    float overlapLeft = (float) fmax(sprite1Left, sprite2Left);
    float overlapTop = (float) fmax(sprite1Top, sprite2Top);
    float overlapRight = (float) fmin(sprite1Right, sprite2Right);
    float overlapBottom = (float) fmin(sprite1Bottom, sprite2Bottom);

    // Convert overlap bounds to pixel coordinates
    int overlapXStart = (int)overlapLeft;
    int overlapYStart = (int)overlapTop;
    int overlapXEnd = (int)overlapRight;
    int overlapYEnd = (int)overlapBottom;

    // Iterate through each overlapping pixel in screen space
    for (int screenY = overlapYStart; screenY < overlapYEnd; screenY++) {
        for (int screenX = overlapXStart; screenX < overlapXEnd; screenX++) {
            // Convert screen space coordinates to sprite-local coordinates
            int sprite1X = (int)((screenX - sprite1->x) / sprite1->scale);
            int sprite1Y = (int)((screenY - sprite1->y) / sprite1->scale);
            int sprite2X = (int)((screenX - sprite2->x) / sprite2->scale);
            int sprite2Y = (int)((screenY - sprite2->y) / sprite2->scale);

            // Ensure coordinates are within the sprite bounds
            if (sprite1X < 0 || sprite1X >= SPRITE_WIDTH || sprite1Y < 0 || sprite1Y >= SPRITE_HEIGHT) continue;
            if (sprite2X < 0 || sprite2X >= SPRITE_WIDTH || sprite2Y < 0 || sprite2Y >= SPRITE_HEIGHT) continue;

            // Get pixel data for both sprites
            int byteIndex1 = (sprite1Y * SPRITE_WIDTH + sprite1X) / 2;
            unsigned char packedPixel1 = sprite1->bitmap[byteIndex1];
            unsigned char leftColorIndex1 = (packedPixel1 >> 4) & 0x0F;
            unsigned char rightColorIndex1 = packedPixel1 & 0x0F;
            unsigned char pixelColorIndex1 = (sprite1X % 2 == 0) ? leftColorIndex1 : rightColorIndex1;

            int byteIndex2 = (sprite2Y * SPRITE_WIDTH + sprite2X) / 2;
            unsigned char packedPixel2 = sprite2->bitmap[byteIndex2];
            unsigned char leftColorIndex2 = (packedPixel2 >> 4) & 0x0F;
            unsigned char rightColorIndex2 = packedPixel2 & 0x0F;
            unsigned char pixelColorIndex2 = (sprite2X % 2 == 0) ? leftColorIndex2 : rightColorIndex2;

            // Check if both pixels are collidable
            if (((sprite1->collidableColors & (1 << pixelColorIndex1)) != 0) &&
                ((sprite2->collidableColors & (1 << pixelColorIndex2)) != 0)) {
                return true; // Collision detected
            }
        }
    }

    return false; // No collision
}

bool CheckRotatedPixelCollision(int spriteIndex1, int spriteIndex2) {
    memset(buffer1, 0, sizeof(buffer1));
    memset(buffer2, 0, sizeof(buffer2));
    memset(rotated1, 0, sizeof(rotated1));
    memset(rotated2, 0, sizeof(rotated2));

    sprite1 = &sprites[spriteIndex1];
    sprite2 = &sprites[spriteIndex2];

    if (!sprite1->enabled || !sprite2->enabled || !sprite1->bitmap || !sprite2->bitmap) {
        return false;
    }

    // Convert sprite bitmaps to binary collision buffers
    for (int y = 0; y < SPRITE_HEIGHT; y++) {
        for (int x = 0; x < SPRITE_WIDTH; x++) {
            int index = y * SPRITE_WIDTH + x;
            unsigned char color1 = (sprite1->bitmap[index / 2] >> ((x % 2) * 4)) & 0x0F;
            unsigned char color2 = (sprite2->bitmap[index / 2] >> ((x % 2) * 4)) & 0x0F;
            buffer1[index] = (sprite1->collidableColors & (1 << color1)) ? 1 : 0;
            buffer2[index] = (sprite2->collidableColors & (1 << color2)) ? 1 : 0;
        }
    }

    // Rotate buffers
    float angle1 = sprite1->rotation * (M_PI / 180.0);
    float angle2 = sprite2->rotation * (M_PI / 180.0);
    int cx = SPRITE_WIDTH / 2, cy = SPRITE_HEIGHT / 2;

    for (int y = 0; y < SPRITE_HEIGHT; y++) {
        for (int x = 0; x < SPRITE_WIDTH; x++) {
            int nx = (int)(cos(angle1) * (x - cx) - sin(angle1) * (y - cy) + cx);
            int ny = (int)(sin(angle1) * (x - cx) + cos(angle1) * (y - cy) + cy);
            if (nx >= 0 && nx < SPRITE_WIDTH && ny >= 0 && ny < SPRITE_HEIGHT) {
                rotated1[ny * SPRITE_WIDTH + nx] = buffer1[y * SPRITE_WIDTH + x];
            }

            nx = (int)(cos(angle2) * (x - cx) - sin(angle2) * (y - cy) + cx);
            ny = (int)(sin(angle2) * (x - cx) + cos(angle2) * (y - cy) + cy);
            if (nx >= 0 && nx < SPRITE_WIDTH && ny >= 0 && ny < SPRITE_HEIGHT) {
                rotated2[ny * SPRITE_WIDTH + nx] = buffer2[y * SPRITE_WIDTH + x];
            }
        }
    }

    // Determine overlapping region
    int minX = fmax(sprite1->x, sprite2->x);
    int minY = fmax(sprite1->y, sprite2->y);
    int maxX = fmin(sprite1->x + SPRITE_WIDTH, sprite2->x + SPRITE_WIDTH);
    int maxY = fmin(sprite1->y + SPRITE_HEIGHT, sprite2->y + SPRITE_HEIGHT);

    int collisionCount = 0;

    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            int localX1 = x - sprite1->x;
            int localY1 = y - sprite1->y;
            int localX2 = x - sprite2->x;
            int localY2 = y - sprite2->y;

            if (rotated1[localY1 * SPRITE_WIDTH + localX1] && rotated2[localY2 * SPRITE_WIDTH + localX2]) {
                collisionCount++;
            }
        }
    }

    return collisionCount > 0;
}

int DetectCollisions() {

    int collisions = 0; 
    for (int i = 0; i < MAX_SPRITES; i++) {
        for (int j = i + 1; j < MAX_SPRITES; j++) {
            if (CheckSpriteCollision(i, j)) {
                printf("Collision detected between Sprite %d and Sprite %d\n", i, j);
                collisions++;
                break;
            }
            
        }        
    }
    printf("%d collisions detected.\n", collisions);
    return collisions; 
}

void RenderSpritesToLayers(void) {

    BeginTextureMode(spriteLayer);
    ClearBackground(BLANK);    
    // Sprites will either be drawn here or in the Layer or direclty onto the display
    // if the layer is not used.
    drawSprites();

    EndTextureMode();

    // Return the total number of sprites that were drawn
    return;
}

// Update a sprite's position
bool UpdateSpritePosition(int spriteIndex, float X, float Y) {
	if (spriteIndex < 0 || spriteIndex >= MAX_SPRITES || !sprites[spriteIndex].enabled) return 0;

	// Adjust the sprite’s position by delta values
	sprites[spriteIndex].x = X;
	sprites[spriteIndex].y = Y;

    return 1;
}

unsigned short SetSpriteCollidableColors(int spriteIndex, unsigned short collisionMask) {
    // Set colors to detect for collision by mask
    // Example No collision 0x0000 or all detected 0xFFFF. Default 0xFFFE: Only color 0 not detected

    sprites[spriteIndex].collidableColors = collisionMask;

    return 0;
}

bool SetSpriteColorPalette(int spriteIndex, unsigned char palette) {
    // Retain the sprite's state
    bool deactivate = sprites[spriteIndex].enabled;
    bool updated = false;
    //printf("Assigning palette %d to sprite %d.\n", palette, spriteIndex);
    sprites[spriteIndex].enabled = true;
    sprites[spriteIndex].palette = palette;
    updated = updateSprite(spriteIndex);
    if (updated) updated = updateAtlas(spriteIndex);
    sprites[spriteIndex].enabled = deactivate;

    return updated;
};

unsigned char GetSpriteColorPalette(int spriteIndex) {
    unsigned char palette = sprites[spriteIndex].palette;

    return palette;
};

short GetRenderedSpriteCount() {
    return renderedsprites;
}

float GetSpriteX(int spriteIndex) {
    return sprites[spriteIndex].x;
}

float GetSpriteY(int spriteIndex) {
    return sprites[spriteIndex].y;
}

bool GetSpriteEnabled(int spriteIndex) {
    //if (sprites[spriteIndex].enabled == true) printf("SPRITE %d IS ENABLED\n", spriteIndex);
    //else printf("SPRITE %d IS DISABLED\n", spriteIndex);
    return sprites[spriteIndex].enabled;
}

bool ToggleSpriteEnabled(int spriteIndex) {
    sprites[spriteIndex].enabled = !sprites[spriteIndex].enabled;
    //printf("SET SPRITE ENABLE FLAG: %d = ", spriteIndex);
    //if (sprites[spriteIndex].enabled) printf("TRUE\n");
    //else printf("FALSE\n");
    return GetSpriteEnabled(spriteIndex);
}

bool SetSpriteEnabled(int spriteIndex, bool enabled) {
    sprites[spriteIndex].enabled = enabled;
    //printf("SET SPRITE ENABLE FLAG: %d = ", spriteIndex);
    //if (enabled) printf("TRUE\n");
    //else printf("FALSE\n");

    return sprites[spriteIndex].enabled;
}

bool GetSpriteVisible(int spriteIndex) {
    //if (sprites[spriteIndex].visible == true) printf("SPRITE INVISIBLE: %d\n", spriteIndex);
    //else printf("SPRITE INVISIBLE: %d\n", spriteIndex);
    return sprites[spriteIndex].visible;
}

bool ToggleSpriteVisible(int spriteIndex) {
    sprites[spriteIndex].visible = sprites[spriteIndex].visible;
    //printf("TOGGLE SPRITE VISIBLE: %d\n", spriteIndex);
    return GetSpriteVisible(spriteIndex);
}

bool SetSpriteVisible(int spriteIndex, bool enabled) {
    sprites[spriteIndex].visible = enabled;
    //printf("SET VISIBLE: %d\n", spriteIndex);
    return sprites[spriteIndex].visible;
}

void SetSpriteScale(int spriteIndex, float scale){
    sprites[spriteIndex].scale = scale;
    return;
}

float GetSpriteScale(int spriteIndex) {   
    return sprites[spriteIndex].scale;
}

float ChangeSpriteScale(int spriteIndex, float scale) {
    sprites[spriteIndex].scale = sprites[spriteIndex].scale + scale;
    return sprites[spriteIndex].scale;
}

void SetSpriteRotation(int spriteIndex, float rotation) {
    sprites[spriteIndex].rotation = rotation;
    return;
}

float GetSpriteRotation(int spriteIndex) {
    return sprites[spriteIndex].rotation;
}

float ChangeSpriteRotation(int spriteIndex, float rotation) {
    sprites[spriteIndex].rotation = sprites[spriteIndex].rotation+rotation;
    return sprites[spriteIndex].rotation;
}

// Should color register 0 be transparent or visible?
void SpriteShowZero(int spriteIndex, bool showzero) {
    sprites[spriteIndex].showzero = showzero;
    return;
}

void displaySpriteAtlas() {

        int screenwidth = GetScreenWidth();
        int screenheight = GetScreenHeight();
        int atlaswidth = spriteAtlas.texture.width;
        int atlasheight = spriteAtlas.texture.height;
        float scale = fminf((float)screenwidth / atlaswidth, (float)screenheight / atlasheight);

        Rectangle destRectangle = { (screenwidth - (atlaswidth * scale)) / 2.0f, (screenheight - (atlasheight * scale)) / 2.0f, atlaswidth * scale, atlasheight * scale };
        Rectangle sourceRectangle = { 0, 0, (float)atlaswidth, (float)atlasheight };

        DrawTexturePro(spriteAtlas.texture, sourceRectangle, destRectangle, (Vector2) { 0, 0 }, 0.0f, WHITE);

    return;
}

bool showSpriteFromAtlas(int spriteIndex, int x, int y) {
    // Safety check in case user passes an invalid index
    if (spriteIndex < 0 || spriteIndex >= MAX_SPRITES) {
        printf("Invalid spriteIndex %d\n", spriteIndex);
        return false;
    }

    //int renderorder = SPRITE_HEIGHT;

    //if (useLayer) renderorder = -renderorder;
    float rotation = sprites[spriteIndex].rotation;
    float scale = sprites[spriteIndex].scale;
    float scaledWidth = SPRITE_WIDTH * scale;
    float scaledHeight = SPRITE_HEIGHT * scale;
    Vector2 position = { (float)x, (float)y };
    Vector2 scaledPosition = {scaledWidth/2, scaledHeight/2};

    // Number of columns in the atlas (matching what you used in generateAtlas)
    //int spritesPerRow = GetScreenWidth() / SPRITE_WIDTH;
    //int spritesPerCol = (MAX_SPRITES % spritesPerRow) / spritesPerRow;
    //if (MAX_SPRITES % spritesPerRow) spritesPerCol++;

    // Calculate the row and column in the atlas
    int col = spriteIndex % spritesPerRow;
    int row = spriteIndex / spritesPerRow;


    // Construct the source rectangle from the atlas
    // The atlas is in spriteAtlas.texture
    Rectangle source = {
        (float)(col * SPRITE_WIDTH),  // x position within atlas
        (float)(row * SPRITE_HEIGHT), // y position within atlas
        (float)SPRITE_WIDTH,          // width
        (float)SPRITE_HEIGHT          // height
    };

    Rectangle destination = {
        position.x+(SPRITE_WIDTH/2),
        position.y+(SPRITE_HEIGHT/2),
        scaledWidth,
        scaledHeight
    };

    // Draw the requested sprite from the atlas to the position on screen
    DrawTexturePro(
        spriteAtlas.texture, 
        source, destination, 
        scaledPosition, 
        rotation, 
        (Color) { 255, 255, 255, sprites[spriteIndex].transparency});
    //printf("Sprite %d: printed at X:%d Y:%d from %d-%d\n", spriteIndex, x, y, col, row);
    return true;
}

void DisplaySprites(void) {
    
    if (useLayers) {
        RenderSpritesToLayers();
        DrawTexture(spriteLayer.texture, 0, 0, WHITE);
    }
    else drawSprites();

    return;
}

// Assign bitmap to sprite by Index
bool AssignSprite(int spriteIndex, unsigned char* bitmap) {
    //printf("ASSIGN SPRITE: %d\n", spriteIndex);
    bool success;
    if (spriteIndex < 0 || spriteIndex >= MAX_SPRITES) {
        printf("Sprite out of range.\n");
        return false;
    }
    sprites[spriteIndex].bitmap = bitmap;
    success = updateSprite(spriteIndex);
    if (success) success = updateAtlas(spriteIndex);

    return true;
}

bool AssignSpriteBitmapFromSprite(int spriteSource, int spriteDestination) {
    // Retain the sprite's state
    //bool deactivate = sprites[spriteDestination].enabled;
    bool success;

    sprites[spriteDestination].showzero = sprites[spriteSource].showzero;
    sprites[spriteDestination].bitmap = sprites[spriteSource].bitmap;

    //printf("Assigning %d to same sprite as %d.\n", spriteSource, spriteDestination);
    if (spriteSource < 0 || spriteSource >= MAX_SPRITES) {
        printf("Source sprite out of range.\n");
        return false;
    }
    if (spriteDestination < 0 || spriteDestination >= MAX_SPRITES) {
        printf("Destination sprite out of range.\n");
        return false;
    }

    //if (sprites[spriteSource].bitmap == NULL) return false;

    //sprites[spriteDestination].enabled = true;
    success = updateSprite(spriteDestination);
    if (success) success = updateAtlas(spriteDestination);
    //sprites[spriteDestination].enabled = deactivate;

    return true;
}

unsigned char* GetSpriteBitmap(int spriteIndex) {
    return sprites[spriteIndex].bitmap;
}

void SetSpriteTransparency(int spriteIndex, unsigned char transparency) {
    sprites[spriteIndex].transparency = transparency; 

    return;
}

unsigned char GetSpriteTransparency(int spriteIndex) {
    return sprites[spriteIndex].transparency;
}

// Clear Existing Bitmap
void ClearSprite(int spriteIndex) {

    printf("Clearing sprite: %d\n", spriteIndex);
    SetSpriteEnabled(spriteIndex, false);
    AssignSprite(spriteIndex, NULL);
    updateAtlas(spriteIndex);

    return;
};

// Insert Test Pattern
void SpriteTestPattern(int spriteIndex) {
    //SetSpriteEnabled(spriteIndex, true);
    //SpriteShowZero(spriteIndex, true);
    AssignSprite(spriteIndex, testsprite);
    updateAtlas(spriteIndex);
    //SetSpriteVisible(spriteIndex, true);
    return;
};

void toggleAtlas() {
    useAtlas = !useAtlas;
};

void toggleLayer() {
    useLayers = !useLayers;
}

short drawSprites() {
    renderedsprites = 0;

    // Iterate through all sprites
    for (int i = 0; i < MAX_SPRITES; i++) {
        if (sprites[i].enabled && sprites[i].visible) {

            // For example, use DrawTextureEx or DrawTexturePro with rotation,
            // scale, transparency, etc.
            // This snippet uses the built-in DrawTextureEx from raylib for brevity:

            float rotation = sprites[i].rotation;        // degrees
            float scale = sprites[i].scale;
            float posX = sprites[i].x;
            float posY = sprites[i].y;

            // For now transparency affects the whole sprite.
            Color tint = WHITE;
            tint.a = sprites[i].transparency;

            if (useAtlas) {
                if (showSpriteFromAtlas(i, (int)posX, (int)posY)) renderedsprites++; // **** SERIOUSLY BROKEN ****
            }
            else {
                DrawTextureEx(
                    spriteBuffer.texture,    // The sprite buffer texture
                    (Vector2) {
                    posX, posY
                },    // Position where the sprite is drawn
                    rotation,                   // Rotation in degrees
                    scale,                      // Scaling factor
                    tint                        // Tint color (including alpha)
                );
                renderedsprites++;
            }
        }
    }
/*/    else {
        // Iterate through all sprites in the reverse order.
        int i = MAX_SPRITES;
        for (i = MAX_SPRITES - 1; i > -1; i--) {
            if (sprites[i].enabled && sprites[i].visible) {

                // For example, use DrawTextureEx or DrawTexturePro with rotation,
                // scale, transparency, etc.
                // This snippet uses the built-in DrawTextureEx from raylib for brevity:

                float rotation = sprites[i].rotation;        // degrees
                float scale = sprites[i].scale;
                float posX = sprites[i].x;
                float posY = sprites[i].y;

                // For now transparency affects the whole sprite.
                Color tint = WHITE;
                tint.a = sprites[i].transparency;

                if (useAtlas) {
                    if (showSpriteFromAtlas(i, posX, posY)) renderedsprites++; // **** SERIOUSLY BROKEN ****
                }
                else {
                    DrawTextureEx(
                        spriteBuffer[i].texture,    // The sprite buffer texture
                        (Vector2) {
                        posX, posY
                    },    // Position where the sprite is drawn
                        rotation,                   // Rotation in degrees
                        scale,                      // Scaling factor
                        tint                        // Tint color (including alpha)
                    );
                    renderedsprites++;
                }
            }
        }
    }*/

    return renderedsprites;
}

void ShutdownSprites() {

    UnloadRenderTexture(spriteBuffer);
    UnloadRenderTexture(spriteLayer);
    UnloadRenderTexture(spriteAtlas);

    return;
}

#ifdef __cplusplus
}
#endif