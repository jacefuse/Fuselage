/* Native tiles adapter. SEE: (MISL_HOST_GRAPHICS.md, Tiles) */
#include "../runtime/misl_host_arguments_internal.h"
#include "../runtime/misl_host_pointers_internal.h"
#include "../runtime/misl_host_services_internal.h"
#include "../../misl_abi.h"
#include "../../../GDMF/tiles/gdmf_tiles.h"
#include "../../../GDMF/tiles/gdmf_tiles_help.h"
#include <string.h>

int fuselage_guest_service_tiles(uint32_t call) {
    switch (call) {
    case MISL_TILE_INIT:
        fuselage_guest_set_result(
            InitTileLayer((uint8_t)fuselage_guest_get_argument(0), (uint16_t)fuselage_guest_get_argument(1),
                          (uint16_t)fuselage_guest_get_argument(2), (uint16_t)fuselage_guest_get_argument(3),
                          (uint16_t)fuselage_guest_get_argument(4), (uint16_t)fuselage_guest_get_argument(5),
                          fuselage_guest_get_float_argument(6))
                ? 1u
                : 0u);
        fuselage_guest_refresh_tile_banks();
        return 1;
    case MISL_TILE_RELEASE:
        fuselage_guest_set_result(ReleaseTileLayer((uint8_t)fuselage_guest_get_argument(0)) ? 1u : 0u);
        fuselage_guest_refresh_tile_banks();
        return 1;
    case MISL_TILE_UPLOAD_BITMAP: {
        uint32_t width = GetTileLayerTileWidth((uint8_t)fuselage_guest_get_argument(0));
        uint32_t height = GetTileLayerTileHeight((uint8_t)fuselage_guest_get_argument(0));
        uint64_t byte_count = ((uint64_t)width * height + 1u) / 2u;
        const unsigned char* bitmap =
            byte_count && byte_count <= UINT32_MAX
                ? fuselage_guest_resolve_input(fuselage_guest_get_argument(2), (uint32_t)byte_count)
                : NULL;
        fuselage_guest_set_result(bitmap && UploadTileBitmap((uint8_t)fuselage_guest_get_argument(0),
                                                             (int)fuselage_guest_get_argument(1), bitmap)
                                      ? 1u
                                      : 0u);
        return 1;
    }
    case MISL_TILE_GET_VISIBLE:
        fuselage_guest_set_result(GetTileLayerVisible((uint8_t)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_TILE_SET_VISIBLE:
        fuselage_guest_set_result(
            SetTileLayerVisible((uint8_t)fuselage_guest_get_argument(0), fuselage_guest_get_argument(1) != 0)
                ? 1u
                : 0u);
        return 1;
    case MISL_TILE_TOGGLE_VISIBLE:
        fuselage_guest_set_result(ToggleTileLayerVisible((uint8_t)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_TILE_GET_ENABLED:
        fuselage_guest_set_result(GetTileLayerEnabled((uint8_t)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_TILE_SET_ENABLED:
        fuselage_guest_set_result(
            SetTileLayerEnabled((uint8_t)fuselage_guest_get_argument(0), fuselage_guest_get_argument(1) != 0)
                ? 1u
                : 0u);
        return 1;
    case MISL_TILE_TOGGLE_ENABLED:
        fuselage_guest_set_result(ToggleTileLayerEnabled((uint8_t)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_TILE_PLACE:
        fuselage_guest_set_result(
            PlaceTile((uint8_t)fuselage_guest_get_argument(0), (uint16_t)fuselage_guest_get_argument(1),
                      (uint16_t)fuselage_guest_get_argument(2), (uint16_t)fuselage_guest_get_argument(3))
                ? 1u
                : 0u);
        return 1;
    case MISL_TILE_FILL:
        fuselage_guest_set_result(
            FillTileLayer((uint8_t)fuselage_guest_get_argument(0), (uint16_t)fuselage_guest_get_argument(1))
                ? 1u
                : 0u);
        return 1;
    case MISL_TILE_FILL_ROW:
        fuselage_guest_set_result(FillTileRow((uint8_t)fuselage_guest_get_argument(0),
                                              (uint16_t)fuselage_guest_get_argument(1),
                                              (uint16_t)fuselage_guest_get_argument(2))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_TILE_FILL_COLUMN:
        fuselage_guest_set_result(FillTileColumn((uint8_t)fuselage_guest_get_argument(0),
                                                 (uint16_t)fuselage_guest_get_argument(1),
                                                 (uint16_t)fuselage_guest_get_argument(2))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_TILE_FILL_REGION:
        fuselage_guest_set_result(
            FillTileRegion((uint8_t)fuselage_guest_get_argument(0), (uint16_t)fuselage_guest_get_argument(1),
                           (uint16_t)fuselage_guest_get_argument(2), (uint16_t)fuselage_guest_get_argument(3),
                           (uint16_t)fuselage_guest_get_argument(4), (uint16_t)fuselage_guest_get_argument(5))
                ? 1u
                : 0u);
        return 1;
    case MISL_TILE_GET_TILE_WIDTH:
        fuselage_guest_set_result(GetTileLayerTileWidth((uint8_t)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_TILE_GET_TILE_HEIGHT:
        fuselage_guest_set_result(GetTileLayerTileHeight((uint8_t)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_TILE_CHECK_POINT_COLLISION:
        fuselage_guest_set_result(CheckTilePointCollision((uint8_t)fuselage_guest_get_argument(0),
                                                          fuselage_guest_get_float_argument(1),
                                                          fuselage_guest_get_float_argument(2))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_TILE_SET_MAP_ROTATION:
        fuselage_guest_set_result(
            SetTileMapRotation((uint8_t)fuselage_guest_get_argument(0), fuselage_guest_get_float_argument(1))
                ? 1u
                : 0u);
        return 1;
    case MISL_TILE_GET_MAP_ROTATION:
        fuselage_guest_set_float_result(GetTileMapRotation((uint8_t)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_TILE_SET_MAP_PIVOT:
        fuselage_guest_set_result(SetTileMapPivot((uint8_t)fuselage_guest_get_argument(0),
                                                  fuselage_guest_get_float_argument(1),
                                                  fuselage_guest_get_float_argument(2))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_TILE_GET_MAP_PIVOT_X: {
        float pivot_x = 0.0f, pivot_y = 0.0f;
        GetTileMapPivot((uint8_t)fuselage_guest_get_argument(0), &pivot_x, &pivot_y);
        fuselage_guest_set_float_result(pivot_x);
    }
        return 1;
    case MISL_TILE_GET_MAP_PIVOT_Y: {
        float pivot_x = 0.0f, pivot_y = 0.0f;
        GetTileMapPivot((uint8_t)fuselage_guest_get_argument(0), &pivot_x, &pivot_y);
        fuselage_guest_set_float_result(pivot_y);
    }
        return 1;
    case MISL_TILE_SET_CELL_PALETTE:
        fuselage_guest_set_result(SetTileCellPalette((uint8_t)fuselage_guest_get_argument(0),
                                                     (uint16_t)fuselage_guest_get_argument(1),
                                                     (uint16_t)fuselage_guest_get_argument(2),
                                                     (uint8_t)fuselage_guest_get_argument(3))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_TILE_SET_PALETTE:
        fuselage_guest_set_result(SetTileLayerPalette((uint8_t)fuselage_guest_get_argument(0),
                                                      (uint8_t)fuselage_guest_get_argument(1))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_TILE_SET_CELL_TRANSP:
        fuselage_guest_set_result(SetTileCellTransparency((uint8_t)fuselage_guest_get_argument(0),
                                                          (uint16_t)fuselage_guest_get_argument(1),
                                                          (uint16_t)fuselage_guest_get_argument(2),
                                                          (uint8_t)fuselage_guest_get_argument(3))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_TILE_SET_TRANSP:
        fuselage_guest_set_result(SetTileLayerTransparency((uint8_t)fuselage_guest_get_argument(0),
                                                           (uint8_t)fuselage_guest_get_argument(1))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_TILE_SET_CELL_SHOWZERO:
        fuselage_guest_set_result(SetTileCellShowZero((uint8_t)fuselage_guest_get_argument(0),
                                                      (uint16_t)fuselage_guest_get_argument(1),
                                                      (uint16_t)fuselage_guest_get_argument(2),
                                                      fuselage_guest_get_argument(3) != 0)
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_TILE_SET_SHOWZERO:
        fuselage_guest_set_result(
            SetTileLayerShowZero((uint8_t)fuselage_guest_get_argument(0), fuselage_guest_get_argument(1) != 0)
                ? 1u
                : 0u);
        return 1;
    case MISL_TILE_SET_CELL_FLIP_X:
        fuselage_guest_set_result(SetTileCellFlipX((uint8_t)fuselage_guest_get_argument(0),
                                                   (uint16_t)fuselage_guest_get_argument(1),
                                                   (uint16_t)fuselage_guest_get_argument(2),
                                                   fuselage_guest_get_argument(3) != 0)
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_TILE_SET_CELL_FLIP_Y:
        fuselage_guest_set_result(SetTileCellFlipY((uint8_t)fuselage_guest_get_argument(0),
                                                   (uint16_t)fuselage_guest_get_argument(1),
                                                   (uint16_t)fuselage_guest_get_argument(2),
                                                   fuselage_guest_get_argument(3) != 0)
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_TILE_GET_CELL_FLIP_X:
        fuselage_guest_set_result(GetTileCellFlipX((uint8_t)fuselage_guest_get_argument(0),
                                                   (uint16_t)fuselage_guest_get_argument(1),
                                                   (uint16_t)fuselage_guest_get_argument(2))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_TILE_GET_CELL_FLIP_Y:
        fuselage_guest_set_result(GetTileCellFlipY((uint8_t)fuselage_guest_get_argument(0),
                                                   (uint16_t)fuselage_guest_get_argument(1),
                                                   (uint16_t)fuselage_guest_get_argument(2))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_TILE_SET_ROTATION:
        fuselage_guest_set_result(SetTileCellRotation((uint8_t)fuselage_guest_get_argument(0),
                                                      (uint16_t)fuselage_guest_get_argument(1),
                                                      (uint16_t)fuselage_guest_get_argument(2),
                                                      fuselage_guest_get_argument(3) != 0)
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_TILE_GET_ROTATION:
        fuselage_guest_set_result(GetTileCellRotation((uint8_t)fuselage_guest_get_argument(0),
                                                      (uint16_t)fuselage_guest_get_argument(1),
                                                      (uint16_t)fuselage_guest_get_argument(2))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_TILE_SET_CELL_COLLISION:
        fuselage_guest_set_result(SetTileCellCollision((uint8_t)fuselage_guest_get_argument(0),
                                                       (uint16_t)fuselage_guest_get_argument(1),
                                                       (uint16_t)fuselage_guest_get_argument(2),
                                                       fuselage_guest_get_argument(3) != 0)
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_TILE_SET_WRAP_X:
        fuselage_guest_set_result(
            SetTileMapWrapX((uint8_t)fuselage_guest_get_argument(0), fuselage_guest_get_argument(1) != 0)
                ? 1u
                : 0u);
        return 1;
    case MISL_TILE_SET_WRAP_Y:
        fuselage_guest_set_result(
            SetTileMapWrapY((uint8_t)fuselage_guest_get_argument(0), fuselage_guest_get_argument(1) != 0)
                ? 1u
                : 0u);
        return 1;
    case MISL_TILE_GET_WRAP_X:
        fuselage_guest_set_result(GetTileMapWrapX((uint8_t)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_TILE_GET_WRAP_Y:
        fuselage_guest_set_result(GetTileMapWrapY((uint8_t)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_TILE_SET_SCALE:
        fuselage_guest_set_result(
            SetTileLayerScale((uint8_t)fuselage_guest_get_argument(0), fuselage_guest_get_float_argument(1))
                ? 1u
                : 0u);
        return 1;
    case MISL_TILE_SET_VIEWPORT:
        fuselage_guest_set_result(
            SetTileViewport((uint8_t)fuselage_guest_get_argument(0), fuselage_guest_get_float_argument(1),
                            fuselage_guest_get_float_argument(2), fuselage_guest_get_float_argument(3),
                            fuselage_guest_get_float_argument(4))
                ? 1u
                : 0u);
        return 1;
    case MISL_TILE_SCROLL:
        fuselage_guest_set_result(ScrollTileMap((uint8_t)fuselage_guest_get_argument(0),
                                                fuselage_guest_get_double_argument(1),
                                                fuselage_guest_get_double_argument(3))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_TILE_SET_MAP_OFFSET:
        fuselage_guest_set_result(SetTileMapOffset((uint8_t)fuselage_guest_get_argument(0),
                                                   fuselage_guest_get_double_argument(1),
                                                   fuselage_guest_get_double_argument(3))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_TILE_SET_CELL_OFFSET:
        fuselage_guest_set_result(SetTileCellOffset((uint8_t)fuselage_guest_get_argument(0),
                                                    (uint16_t)fuselage_guest_get_argument(1),
                                                    (uint16_t)fuselage_guest_get_argument(2),
                                                    (int8_t)fuselage_guest_get_argument(3),
                                                    (int8_t)fuselage_guest_get_argument(4))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_TILE_SET_COLLIDABLE:
        fuselage_guest_set_result(SetTileLayerCollidableColors((uint8_t)fuselage_guest_get_argument(0),
                                                               (uint16_t)fuselage_guest_get_argument(1))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_TILE_GET_COLLIDABLE:
        fuselage_guest_set_result(
            (uint32_t)GetTileLayerCollidableColors((uint8_t)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_TILE_TEST_PATTERN:
        fuselage_guest_set_result(UploadTileTestPattern((uint8_t)fuselage_guest_get_argument(0),
                                                        (int)fuselage_guest_get_argument(1))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_TILE_BOX_PATTERN:
        fuselage_guest_set_result(
            UploadTileBoxPattern((uint8_t)fuselage_guest_get_argument(0), (int)fuselage_guest_get_argument(1))
                ? 1u
                : 0u);
        return 1;
    case MISL_TILE_TOGGLE_ATLAS:
        ToggleTileAtlasView((uint8_t)fuselage_guest_get_argument(0));
        return 1;
    case MISL_TILE_GET_ATLAS_ACTIVE:
        fuselage_guest_set_result(GetTileAtlasViewActive((uint8_t)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_TILE_SET_PRIORITY:
        fuselage_guest_set_result(SetTileLayerPriority((uint8_t)fuselage_guest_get_argument(0),
                                                       (unsigned char)fuselage_guest_get_argument(1))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_TILE_GET_PRIORITY:
        fuselage_guest_set_result((uint32_t)GetTileLayerPriority((uint8_t)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_TILE_GET_TILE:
        fuselage_guest_set_result((uint32_t)GetTile((uint8_t)fuselage_guest_get_argument(0),
                                                    (uint16_t)fuselage_guest_get_argument(1),
                                                    (uint16_t)fuselage_guest_get_argument(2)));
        return 1;
    case MISL_TILE_GET_CELL_PALETTE:
        fuselage_guest_set_result((uint32_t)GetTileCellPalette((uint8_t)fuselage_guest_get_argument(0),
                                                               (uint16_t)fuselage_guest_get_argument(1),
                                                               (uint16_t)fuselage_guest_get_argument(2)));
        return 1;
    case MISL_TILE_GET_CELL_TRANSP:
        fuselage_guest_set_result((uint32_t)GetTileCellTransparency(
            (uint8_t)fuselage_guest_get_argument(0), (uint16_t)fuselage_guest_get_argument(1),
            (uint16_t)fuselage_guest_get_argument(2)));
        return 1;
    case MISL_TILE_GET_CELL_SHOWZERO:
        fuselage_guest_set_result(GetTileCellShowZero((uint8_t)fuselage_guest_get_argument(0),
                                                      (uint16_t)fuselage_guest_get_argument(1),
                                                      (uint16_t)fuselage_guest_get_argument(2))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_TILE_GET_CELL_COLLIS:
        fuselage_guest_set_result(GetTileCellCollision((uint8_t)fuselage_guest_get_argument(0),
                                                       (uint16_t)fuselage_guest_get_argument(1),
                                                       (uint16_t)fuselage_guest_get_argument(2))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_TILE_SET_CELL_TAG:
        fuselage_guest_set_result(
            SetTileCellTag((uint8_t)fuselage_guest_get_argument(0), (uint16_t)fuselage_guest_get_argument(1),
                           (uint16_t)fuselage_guest_get_argument(2), fuselage_guest_get_argument(3))
                ? 1u
                : 0u);
        return 1;
    case MISL_TILE_GET_CELL_TAG:
        fuselage_guest_set_result(GetTileCellTag((uint8_t)fuselage_guest_get_argument(0),
                                                 (uint16_t)fuselage_guest_get_argument(1),
                                                 (uint16_t)fuselage_guest_get_argument(2)));
        return 1;
    case MISL_TILE_GET_SCALE:
        fuselage_guest_set_float_result(GetTileLayerScale((uint8_t)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_TILE_GET_VIEWPORT: {
        float values[4];
        void* outputs[4];
        outputs[0] = fuselage_guest_resolve_output(fuselage_guest_get_argument(1), sizeof(float));
        outputs[1] = fuselage_guest_resolve_output(fuselage_guest_get_argument(2), sizeof(float));
        outputs[2] = fuselage_guest_resolve_output(fuselage_guest_get_argument(3), sizeof(float));
        outputs[3] = fuselage_guest_resolve_output(fuselage_guest_get_argument(4), sizeof(float));
        if (outputs[0] && outputs[1] && outputs[2] && outputs[3]) {
            for (unsigned index = 0; index < 4; index++) {
                memcpy(&values[index], outputs[index], sizeof values[index]);
            }
            GetTileViewport((uint8_t)fuselage_guest_get_argument(0), &values[0], &values[1], &values[2],
                            &values[3]);
            for (unsigned index = 0; index < 4; index++) {
                memcpy(outputs[index], &values[index], sizeof values[index]);
            }
        }
        return 1;
    }
    case MISL_TILE_GET_MAP_OFFSET: {
        double values[2];
        void* outputs[2];
        outputs[0] = fuselage_guest_resolve_output(fuselage_guest_get_argument(1), sizeof(double));
        outputs[1] = fuselage_guest_resolve_output(fuselage_guest_get_argument(2), sizeof(double));
        if (outputs[0] && outputs[1]) {
            for (unsigned index = 0; index < 2; index++) {
                memcpy(&values[index], outputs[index], sizeof values[index]);
            }
            GetTileMapOffset((uint8_t)fuselage_guest_get_argument(0), &values[0], &values[1]);
            for (unsigned index = 0; index < 2; index++) {
                memcpy(outputs[index], &values[index], sizeof values[index]);
            }
        }
        return 1;
    }
    case MISL_TILE_GET_CELL_OFFSET: {
        int8_t values[2];
        void* outputs[2];
        outputs[0] = fuselage_guest_resolve_output(fuselage_guest_get_argument(3), sizeof(int8_t));
        outputs[1] = fuselage_guest_resolve_output(fuselage_guest_get_argument(4), sizeof(int8_t));
        if (outputs[0] && outputs[1]) {
            for (unsigned index = 0; index < 2; index++) {
                memcpy(&values[index], outputs[index], sizeof values[index]);
            }
            GetTileCellOffset((uint8_t)fuselage_guest_get_argument(0),
                              (uint16_t)fuselage_guest_get_argument(1),
                              (uint16_t)fuselage_guest_get_argument(2), &values[0], &values[1]);
            for (unsigned index = 0; index < 2; index++) {
                memcpy(outputs[index], &values[index], sizeof values[index]);
            }
        }
        return 1;
    }
    default:
        return fuselage_guest_unknown_ecall(call);
    }
}
