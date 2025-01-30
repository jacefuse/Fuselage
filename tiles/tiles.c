/* Fuselage tile layer Implementations */

#include "tiles.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// Most of this will probably be rewritten at some point.

bool layerActive[4] = { false, false, false, false }; // Might rework this;

int InitTileMap(TileMap* tilemap, const unsigned short* mapData, unsigned short width, unsigned short height) {

    if (width > MAX_MAP_SIZE || height > MAX_MAP_SIZE) {
        printf("Error: Map size exceeds maximum allowed dimensions.\n");
        return -1;
    }

    tilemap->width = width;
    tilemap->height = height;
    tilemap->tiles = (Tile*)malloc(width * height * sizeof(Tile));
    if (!tilemap->tiles) {
        printf("Error: Memory allocation failed for tilemap.\n");
        return -1;
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int index = y * width + x;
            tilemap->tiles[index].tileIndex = mapData[index];
            tilemap->tiles[index].palette = 0;
            tilemap->tiles[index].solid = false;
        }
    }

    return 0;
}

void DrawTileMap(TileMap* tilemap, Texture2D tileAtlas) {

    for (int y = 0; y < tilemap->height; y++) {
        for (int x = 0; x < tilemap->width; x++) {
            Tile tile = tilemap->tiles[y * tilemap->width + x];
            unsigned short tilesPerRow = tileAtlas.width / tilemap->tileWidth;

            Rectangle source = {
                (tile.tileIndex % tilesPerRow) * tilemap->tileWidth,
                (tile.tileIndex / tilesPerRow) * tilemap->tileHeight,
                tilemap->tileWidth, tilemap->tileHeight };

            Rectangle dest = {
                x * tilemap->tileWidth,
                y * tilemap->tileHeight,
                tilemap->tileWidth,
                tilemap->tileHeight };

            DrawTextureRec(
                tileAtlas,
                source,
                (Vector2) { dest.x, dest.y },
                WHITE);
        }
    }

    return;
}

void SetTile(TileMap* tilemap, int x, int y, unsigned short tileIndex, unsigned char palette, bool solid) {

    if (x >= 0 && x < tilemap->width && y >= 0 && y < tilemap->height) {
        int index = y * tilemap->width + x;
        tilemap->tiles[index].tileIndex = tileIndex;
        tilemap->tiles[index].palette = palette;
        tilemap->tiles[index].solid = solid;
    }

    return;
}

Tile GetTile(TileMap* tilemap, int x, int y) {

    if (x >= 0 && x < tilemap->width && y >= 0 && y < tilemap->height) {
        return tilemap->tiles[y * tilemap->width + x];
    }

    return (Tile) { 0, 0, false };  // Default tile if out of bounds
}

void FreeTileMap(TileMap* tilemap) {

    free(tilemap->tiles);
    tilemap->tiles = NULL;

    return;
}
