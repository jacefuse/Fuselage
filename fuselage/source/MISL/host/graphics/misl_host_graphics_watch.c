/* Mark byte ranges during stores and attempt native uploads after the slice. */
#include "misl_host_graphics_internal.h"
#include "../runtime/misl_host_state_internal.h"
#include "../../../GDMF/sprites/gdmf_sprites.h"
#include "../../../GDMF/tiles/gdmf_tiles.h"

_Static_assert((uint64_t)SPRITE_WIDTH* SPRITE_HEIGHT / 2 == MISL_SPRITE_STRIDE &&
                   MAX_SPRITE_BITMAPS == MISL_SPRITE_SLOTS,
               "Sprite bitmap mapping differs");
_Static_assert(MAX_TILES == MISL_TILE_SLOTS && MAX_TILE_LAYERS == MISL_TILE_LAYERS,
               "Tile bitmap mapping differs");
_Static_assert((((uint64_t)MAX_TILE_WIDTH * MAX_TILE_HEIGHT + 1u) / 2u) * MISL_TILE_SLOTS <=
                   MISL_TILE_LAYER_STRIDE,
               "Maximum native tile bitmaps exceed a mapped layer");

static uint32_t fuselage_guest_tile_slot_bytes(uint8_t layer) {
    uint64_t width = GetTileLayerTileWidth(layer), height = GetTileLayerTileHeight(layer);
    uint64_t byte_count = (width * height + 1u) / 2u;
    return byte_count <= MISL_TILE_LAYER_STRIDE ? (uint32_t)byte_count : 0;
}

static void fuselage_guest_watch_graphics(uint32_t address, uint32_t byte_count, void* user) {
    (void)user;
    uint64_t end = (uint64_t)address + byte_count;
    if (!byte_count) {
        return;
    }
    uint64_t lower = address > MISL_TILE_MAP_ADDR ? address : MISL_TILE_MAP_ADDR;
    uint64_t upper = end < MISL_MMIO_BASE ? end : MISL_MMIO_BASE;
    if (lower < upper) {
        uint32_t first_layer = (uint32_t)(lower - MISL_TILE_MAP_ADDR) >> MISL_TILE_MAP_SHIFT;
        uint32_t last_layer = (uint32_t)(upper - 1u - MISL_TILE_MAP_ADDR) >> MISL_TILE_MAP_SHIFT;
        for (uint32_t layer = first_layer; layer <= last_layer; layer++) {
            fuselage_guest_session.tile_map_dirty[layer] = 1;
            fuselage_guest_session.tile_maps_dirty = 1;
        }
    }
    lower = address > MISL_SPRITE_ADDR ? address : MISL_SPRITE_ADDR;
    upper = end < (uint64_t)MISL_SPRITE_ADDR + MISL_SPRITE_BYTES
                ? end
                : (uint64_t)MISL_SPRITE_ADDR + MISL_SPRITE_BYTES;
    if (lower < upper) {
        uint32_t first_slot = (uint32_t)(lower - MISL_SPRITE_ADDR) / MISL_SPRITE_STRIDE;
        uint32_t last_slot = (uint32_t)(upper - 1u - MISL_SPRITE_ADDR) / MISL_SPRITE_STRIDE;
        for (uint32_t slot = first_slot; slot <= last_slot; slot++) {
            fuselage_guest_session.sprite_dirty[slot >> 5] |= 1u << (slot & 31u);
            fuselage_guest_session.sprites_dirty = 1;
        }
    }
    lower = address > MISL_TILE_ADDR ? address : MISL_TILE_ADDR;
    upper =
        end < (uint64_t)MISL_TILE_ADDR + MISL_TILE_BYTES ? end : (uint64_t)MISL_TILE_ADDR + MISL_TILE_BYTES;
    if (lower >= upper) {
        return;
    }
    uint32_t first_layer = (uint32_t)(lower - MISL_TILE_ADDR) / MISL_TILE_LAYER_STRIDE;
    uint32_t last_layer = (uint32_t)(upper - 1u - MISL_TILE_ADDR) / MISL_TILE_LAYER_STRIDE;
    for (uint32_t layer = first_layer; layer <= last_layer; layer++) {
        uint32_t slot_bytes = fuselage_guest_tile_slot_bytes((uint8_t)layer);
        if (!slot_bytes) {
            continue;
        }
        uint64_t layer_base = MISL_TILE_LAYER_ADDR(layer);
        uint32_t offset = lower > layer_base ? (uint32_t)(lower - layer_base) : 0;
        uint32_t end_offset = upper < layer_base + MISL_TILE_LAYER_STRIDE ? (uint32_t)(upper - layer_base)
                                                                          : MISL_TILE_LAYER_STRIDE;
        uint32_t first_slot = offset / slot_bytes, last_slot = (end_offset - 1u) / slot_bytes;
        for (uint32_t slot = first_slot; slot <= last_slot && slot < MISL_TILE_SLOTS; slot++) {
            fuselage_guest_session.tile_dirty[layer][slot >> 5] |= 1u << (slot & 31u);
            fuselage_guest_session.tiles_dirty = 1;
        }
    }
}

void fuselage_guest_install_graphics_watch(void) {
    if (fuselage_guest_session.phase == MISL_HOST_RUNNING) {
        VIVIAN_SetStoreWatch(&fuselage_guest_session.cpu, MISL_TILE_MAP_ADDR,
                             MISL_TILE_ADDR + MISL_TILE_BYTES, fuselage_guest_watch_graphics, NULL);
    }
}

void fuselage_guest_flush_graphics(void) {
    if (!fuselage_guest_session.cpu.mem) {
        return;
    }
    if (fuselage_guest_session.sprites_dirty) {
        for (uint32_t word = 0; word < MISL_HOST_SPRITE_DIRTY_WORDS; word++) {
            uint32_t bits = fuselage_guest_session.sprite_dirty[word];
            fuselage_guest_session.sprite_dirty[word] = 0;
            while (bits) {
                uint32_t bit = (uint32_t)__builtin_ctz(bits);
                bits &= bits - 1u;
                uint32_t slot = word * 32u + bit;
                if (slot < MISL_SPRITE_SLOTS) {
                    UploadSpriteBitmap((SpriteBitmapID)slot,
                                       fuselage_guest_get_window_pointer(
                                           MISL_SPRITE_ADDR + slot * MISL_SPRITE_STRIDE, MISL_SPRITE_STRIDE));
                }
            }
        }
        fuselage_guest_session.sprites_dirty = 0;
    }
    if (fuselage_guest_session.tiles_dirty) {
        for (uint32_t layer = 0; layer < MISL_TILE_LAYERS; layer++) {
            uint32_t slot_bytes = fuselage_guest_tile_slot_bytes((uint8_t)layer);
            for (uint32_t word = 0; word < MISL_HOST_TILE_DIRTY_WORDS; word++) {
                uint32_t bits = fuselage_guest_session.tile_dirty[layer][word];
                fuselage_guest_session.tile_dirty[layer][word] = 0;
                while (bits) {
                    uint32_t bit = (uint32_t)__builtin_ctz(bits);
                    bits &= bits - 1u;
                    uint32_t slot = word * 32u + bit;
                    if (slot_bytes && slot < MISL_TILE_SLOTS) {
                        UploadTileBitmap((uint8_t)layer, (int)slot,
                                         fuselage_guest_get_window_pointer(
                                             MISL_TILE_LAYER_ADDR(layer) + slot * slot_bytes, slot_bytes));
                    }
                }
            }
        }
        fuselage_guest_session.tiles_dirty = 0;
    }
    if (fuselage_guest_session.tile_maps_dirty) {
        for (uint32_t layer = 0; layer < MISL_TILE_MAP_BANKS; layer++) {
            if (fuselage_guest_session.tile_map_dirty[layer]) {
                fuselage_guest_session.tile_map_dirty[layer] = 0;
                MarkTileLayerDirty((uint8_t)layer);
            }
        }
        fuselage_guest_session.tile_maps_dirty = 0;
    }
}
