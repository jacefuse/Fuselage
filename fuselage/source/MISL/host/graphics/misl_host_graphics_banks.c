/* Refresh on lifecycle changes and before slices; GDMF does not call the host. */
#include "../runtime/misl_host_state_internal.h"
#include "../../../GDMF/pixies/gdmf_pixies.h"
#include "../../../GDMF/tiles/gdmf_tiles.h"
#include "../../../GDMF/sprites/gdmf_sprites.h"
#include "../../../GDMF/gdmf_interactions.h"
#include "../../../GDMF/gdmf_colors.h"
#include <stddef.h>

_Static_assert(PIXIE_RAM_SIZE == MISL_PIXIE_RAM_STRIDE && MAX_PIXIES == MISL_PIXIE_RAM_BANKS,
               "Pixie bank mapping differs from native storage");
_Static_assert(MAX_TILE_LAYERS == MISL_TILE_MAP_BANKS && MAX_MAP_WIDTH == MISL_TILE_MAP_WIDTH &&
                   MAX_MAP_HEIGHT == MISL_TILE_MAP_HEIGHT,
               "Tile bank dimensions differ");
_Static_assert(sizeof(TileLocation) == MISL_TILE_CELL_BYTES, "Tile cell wire size differs");
#define MISL_HOST_CELL_FIELD(field)                                                                          \
    _Static_assert(offsetof(TileLocation, field) == offsetof(MislTileCell, field), "Tile cell field "        \
                                                                                   "differs")
MISL_HOST_CELL_FIELD(tileTypeID);
MISL_HOST_CELL_FIELD(flags);
MISL_HOST_CELL_FIELD(offsetX);
MISL_HOST_CELL_FIELD(offsetY);
MISL_HOST_CELL_FIELD(palette);
MISL_HOST_CELL_FIELD(transparency);
MISL_HOST_CELL_FIELD(showzero);
MISL_HOST_CELL_FIELD(tag);
#undef MISL_HOST_CELL_FIELD

_Static_assert(sizeof(SpritePairRef) == 4 && sizeof(TileCellRef) == 4 && sizeof(short) == 2,
               "Collision arrays differ from guest records");
_Static_assert(sizeof(SpriteCollisionInfo) == 8 && offsetof(SpriteCollisionInfo, otherSprite) == 0 &&
                   offsetof(SpriteCollisionInfo, type) == 4,
               "Collision reports differ from guest records");
_Static_assert(sizeof(PixieTransform) == 24 && sizeof(Color) == 4, "Native graphics records differ");
_Static_assert(sizeof(PixieCommand) == sizeof(MislPixieCmd) &&
                   offsetof(PixieCommand, pixie) == offsetof(MislPixieCmd, pixie) &&
                   offsetof(PixieCommand, opcode) == offsetof(MislPixieCmd, opcode) &&
                   offsetof(PixieCommand, flags) == offsetof(MislPixieCmd, flags) &&
                   offsetof(PixieCommand, args) == offsetof(MislPixieCmd, args),
               "Pixie list records differ");

void fuselage_guest_refresh_pixie_banks(void) {
    for (uint32_t index = 0; index < MISL_PIXIE_RAM_BANKS; index++) {
        fuselage_guest_session.pixie_banks[index] = (uint8_t*)(uintptr_t)GetPixieRAMReadPtr((int)index);
    }
}
void fuselage_guest_refresh_tile_banks(void) {
    for (uint32_t layer = 0; layer < MISL_TILE_MAP_BANKS; layer++) {
        fuselage_guest_session.tile_banks[layer] = (uint8_t*)GetTileLayerCellsPtr((uint8_t)layer);
    }
}
