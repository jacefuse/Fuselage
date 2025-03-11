/* Fuselage tile layer */

#ifndef TILE_LAYER_H
#define TILE_LAYER_H

#include <raylib.h>
#include "colors.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_TILES 1024
#define MAX_LAYERS 4
#define MAX_MAP_WIDTH 512
#define MAX_MAP_HEIGHT 512
#define MAX_TILE_WIDTH 32
#define MAX_TILE_HEIGHT 32

    typedef struct {
        unsigned char* bitmap;     // Pointer to a bitmap for the tile
        unsigned char palette;     // Palette index
    } Tile;

	typedef struct {
        unsigned short tileTypeID;          // ID of tile type
        unsigned char* metadata;            // Pointer to optional metadata (NULL if unused)
        unsigned short collidableColors;    // Colors to detect for collision
        unsigned char flags;                // 0 collision on/off : 2 HFLIP : 3 VFLIP : 4-7 reserved 
        unsigned char transparency[16];     // The transparency level of each tile color
	} TileLocation;        

    typedef struct {
        bool wrapX;
        bool wrapY;
        double mapOffsetX;
        double mapOffsetY;
        unsigned short width;
        unsigned short height;
        unsigned short tileWidth;
        unsigned short tileHeight;
        unsigned short viewportX;       
        unsigned short viewportY;      
        unsigned short viewportWidth;  
        unsigned short viewportHeight;
        float scale;                    // Scale factor (1.0 = normal, 2.0 = 2x, etc.)
        TileLocation location[MAX_MAP_WIDTH][MAX_MAP_HEIGHT];
    } Map;


	bool InitTiles(unsigned char layer, unsigned short tileWidth, unsigned short tileHeight, unsigned short tileCount);
    bool InitTileMap(unsigned char layer, unsigned short mapWidth, unsigned short mapHeight,
        unsigned short tileWidth, unsigned short tileHeight, unsigned short tileCount,
        float scale);
    bool AssignTile(unsigned char layer, int tileID, unsigned char* bitmap);

    void SetTileViewport(unsigned char layer, unsigned short x, unsigned short y, unsigned short width, unsigned short height);
    void SetTileLayerScale(unsigned char layer, float scale);
    bool SetTilePalette(unsigned char layer, int tileID, unsigned char palette);
    void SetTileMapWrapping(unsigned char layer, bool wrapX, bool wrapY);

    bool updateTileAtlas(unsigned char layer, int tileID);
    bool updateTile(unsigned char layer, int tileID);

    void TileTestPattern(unsigned char layer, int tileIndex);

    void displayTileAtlas(unsigned char layer);

    void ScrollTileMap(unsigned char layer, double deltaX, double deltaY);

    void PlaceTileInMap(unsigned char layer, unsigned short x, unsigned short y, unsigned short tileID);

    void DrawTileLayer(unsigned char layer);

    void ShutdownTiles();

#ifdef __cplusplus
}
#endif

#endif /* TILE_LAYER_H */