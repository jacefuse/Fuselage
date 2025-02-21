/* Fuselage tile layer */

#ifndef TILE_LAYER_H
#define TILE_LAYER_H

#include <raylib.h>
#include "colors.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_TILES 1024
#define MAX_MAP_WIDTH 512
#define MAX_MAP_HEIGHT 512
#define MAX_TILE_WIDTH 32
#define MAX_TILE_HEIGHT 32

    typedef struct {
        unsigned char* bitmap;     // Pointer to a bitmap for the tile
        unsigned char palette;     // Palette index
    } Tile;

	typedef struct {
        unsigned short tileTypeID;     // ID of tile type
        unsigned char* metadata;   // Pointer to optional metadata (NULL if unused)
        unsigned short collidableColors; // Colors to detect for collision
        unsigned char flags; // 0 collision on/off : 2 HFLIP : 3 VFLIP : 4-7 reserved 
        unsigned char transparency[16]; // The transparency level of each tile color
	} TileLocation;        

    typedef struct {
        unsigned short width;
        unsigned short height;
        unsigned short tileWidth;  // Width of individual tiles (default 32)
        unsigned short tileHeight; // Height of individual tiles (default 32)
        TileLocation location[MAX_MAP_WIDTH][MAX_MAP_HEIGHT];
    } Map;

	bool InitTiles(unsigned short tileWidth, unsigned short tileHeight, unsigned short tileCount);
    bool InitTileMap(unsigned char layer, unsigned short width, unsigned short height, unsigned short tileWidth, unsigned short tileHeight, unsigned short tileCount);

    bool AssignTile(int tileID, unsigned char* bitmap);
    bool SetTilePalette(int tileID, unsigned char palette);
    bool updateTileAtlas(int tileID);
    bool updateTile(int tileID);

    void TileTestPattern(int tileIndex);

    void displayTileAtlas();

    void ShutdownTiles();

#ifdef __cplusplus
}
#endif

#endif /* TILE_LAYER_H */