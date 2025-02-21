/* Fuselage tile layer Implementations */

#include "tiles.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// Most of this will probably be rewritten at some point.

bool layerActive[4] = { false, false, false, false }; // Might rework this;
RenderTexture2D tileBuffer;
RenderTexture2D tileAtlas;
//RenderTexture2D tileLayer;
Map tilemap[4];
Tile tiles[MAX_TILES];

static unsigned char testtile[MAX_TILE_WIDTH * MAX_TILE_HEIGHT] = { 0x0 };

short renderedtiles = 0;

int tilesPerRow;// = GetScreenWidth() / TILE_WIDTH; // Calculate tiles per row
int tilesPerCol;// = (MAX_TILES + tilesPerRow - 1) / tilesPerRow; 
int atlasWidth;// = tilesPerRow * TILE_WIDTH;       // Atlas width in pixels
int atlasHeight;// = (MAX_TILES / tilePerRow + (MAX_TILES % tilesPerRow != 0)) * TILE_HEIGHT; // Total height of the atlas

bool InitTileMap(unsigned char layer, unsigned short mapWidth, unsigned short mapHeight, unsigned short tileWidth, unsigned short tileHeight, unsigned short tileCount) {
    printf("Initializing all tile locations...\n");
    int initcount = 0;

    if (mapWidth >= MAX_MAP_WIDTH || mapHeight >= MAX_MAP_HEIGHT) {
        printf("MAP INITIALILZE FAILURE: Illegal Map Size!\n");
        return false;
    }

    for (int x = 0; x < mapWidth; x++) {
        for (int y = 0; y < mapHeight; y++)
        {
            tilemap[layer].location[x][y].tileTypeID = 0; // ID of tile type at Location
            tilemap[layer].location[x][y].metadata = NULL; // Pointer to optional metadata
            tilemap[layer].location[x][y].collidableColors = 0xFFFF; // Colors to detect for collision
            tilemap[layer].location[x][y].flags = 0x00; // flags: 0 collision on/off : 1 HFLIP : 2 VFLIP : 3-7 reserved
            for (int t = 0; t < 16; t++) tilemap[layer].location[x][y].transparency[t] = 255;
            initcount++;
        }
        
    }

    printf("Initialized %d tile locations on map.\n", initcount);

    InitTiles(tileWidth, tileHeight, tileCount);

    return true;
}

bool InitTiles(unsigned short tileWidth, unsigned short tileHeight, unsigned short tileCount){

    // Compute rows&cols for the tile index
    tilesPerRow = (int)sqrt(tileCount);// GetScreenWidth() / TILE_WIDTH;
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

    /*/ This code crates a checkerboard of 4 pixels by 4 pixels
    for (int y = 0; y < tileHeight; ++y) {
        for (int x = 0; x < tileWidth; x += 2) {
            int gridX = x / 8;
            int gridY = y / 8;
            int colorIndex0 = gridY * 4 + gridX;
            int colorIndex1 = colorIndex0;

            unsigned char packedValue = (colorIndex0 << 4) | (colorIndex1 & 0x0F);
            testtile[(y * tileWidth + x)] = packedValue;
            //printf("%u.", packedValue);
        }
        //printf("\n.");
    }*/

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
    tileAtlas = LoadRenderTexture(atlasWidth, atlasHeight);
    if (tileAtlas.texture.id == 0) return -1;

    return true;
}

// Set current tile data to temporary tile buffer (working stage)
bool updateTile(int tileID) {
    if (tileID < 0 || tileID >= MAX_TILES) {
        printf("UPDATE: Tile %d is out of range.\n", tileID);
        return false;
    }
    if (tiles[tileID].bitmap == NULL) {
        printf("UPDATE: tile %d is missing bitmap.\n", tileID);
        tiles[tileID].bitmap = testtile;
        //return false;
    }

    BeginTextureMode(tileBuffer);
    ClearBackground(BLANK);

    //printf("Palette: %d\n", tiles[tileID].palette);

    for (int y = 0; y < MAX_TILE_HEIGHT; y++) {
        for (int x = 0; x < MAX_TILE_WIDTH; x += 2) {
            // Calculate index into the bitmap
            int byteIndex = (y * MAX_TILE_WIDTH + x) / 2;
            unsigned char packedPixels = tiles[tileID].bitmap[byteIndex];
            //printf("%d", packedPixels);
            // Extract left pixel (high nibble) and right pixel (low nibble)
            unsigned char leftColorIndex = (packedPixels >> 4) & 0x0F;
            unsigned char rightColorIndex = packedPixels & 0x0F;
            //unsigned char transparency = tiles[tileID].transparency[0];
            //printf("I%d:%u(%u.%u)", byteIndex, tiles[tileID].bitmap[byteIndex], leftColorIndex, rightColorIndex);
            // Left pixel
            if (leftColorIndex != 0) {
                Color cl = Colors[tiles[tileID].palette][leftColorIndex];
                //cl.a = transparency;
                DrawPixel(x, y, cl);
                //printf("%d:%d.%d.%d:", leftColorIndex, cl.r, cl.g, cl.b);
            }

            // Right pixel
            if (rightColorIndex != 0) {
                Color cr = Colors[tiles[tileID].palette][rightColorIndex];
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

// Updates the Atlas
bool updateTileAtlas(int tileID) {
    // Validate tile index
    if (tileID < 0 || tileID >= MAX_TILES) {
        printf("Invalid tile index: %d\n", tileID);
        return false;
    }

    // Ensure the tile buffer exists
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
    BeginTextureMode(tileAtlas);
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

    if (tiles[tileID].bitmap != NULL) {
        // Draw the updated tile at the correct position
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

bool AssignTile(int tileID, unsigned char* bitmap) {
    //printf("ASSIGN TILE: %d\n", tileIndex);
    bool success;
    if (tileID < 0 || tileID >= MAX_TILES) {
        printf("Tile out of range.\n");
        return false;
    }
    tiles[tileID].bitmap = bitmap;
    success = updateTile(tileID);
    if (success) success = updateTileAtlas(tileID);

    return success;
}

bool SetTilePalette(int tileID, unsigned char palette) {
    tiles[tileID].palette = palette;
    //printf("Tile %d palette set to %d.\n", tileID, palette);
    return true;
}

void displayTileAtlas() {

    int screenwidth = GetScreenWidth();
    int screenheight = GetScreenHeight();
    int atlaswidth = tileAtlas.texture.width;
    int atlasheight = tileAtlas.texture.height;
    float scale = fminf((float)screenwidth / atlaswidth, (float)screenheight / atlasheight);

    Rectangle destRectangle = { (screenwidth - (atlaswidth * scale)) / 2.0f, (screenheight - (atlasheight * scale)) / 2.0f, atlaswidth * scale, atlasheight * scale };
    Rectangle sourceRectangle = { 0, 0, (float)atlaswidth, (float)atlasheight };

    DrawTexturePro(tileAtlas.texture, sourceRectangle, destRectangle, (Vector2) { 0, 0 }, 0.0f, WHITE);

    //DrawTexture(tileBuffer.texture, 0, 0, WHITE);

    //printf("Displaying Tile Atlas.\n");

    return;
}

// Insert Test Pattern
void TileTestPattern(int tileIndex) {
    AssignTile(tileIndex, testtile);
    updateTileAtlas(tileIndex);
    return;
};

void ShutdownTiles() {

    UnloadRenderTexture(tileBuffer);
//    UnloadRenderTexture(tileLayer);
    UnloadRenderTexture(tileAtlas);

    return;
}