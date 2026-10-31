/* Native interactions adapter. SEE: (MISL_HOST_GRAPHICS.md, Interactions) */
#include "../runtime/misl_host_arguments_internal.h"
#include "../runtime/misl_host_pointers_internal.h"
#include "../runtime/misl_host_services_internal.h"
#include "misl_host_graphics_internal.h"
#include "../../misl_abi.h"
#include "../../../GDMF/gdmf_interactions.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

int fuselage_guest_service_interactions(uint32_t call) {
    switch (call) {
    case MISL_IACT_DIST_SPRITES:
        fuselage_guest_set_float_result(GetInteractionsSpriteDistance((int)fuselage_guest_get_argument(0),
                                                                      (int)fuselage_guest_get_argument(1)));
        return 1;
    case MISL_IACT_DIR_SPRITES:
        fuselage_guest_set_float_result(GetInteractionsSpriteDirection((int)fuselage_guest_get_argument(0),
                                                                       (int)fuselage_guest_get_argument(1)));
        return 1;
    case MISL_IACT_LOCALPIX_TO_WORLD: {
        void* output_x = fuselage_guest_resolve_output(fuselage_guest_get_argument(3), sizeof(float));
        void* output_y = fuselage_guest_resolve_output(fuselage_guest_get_argument(4), sizeof(float));
        float world_x, world_y;
        int accepted = output_x && output_y &&
                       GetInteractionsSpritePixelWorldPosition(
                           (int)fuselage_guest_get_argument(0), (int)fuselage_guest_get_argument(1),
                           (int)fuselage_guest_get_argument(2), &world_x, &world_y);
        if (accepted) {
            memcpy(output_x, &world_x, sizeof world_x);
            memcpy(output_y, &world_y, sizeof world_y);
        }
        fuselage_guest_set_result(accepted ? 1u : 0u);
        return 1;
    }
    case MISL_IACT_SPRITE_TILE_COLLISION:
        fuselage_guest_set_result(CheckInteractionsSpriteTileCollision(
            (int)fuselage_guest_get_argument(0), (uint8_t)fuselage_guest_get_argument(1),
            (unsigned char)fuselage_guest_get_argument(2)));
        return 1;
    case MISL_IACT_SPRITE_TILE_CELLS: {
        uint32_t capacity = fuselage_guest_get_argument(3);
        void* output = NULL;
        TileCellRef* cells = NULL;
        if (capacity && fuselage_guest_get_argument(2)) {
            if (capacity > INT_MAX) {
                fuselage_guest_set_result(0);
                return 1;
            }
            output = fuselage_guest_resolve_record_output(fuselage_guest_get_argument(2), capacity,
                                                          sizeof(TileCellRef));
            if (!output) {
                fuselage_guest_set_result(0);
                return 1;
            }
            cells = malloc((size_t)capacity * sizeof *cells);
            if (!cells) {
                fuselage_guest_set_result(0);
                return 1;
            }
        }
        int total = GetInteractionsSpriteTileCollisionCells((int)fuselage_guest_get_argument(0),
                                                            (uint8_t)fuselage_guest_get_argument(1), cells,
                                                            cells ? (int)capacity : 0);
        uint32_t written = total > 0 ? (uint32_t)total : 0;
        if (written > capacity) {
            written = capacity;
        }
        if (output && written) {
            memcpy(output, cells, (size_t)written * sizeof *cells);
        }
        free(cells);
        fuselage_guest_set_result((uint32_t)total);
        return 1;
    }
    case MISL_IACT_SPRITE_TILE_BATCH: {
        uint32_t count = fuselage_guest_get_argument(1);
        if (!count || count > MISL_HOST_BATCH_MAX_ITEMS) {
            fuselage_guest_set_result(0);
            return 1;
        }
        uint32_t input_bytes = count * (uint32_t)sizeof(short);
        unsigned char* results = fuselage_guest_resolve_batch_output(fuselage_guest_get_argument(4), count);
        if (!results || !fuselage_guest_batch_ranges_valid(fuselage_guest_get_argument(0), input_bytes,
                                                           fuselage_guest_get_argument(4), count)) {
            fuselage_guest_set_result(0);
            return 1;
        }
        short* sprites = fuselage_guest_copy_records(fuselage_guest_get_argument(0), count, sizeof(short));
        int matches = sprites ? CheckInteractionsSpriteTileCollisionBatch(
                                    sprites, (int)count, (uint8_t)fuselage_guest_get_argument(2),
                                    (unsigned char)fuselage_guest_get_argument(3), results)
                              : 0;
        free(sprites);
        fuselage_guest_set_result((uint32_t)matches);
        return 1;
    }
    default:
        return fuselage_guest_unknown_ecall(call);
    }
}
