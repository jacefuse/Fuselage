/* Fuselage tile layer */

#ifndef TILE_LAYER_H
#define TILE_LAYER_H
#define FUSELAGE_TILELAYER_VERSION "20250129"

#include <raylib.h>
#include "colors.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_TILES 1024
#define MAX_MAP_SIZE 512

    // Structure representing a single tile
    // Most of this will probably change - all data will be packed for efficiency.
    typedef struct {
        unsigned short tileIndex;  // Index in the tile atlas
        unsigned char palette;     // Palette index
        bool solid;                // Collision flag
        unsigned char* bitmap;     // Pointer to a bitmap for the tile
        unsigned char* metadata;   // Pointer to optional metadata (NULL if unused)

    } Tile;

    // Structure representing a tilemap - ditto for this stuff
    typedef struct {
        Tile* tiles;
        unsigned short width;
        unsigned short height;
        unsigned short tileWidth;  // Width of individual tiles (default 32)
        unsigned short tileHeight; // Height of individual tiles (default 32)
    } TileMap;

    // Function prototypes
    int InitTileMap(TileMap* tilemap, const unsigned short* mapData, unsigned short width, unsigned short height);
    void DrawTileMap(TileMap* tilemap, Texture2D tileAtlas);
    void SetTile(TileMap* tilemap, int x, int y, unsigned short tileIndex, unsigned char palette, bool solid);
    Tile GetTile(TileMap* tilemap, int x, int y);
    void FreeTileMap(TileMap* tilemap);

#ifdef __cplusplus
}
#endif

#endif /* TILE_LAYER_H */
