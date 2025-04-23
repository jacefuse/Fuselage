/* Fuselage tile layer Implementations */

#include "tiles.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// Most of this will probably be rewritten at some point.

bool layerActive[MAX_LAYERS] = { false, false, false, false }; // Might rework this;
RenderTexture2D tileBuffer;
RenderTexture2D layerBuffer;
RenderTexture2D tileAtlas[MAX_LAYERS];
//RenderTexture2D tileLayer;
Map tilemap[MAX_LAYERS];
Tile tiles[MAX_LAYERS][MAX_TILES];

static unsigned char testtile[MAX_TILE_WIDTH * MAX_TILE_HEIGHT] = { 0x0 };

short renderedtiles = 0;

int tilesPerRow;// = GetScreenWidth() / TILE_WIDTH; // Calculate tiles per row
int tilesPerCol;// = (MAX_TILES + tilesPerRow - 1) / tilesPerRow; 
int atlasWidth;// = tilesPerRow * TILE_WIDTH;     // Atlas width in pixels
int atlasHeight;// = (MAX_TILES / tilesPerRow + (MAX_TILES % tilesPerRow != 0)) * TILE_HEIGHT; // Total height of the atlas

bool InitTileMap(unsigned char layer, unsigned short mapWidth, unsigned short mapHeight,
    unsigned short tileWidth, unsigned short tileHeight, unsigned short tileCount, float scale) {
    printf("Initializing all tile locations...\n");

    if (mapWidth > MAX_MAP_WIDTH || mapHeight > MAX_MAP_HEIGHT) {
        printf("MAP INITIALIZE FAILURE: Illegal Map Size!\n");
        return false;
    }

    // Initialize map offsets ONCE
    tilemap[layer].mapOffsetX = 0.0;
    tilemap[layer].mapOffsetY = 0.0;

    tilemap[layer].width = mapWidth;
    tilemap[layer].height = mapHeight;
    tilemap[layer].tileWidth = tileWidth;
    tilemap[layer].tileHeight = tileHeight;

    if (scale != 1.0f) tilemap[layer].scale = scale;
    else tilemap[layer].scale = 1.0f;  // Default to no scaling

    // Defaults to CLAMPING: Map Wrap is disabled by default.
    tilemap[layer].wrapX = false;
    tilemap[layer].wrapY = false;

    // Set viewport to default to the full map size (can be changed later)
    tilemap[layer].viewportX = 0;
    tilemap[layer].viewportY = 0;
    tilemap[layer].viewportWidth = (unsigned short)(2+(GetScreenWidth()/tileWidth)/scale);
    tilemap[layer].viewportHeight = (unsigned short)(2+(GetScreenHeight()/tileHeight)/scale);

    int initcount = 0;

    for (int x = 0; x < mapWidth; x++) {
        for (int y = 0; y < mapHeight; y++) {
            tilemap[layer].location[x][y].tileTypeID = 0; // ID of tile type at Location
            tilemap[layer].location[x][y].metadata = NULL; // Pointer to optional metadata
            tilemap[layer].location[x][y].collidableColors = 0xFFFF; // Colors to detect for collision
            tilemap[layer].location[x][y].flags = 0x00; // flags: 0 collision on/off : 1 HFLIP : 2 VFLIP : 3-7 reserved

            // Initialize transparency levels
            for (int t = 0; t < 16; t++) {
                tilemap[layer].location[x][y].transparency[t] = 255;
            }

            initcount++;
        }
    }

    printf("Initialized %d tile locations on map.\n", initcount);

    // Initialize tile system
    InitTiles(layer, tileWidth, tileHeight, tileCount);

    return true;
}

// Initiate atlas for tiles and creates blank grid test pattern for testing tiles
bool InitTiles(unsigned char layer, unsigned short tileWidth, unsigned short tileHeight, unsigned short tileCount){

    // Compute rows&cols for the tile index
    tilesPerRow = (int)sqrt(tileCount);
    //tilesPerRow = GetScreenWidth() / MAX_TILE_WIDTH;
    tilesPerCol = (tileCount + tilesPerRow - 1) / tilesPerRow;

    if (tileCount > MAX_TILES) {
        printf("TILE INITIALILZE FAILURE: Tile limit of %d exceeded by %d.", MAX_TILES, tileCount-MAX_TILES);
        return false;
    }
    if (tileWidth > MAX_TILE_WIDTH || tileHeight > MAX_TILE_HEIGHT) {
        printf("TILE INITIALILZE FAILURE: Illegal Tile Size! W:%d H:%d\n", tileWidth, tileHeight);
        return false;
    }

    // Size up atlas
    atlasWidth = tilesPerRow * tileWidth;
    atlasHeight = tileHeight * tilesPerCol;
    printf("TILE ATLAS:\n%d rows of %d tiles\n%d columns of %d tiles.\n", atlasHeight / tileWidth, tilesPerRow, atlasWidth / tileHeight, tilesPerCol);

    // This code crates a checkerboard of 4 pixels by 4 pixels
    for (int y = 0; y < tileHeight; y++) {
        for (int x = 0; x < tileWidth; x++) {
            // Figure out the color index.
            int gridX = x / 8;
            int gridY = y / 8;
            int colorIndex = (gridY * 4 + gridX) & 0x0F;

            // Compute which byte in 'testtile' we store into:
            int byteIndex = y * (tileWidth / 2) + (x / 2);  // each row has 16 bytes
            unsigned char oldByte = testtile[byteIndex];

            // Store colorIndex in the high nibble if x is even,
            // or in the low nibble if x is odd.
            if ((x & 1) == 0) {
                // Even x => put color in the high nibble
                testtile[byteIndex] = (oldByte & 0x0F) | (colorIndex << 4);
            }
            else {
                // Odd x => put color in the low nibble
                testtile[byteIndex] = (oldByte & 0xF0) | (colorIndex & 0x0F);
            }
        }
    }

    tileBuffer = LoadRenderTexture(tileWidth, tileHeight);
    layerBuffer = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    tileAtlas[layer] = LoadRenderTexture(atlasWidth, atlasHeight);
    if (tileAtlas[layer].texture.id == 0) return -1;

    return true;
}

// Set current sprite data to temporary sprite buffer (working stage)
bool updateTile(unsigned char layer, int tileID) {
    if (tileID < 0 || tileID >= MAX_TILES) {
        printf("UPDATE: Tile %d is out of range.\n", tileID);
        return false;
    }
    if (tiles[layer][tileID].bitmap == NULL) {
        printf("UPDATE: tile %d is missing bitmap.\n", tileID);
        tiles[layer][tileID].bitmap = testtile;
        //return false;
    }

    BeginTextureMode(tileBuffer);
    ClearBackground(BLANK);

    //printf("Palette: %d\n", tiles[tileID].palette);

    for (int y = 0; y < MAX_TILE_HEIGHT; y++) {
        for (int x = 0; x < MAX_TILE_WIDTH; x += 2) {
            // Calculate index into the bitmap
            int byteIndex = (y * MAX_TILE_WIDTH + x) / 2;
            unsigned char packedPixels = tiles[layer][tileID].bitmap[byteIndex];
            //printf("%d", packedPixels);
            // Extract left pixel (high nibble) and right pixel (low nibble)
            unsigned char leftColorIndex = (packedPixels >> 4) & 0x0F;
            unsigned char rightColorIndex = packedPixels & 0x0F;
            //unsigned char transparency = tiles[tileID].transparency[0];
            //printf("I%d:%u(%u.%u)", byteIndex, tiles[tileID].bitmap[byteIndex], leftColorIndex, rightColorIndex);
            // Left pixel
            if (leftColorIndex != 0) {
                Color cl = Colors[tiles[layer][tileID].palette][leftColorIndex];
                //cl.a = transparency;
                DrawPixel(x, y, cl);
                //printf("%d:%d.%d.%d:", leftColorIndex, cl.r, cl.g, cl.b);
            }

            // Right pixel
            if (rightColorIndex != 0) {
                Color cr = Colors[tiles[layer][tileID].palette][rightColorIndex];
                //cr.a = transparency;
                DrawPixel(x+1, y, cr);
                //printf("%d:%d.%d.%d:", rightColorIndex, cl.r, cl.g, cl.b);
            }
        }
        //printf("\n");
    }

    EndTextureMode();

    //printf("Tile %d updated.\n", tileID);

    return true;

}

// Updates the Atlas - Should be merged with UpdateSprite
bool updateTileAtlas(unsigned char layer, int tileID) {
    // Validate sprite index
    if (tileID < 0 || tileID >= MAX_TILES) {
        printf("Invalid sprite index: %d\n", tileID);
        return false;
    }

    // Ensure the sprite buffer exists
    if (tileBuffer.texture.id == 0) {
        printf("Tile buffer for index %d is not initialized.\n", tileID);
        return false;
    }

    int col = tileID % tilesPerRow;
    int row = tileID / tilesPerRow;

    // Invert the row to start from the top of the atlas
    float posX = (float)col * MAX_TILE_WIDTH;
    float posY = (float)(tilesPerCol - 1 - row) * MAX_TILE_HEIGHT;

    // Update the specific part of the atlas
    BeginTextureMode(tileAtlas[layer]);
    // Clear only the relevant area(optional, for clean replacement)
    BeginBlendMode(BLEND_CUSTOM);
    Rectangle clearArea = {
        posX,
        posY,
        (float)MAX_TILE_WIDTH,
        (float)MAX_TILE_HEIGHT,
    };
    DrawRectangleRec(clearArea, BLANK);
    EndBlendMode();

    if (tiles[layer][tileID].bitmap != NULL) {
        // Draw the updated sprite at the correct position
        DrawTextureEx(
            tileBuffer.texture,
            (Vector2) {
            posX, posY,
        },
            0.0f,         // rotation
            1.0f,         // scale
            WHITE
        );
    }

    EndTextureMode();

    //printf("Updated atlas entry for tileID %d.\n", tileID);
    return true;
}

// Assign a bitmap to a tileID
bool AssignTile(unsigned char layer, int tileID, unsigned char* bitmap) {

    printf("ASSIGN TILE: %d\n", tileID);
    bool success;
    
    if (tileID < 0 || tileID >= MAX_TILES) {
        printf("Tile out of range.\n");
        return false;
    }

    if (bitmap == NULL) {
        printf("ASSIGN TILE: %d has no bitmap; assigning test pattern.\n", tileID);
        tiles[layer][tileID].bitmap = testtile;
    }
    else {
        tiles[layer][tileID].bitmap = bitmap;
    }

    tiles[layer][tileID].bitmap = bitmap;
    success = updateTile(layer, tileID);
    if (success) success = updateTileAtlas(layer, tileID);

    return success;
}

void SetTileViewport(unsigned char layer, unsigned short x, unsigned short y, unsigned short width, unsigned short height) {
    if (x + width > tilemap[layer].width || y + height > tilemap[layer].height) {
        printf("Error: Viewport exceeds tilemap boundaries.\n");
        return;
    }
    tilemap[layer].viewportX = x;
    tilemap[layer].viewportY = y;
    tilemap[layer].viewportWidth = width;
    tilemap[layer].viewportHeight = height;

    return;
}

// Set a palette for a tile
bool SetTilePalette(unsigned char layer, int tileID, unsigned char palette) {
    
    tiles[layer][tileID].palette = palette;
    printf("Tile %d palette set to %d.\n", tileID, palette);

    return true;
}

// Set scale factor for layer
void SetTileLayerScale(unsigned char layer, float scale) {
    if (layer >= 4) {
        printf("ERROR: Invalid layer %d\n", layer);
        return;
    }

    if (scale < 0.1f) {
        printf("ERROR: Scale must be at least 0.1x\n");
        return;
    }

    tilemap[layer].scale = scale;
    printf("Layer %d scale set to %.2f\n", layer, scale);

    return;
}

// Set map wrap -- DEFAULTS TO OFF
void SetTileMapWrapping(unsigned char layer, bool wrapX, bool wrapY) {
    tilemap[layer].wrapX = wrapX;
    tilemap[layer].wrapY = wrapY;
    printf("Layer %d wrapping set: X=%s, Y=%s\n", layer, wrapX ? "ON" : "OFF", wrapY ? "ON" : "OFF");
}

// Set H/V flip
void SetTileFlip(unsigned char layer, unsigned short x, unsigned short y, bool hflip, bool vflip) {
    // Clear HFLIP and VFLIP bits
    tilemap[layer].location[x][y].flags &= ~(HFLIP_FLAG | VFLIP_FLAG);

    // Set HFLIP if true
    if (hflip) {
        tilemap[layer].location[x][y].flags |= HFLIP_FLAG;
    }

    // Set VFLIP if true
    if (vflip) {
        tilemap[layer].location[x][y].flags |= VFLIP_FLAG;
    }

    return;
}

// Display atlas for testing purposes
void displayTileAtlas(unsigned char layer) {

    int screenwidth = GetScreenWidth();
    int screenheight = GetScreenHeight();
    int atlaswidth = tileAtlas[layer].texture.width;
    int atlasheight = tileAtlas[layer].texture.height;
    //float scale = fminf((float)screenwidth / atlaswidth, (float)screenheight / atlasheight);
    float scale = fminf((float)screenwidth / (float)atlaswidth, (float)screenheight / (float)atlasheight);
    //temp
    //static float scroll = 0.0f;

    Rectangle destRectangle = { (screenwidth - (atlaswidth * scale)) / 2.0f, (screenheight - (atlasheight * scale)) / 2.0f, atlaswidth * scale, atlasheight * scale };
    Rectangle sourceRectangle = { 0, 0, (float)atlaswidth, (float)atlasheight };

    //DrawTexturePro(tileAtlas.texture, sourceRectangle, destRectangle, (Vector2) { 0, 0 }, 0.0f, WHITE);
    //DrawTexture(tileAtlas.texture, 0, 0, WHITE);
    DrawTextureEx(tileAtlas[layer].texture, (Vector2) { 0.0f, 0.0f }, 0, 4.0f, WHITE);
    //DrawTexture(tileBuffer.texture, 0, 0, WHITE);
    //scroll += 0.25;
    //if (scroll == 80.0f) scroll = 0;
    //printf("Displaying Tile Atlas.\n");

    return;
}

// Insert Test Pattern
void TileTestPattern(unsigned char layer, int tileIndex) {

    AssignTile(layer, tileIndex, testtile);
    updateTileAtlas(layer, tileIndex);

    return;
};

// Scrool the tile layer
void ScrollTileMap(unsigned char layer, double deltaX, double deltaY) {
    if (layer >= 4) {
        printf("ERROR: Invalid layer %d\n", layer);
        return;
    }

    float scale = tilemap[layer].scale;
    unsigned short vpw = tilemap[layer].viewportWidth;
    unsigned short vph = tilemap[layer].viewportHeight;

    double tileW = tilemap[layer].tileWidth * scale;
    double tileH = tilemap[layer].tileHeight * scale;
    double mapPixelWidth = tilemap[layer].width * tileW;
    double mapPixelHeight = tilemap[layer].height * tileH;

    // Apply movement
    tilemap[layer].mapOffsetX += deltaX * scale;
    tilemap[layer].mapOffsetY += deltaY * scale;

    // Handle seamless wrapping behavior
    if (tilemap[layer].wrapX) {
        if (tilemap[layer].mapOffsetX < mapPixelWidth) {
            tilemap[layer].mapOffsetX += mapPixelWidth;
        }
        if (tilemap[layer].mapOffsetX >= mapPixelWidth) {
            tilemap[layer].mapOffsetX -= mapPixelWidth;
        }
    }
    else {
        if (tilemap[layer].mapOffsetX < 0) tilemap[layer].mapOffsetX = 0;
        if (tilemap[layer].mapOffsetX > mapPixelWidth - vpw * tileW) {
            tilemap[layer].mapOffsetX = mapPixelWidth - vpw * tileW;
        }
    }

    if (tilemap[layer].wrapY) {
        if (tilemap[layer].mapOffsetY < mapPixelHeight) {
            tilemap[layer].mapOffsetY += mapPixelHeight;
        }
        if (tilemap[layer].mapOffsetY >= mapPixelHeight) {
            tilemap[layer].mapOffsetY -= mapPixelHeight;
        }
    }
    else {
        if (tilemap[layer].mapOffsetY < 0) tilemap[layer].mapOffsetY = 0;
        if (tilemap[layer].mapOffsetY > mapPixelHeight - vph * tileH) {
            tilemap[layer].mapOffsetY = mapPixelHeight - vph * tileH;
        }
    }
}

// Place a tile at location within map
void PlaceTileInMap(unsigned char layer, unsigned short x, unsigned short y, unsigned short tileID) {
    if (layer >= 4) {
        printf("ERROR: Invalid layer %d\n", layer);
        return;
    }

    if (x >= tilemap[layer].width || y >= tilemap[layer].height) {
        printf("ERROR: Tile placement out of bounds at (%d, %d) on layer %d\n", x, y, layer);
        return;
    }

    if (tileID >= MAX_TILES) {
        printf("ERROR: Invalid tile ID %d\n", tileID);
        return;
    }

    tilemap[layer].location[x][y].tileTypeID = tileID;
    printf("Placed tile %d at (%d, %d) on layer %d\n", tileID, x, y, layer);

    return;
}

// Display the entire layer
void DrawTileLayer(unsigned char layer) {
    if (layer >= 4) {
        printf("ERROR: Invalid layer %d\n", layer);
        return;
    }

    unsigned short tileWidth = tilemap[layer].tileWidth;
    unsigned short tileHeight = tilemap[layer].tileHeight;
    float scale = tilemap[layer].scale;

    int mapWidth = tilemap[layer].width;
    int mapHeight = tilemap[layer].height;

    // Get viewport properties
    int viewportX = tilemap[layer].viewportX;  // Viewport position on screen
    int viewportY = tilemap[layer].viewportY;
    int viewportW = tilemap[layer].viewportWidth;  // Width in tiles
    int viewportH = tilemap[layer].viewportHeight;

    int tilesAcross = viewportW;
    int tilesDown = viewportH;
    //int tilesAcross = (int)ceil(viewportW / scale) + 2;  // Extra for overdraw
    //int tilesDown = (int)ceil(viewportH / scale) + 2;    // Extra for overdraw

    // Compute precise tile offsets
    float offsetX = -(float)fmod(tilemap[layer].mapOffsetX, tileWidth * scale);
    float offsetY = -(float)fmod(tilemap[layer].mapOffsetY, tileHeight * scale);

    BeginTextureMode(layerBuffer);
    ClearBackground(BLANK);
    for (int y = 0; y < tilesDown; y++) {
        for (int x = 0; x < tilesAcross; x++) {
            int tileX = ((int)(tilemap[layer].mapOffsetX / (tileWidth * scale)) + x) % mapWidth;
            int tileY = ((int)(tilemap[layer].mapOffsetY / (tileHeight * scale)) + y) % mapHeight;

            // Handle wrapping
            if (tileX < 0) tileX += mapWidth;
            if (tileY < 0) tileY += mapHeight;

            if (!tilemap[layer].wrapX && (tilemap[layer].mapOffsetX + x * tileWidth * scale >= mapWidth * tileWidth * scale)) continue;
            if (!tilemap[layer].wrapY && (tilemap[layer].mapOffsetY + y * tileHeight * scale >= mapHeight * tileHeight * scale)) continue;

            int tileID = tilemap[layer].location[tileX][tileY].tileTypeID;
            if (tileID >= 0 && tileID < MAX_TILES) {
                // Calculate tile position in atlas
                Rectangle sourceRect = {
                    (float)(tileID % tilesPerRow) * tileWidth,
                    (float)(tileID / tilesPerRow) * tileHeight,
                    tileWidth,
                    tileHeight
                };

                /*/ Ensure tiles are drawn at viewport origin on screen
                Vector2 drawPos = {
                    (viewportX*tileWidth)  + (x * tileWidth * scale + offsetX),
                    (viewportY*tileHeight) + (y * tileHeight * scale + offsetY)
                };//*/

                Vector2 drawPos = {
                  x * tileWidth * scale + offsetX,
                  y * tileHeight * scale + offsetY
                };

                // Handle flipping
                bool hFlip = tilemap[layer].location[tileX][tileY].flags & 0x02;
                bool vFlip = tilemap[layer].location[tileX][tileY].flags & 0x04;
                if (hFlip) sourceRect.width = -sourceRect.width;
                if (vFlip) sourceRect.height = -sourceRect.height;

                // Draw the tile
                DrawTexturePro(
                    tileAtlas[layer].texture,
                    sourceRect,
                    (Rectangle) {
                    drawPos.x, drawPos.y, tileWidth* scale, tileHeight* scale
                },
                    (Vector2) {
                    0, 0
                }, 0.0f, WHITE
                );
            }
        }
    }
    EndTextureMode();

    Rectangle source = { 0.0f, 0.0f, (float)layerBuffer.texture.width, (float)-layerBuffer.texture.height };
    //Rectangle source = { 0.0f, 0.0f, (float)viewportW, (float)viewportY };
    //Rectangle destination = { (float)viewportX, (float)viewportY, (float)tilesAcross*MAX_TILE_WIDTH, (float)tilesDown*MAX_TILE_HEIGHT};
    //Rectangle destination = { 0, 0, GetScreenWidth(), GetScreenHeight() };
    Rectangle destination = {
    (float)viewportX*MAX_TILE_WIDTH,
    (float)viewportY*MAX_TILE_HEIGHT,
    (float)layerBuffer.texture.width,      // Could be exactly viewportW
    (float)layerBuffer.texture.height      // or viewportH
    };
    Vector2 origin = { 0,0 };
    Vector2 position = { (float)viewportX, (float)viewportY };

    /*DrawTexture(
        layerBuffer.texture,
        (float)viewportX,
        (float)viewportY,
        WHITE    
    );*/

    /*DrawTextureEx(
        layerBuffer.texture,
        position,
        0.0f,
        1.0f,
        WHITE
    );//*/

    DrawTexturePro(
        layerBuffer.texture,
        source,
        destination, 
        origin,
        0.0f,               // rotation
        WHITE               // tint
    );
}

void ShutdownTiles() {
    for (unsigned char layer = 0; layer < MAX_LAYERS; layer++) {

        if (tileBuffer.texture.id != 0) UnloadRenderTexture(tileBuffer);
        if (tileAtlas[layer].texture.id != 0) UnloadRenderTexture(tileAtlas[layer]);
    }

    return;
}