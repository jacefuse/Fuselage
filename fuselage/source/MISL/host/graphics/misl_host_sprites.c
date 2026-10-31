/* Native sprites adapter. SEE: (MISL_HOST_GRAPHICS.md, Sprites) */
#include "../runtime/misl_host_arguments_internal.h"
#include "../runtime/misl_host_pointers_internal.h"
#include "../runtime/misl_host_services_internal.h"
#include "misl_host_graphics_internal.h"
#include "../../misl_abi.h"
#include "../../../GDMF/sprites/gdmf_sprites.h"
#include "../../../GDMF/sprites/gdmf_sprites_help.h"
#include <stdlib.h>
#include <string.h>

int fuselage_guest_service_sprites(uint32_t call) {
    switch (call) {
    case MISL_SPR_UPLOAD_BITMAP: {
        const unsigned char* bitmap =
            fuselage_guest_resolve_input(fuselage_guest_get_argument(1), MISL_SPRITE_STRIDE);
        fuselage_guest_set_result(
            (bitmap && UploadSpriteBitmap((SpriteBitmapID)fuselage_guest_get_argument(0), bitmap)) ? 1u : 0u);
        return 1;
    }
    case MISL_SPR_ASSIGN:
        fuselage_guest_set_result(
            AssignSprite((int)fuselage_guest_get_argument(0), (SpriteBitmapID)fuselage_guest_get_argument(1))
                ? 1u
                : 0u);
        return 1;
    case MISL_SPR_ASSIGN_FROM:
        fuselage_guest_set_result(AssignSpriteBitmapFromSprite((int)fuselage_guest_get_argument(0),
                                                               (int)fuselage_guest_get_argument(1))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_SPR_GET_BITMAP_ID:
        fuselage_guest_set_result((uint32_t)(int)GetSpriteBitmapID((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_SPR_CLEAR:
        fuselage_guest_set_result(ClearSprite((int)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_SPR_TEST_PATTERN:
        fuselage_guest_set_result(AssignSpriteTestPattern((int)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_SPR_GET_ENABLED:
        fuselage_guest_set_result(GetSpriteEnabled((int)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_SPR_SET_ENABLED:
        fuselage_guest_set_result(
            SetSpriteEnabled((int)fuselage_guest_get_argument(0), fuselage_guest_get_argument(1) != 0) ? 1u
                                                                                                       : 0u);
        return 1;
    case MISL_SPR_TOGGLE_ENABLED:
        fuselage_guest_set_result(ToggleSpriteEnabled((int)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_SPR_GET_VISIBLE:
        fuselage_guest_set_result(GetSpriteVisible((int)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_SPR_SET_VISIBLE:
        fuselage_guest_set_result(
            SetSpriteVisible((int)fuselage_guest_get_argument(0), fuselage_guest_get_argument(1) != 0) ? 1u
                                                                                                       : 0u);
        return 1;
    case MISL_SPR_TOGGLE_VISIBLE:
        fuselage_guest_set_result(ToggleSpriteVisible((int)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_SPR_SET_POSITION:
        fuselage_guest_set_result(SetSpritePosition((int)fuselage_guest_get_argument(0),
                                                    fuselage_guest_get_float_argument(1),
                                                    fuselage_guest_get_float_argument(2))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_SPR_OFFSET_POSITION:
        fuselage_guest_set_result(OffsetSpritePosition((int)fuselage_guest_get_argument(0),
                                                       fuselage_guest_get_float_argument(1),
                                                       fuselage_guest_get_float_argument(2))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_SPR_GET_X:
        fuselage_guest_set_float_result(GetSpriteX((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_SPR_GET_Y:
        fuselage_guest_set_float_result(GetSpriteY((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_SPR_SET_SCALE:
        fuselage_guest_set_result(
            SetSpriteScale((int)fuselage_guest_get_argument(0), fuselage_guest_get_float_argument(1)) ? 1u
                                                                                                      : 0u);
        return 1;
    case MISL_SPR_GET_SCALE:
        fuselage_guest_set_float_result(GetSpriteScale((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_SPR_OFFSET_SCALE:
        fuselage_guest_set_result(
            OffsetSpriteScale((int)fuselage_guest_get_argument(0), fuselage_guest_get_float_argument(1))
                ? 1u
                : 0u);
        return 1;
    case MISL_SPR_SET_ROTATION:
        fuselage_guest_set_result(
            SetSpriteRotation((int)fuselage_guest_get_argument(0), fuselage_guest_get_float_argument(1))
                ? 1u
                : 0u);
        return 1;
    case MISL_SPR_GET_ROTATION:
        fuselage_guest_set_float_result(GetSpriteRotation((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_SPR_OFFSET_ROTATION:
        fuselage_guest_set_result(
            OffsetSpriteRotation((int)fuselage_guest_get_argument(0), fuselage_guest_get_float_argument(1))
                ? 1u
                : 0u);
        return 1;
    case MISL_SPR_SET_SKEW_X:
        fuselage_guest_set_result(
            SetSpriteSkewX((int)fuselage_guest_get_argument(0), fuselage_guest_get_float_argument(1)) ? 1u
                                                                                                      : 0u);
        return 1;
    case MISL_SPR_SET_SKEW_Y:
        fuselage_guest_set_result(
            SetSpriteSkewY((int)fuselage_guest_get_argument(0), fuselage_guest_get_float_argument(1)) ? 1u
                                                                                                      : 0u);
        return 1;
    case MISL_SPR_GET_SKEW_X:
        fuselage_guest_set_float_result(GetSpriteSkewX((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_SPR_GET_SKEW_Y:
        fuselage_guest_set_float_result(GetSpriteSkewY((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_SPR_SET_HOTSPOT:
        fuselage_guest_set_result(SetSpriteHotspot((int)fuselage_guest_get_argument(0),
                                                   fuselage_guest_get_float_argument(1),
                                                   fuselage_guest_get_float_argument(2))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_SPR_SET_HOTSPOT_X:
        fuselage_guest_set_result(
            SetSpriteHotspotX((int)fuselage_guest_get_argument(0), fuselage_guest_get_float_argument(1))
                ? 1u
                : 0u);
        return 1;
    case MISL_SPR_SET_HOTSPOT_Y:
        fuselage_guest_set_result(
            SetSpriteHotspotY((int)fuselage_guest_get_argument(0), fuselage_guest_get_float_argument(1))
                ? 1u
                : 0u);
        return 1;
    case MISL_SPR_GET_HOTSPOT_X:
        fuselage_guest_set_float_result(GetSpriteHotspotX((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_SPR_GET_HOTSPOT_Y:
        fuselage_guest_set_float_result(GetSpriteHotspotY((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_SPR_SET_ROT_HOTSPOT:
        fuselage_guest_set_result(SetSpriteRotateAroundHotspot((int)fuselage_guest_get_argument(0),
                                                               fuselage_guest_get_argument(1) != 0)
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_SPR_GET_ROT_HOTSPOT:
        fuselage_guest_set_result(GetSpriteRotateAroundHotspot((int)fuselage_guest_get_argument(0)) ? 1u
                                                                                                    : 0u);
        return 1;
    case MISL_SPR_SET_SCALE_X:
        fuselage_guest_set_result(
            SetSpriteScaleX((int)fuselage_guest_get_argument(0), fuselage_guest_get_float_argument(1)) ? 1u
                                                                                                       : 0u);
        return 1;
    case MISL_SPR_SET_SCALE_Y:
        fuselage_guest_set_result(
            SetSpriteScaleY((int)fuselage_guest_get_argument(0), fuselage_guest_get_float_argument(1)) ? 1u
                                                                                                       : 0u);
        return 1;
    case MISL_SPR_GET_SCALE_X:
        fuselage_guest_set_float_result(GetSpriteScaleX((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_SPR_GET_SCALE_Y:
        fuselage_guest_set_float_result(GetSpriteScaleY((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_SPR_SET_FLIP_X:
        fuselage_guest_set_result(
            SetSpriteFlipX((int)fuselage_guest_get_argument(0), fuselage_guest_get_argument(1) != 0) ? 1u
                                                                                                     : 0u);
        return 1;
    case MISL_SPR_SET_FLIP_Y:
        fuselage_guest_set_result(
            SetSpriteFlipY((int)fuselage_guest_get_argument(0), fuselage_guest_get_argument(1) != 0) ? 1u
                                                                                                     : 0u);
        return 1;
    case MISL_SPR_GET_FLIP_X:
        fuselage_guest_set_result(GetSpriteFlipX((int)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_SPR_GET_FLIP_Y:
        fuselage_guest_set_result(GetSpriteFlipY((int)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_SPR_SET_PRIORITY:
        fuselage_guest_set_result(SetSpritePriority((int)fuselage_guest_get_argument(0),
                                                    (unsigned char)fuselage_guest_get_argument(1))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_SPR_GET_PRIORITY:
        fuselage_guest_set_result((uint32_t)GetSpritePriority((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_SPR_SET_PALETTE:
        fuselage_guest_set_result(SetSpriteColorPalette((int)fuselage_guest_get_argument(0),
                                                        (unsigned char)fuselage_guest_get_argument(1))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_SPR_GET_PALETTE:
        fuselage_guest_set_result((uint32_t)GetSpriteColorPalette((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_SPR_SET_TRANSPARENCY:
        fuselage_guest_set_result(SetSpriteTransparency((int)fuselage_guest_get_argument(0),
                                                        (unsigned char)fuselage_guest_get_argument(1))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_SPR_GET_TRANSPARENCY:
        fuselage_guest_set_result((uint32_t)GetSpriteTransparency((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_SPR_SHOW_ZERO:
        fuselage_guest_set_result(
            SetSpriteShowZero((int)fuselage_guest_get_argument(0), fuselage_guest_get_argument(1) != 0) ? 1u
                                                                                                        : 0u);
        return 1;
    case MISL_SPR_GET_SHOWZERO:
        fuselage_guest_set_result(GetSpriteShowZero((int)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_SPR_SET_COLLIDABLE:
        fuselage_guest_set_result((uint32_t)SetSpriteCollidableColors(
            (int)fuselage_guest_get_argument(0), (unsigned short)fuselage_guest_get_argument(1)));
        return 1;
    case MISL_SPR_GET_COLLIDABLE:
        fuselage_guest_set_result((uint32_t)GetSpriteCollidableColors((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_SPR_SET_COLLTYPES:
        fuselage_guest_set_result((uint32_t)SetSpriteCollisionTypes(
            (int)fuselage_guest_get_argument(0), (unsigned char)fuselage_guest_get_argument(1)));
        return 1;
    case MISL_SPR_GET_COLLTYPES:
        fuselage_guest_set_result((uint32_t)GetSpriteCollisionTypes((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_SPR_HAS_COLLISION:
        fuselage_guest_set_result(HasSpriteCollision((int)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_SPR_GET_COLL_COUNT:
        fuselage_guest_set_result((uint32_t)GetSpriteCollisionCount((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_SPR_COPY_COLLISIONS: {
        int collision_count = 0;
        const SpriteCollisionInfo* collisions =
            GetSpriteCollisions((int)fuselage_guest_get_argument(0), &collision_count);
        uint32_t count = fuselage_guest_get_argument(2);
        if (!collisions || collision_count <= 0 || collision_count > MAX_COLLISIONS_PER_SPRITE) {
            fuselage_guest_set_result(0);
            return 1;
        }
        if (count > (uint32_t)collision_count) {
            count = (uint32_t)collision_count;
        }
        void* output = fuselage_guest_resolve_record_output(fuselage_guest_get_argument(1), count,
                                                            sizeof(SpriteCollisionInfo));
        if (output) {
            memcpy(output, collisions, (size_t)count * sizeof(SpriteCollisionInfo));
        }
        fuselage_guest_set_result(output ? count : 0);
        return 1;
    }
    case MISL_SPR_CHECK_PAIR:
        fuselage_guest_set_result((uint32_t)CheckSpritePairCollision(
            (int)fuselage_guest_get_argument(0), (int)fuselage_guest_get_argument(1),
            (unsigned char)fuselage_guest_get_argument(2)));
        return 1;
    case MISL_SPR_CHECK_PAIR_BATCH: {
        uint32_t count = fuselage_guest_get_argument(1);
        if (!count || count > MISL_HOST_BATCH_MAX_ITEMS) {
            fuselage_guest_set_result(0);
            return 1;
        }
        uint32_t input_bytes = count * (uint32_t)sizeof(SpritePairRef);
        unsigned char* results = fuselage_guest_resolve_batch_output(fuselage_guest_get_argument(3), count);
        if (!results || !fuselage_guest_batch_ranges_valid(fuselage_guest_get_argument(0), input_bytes,
                                                           fuselage_guest_get_argument(3), count)) {
            fuselage_guest_set_result(0);
            return 1;
        }
        SpritePairRef* pairs =
            fuselage_guest_copy_records(fuselage_guest_get_argument(0), count, sizeof(SpritePairRef));
        int matches = pairs ? CheckSpritePairCollisionBatch(
                                  pairs, (int)count, (unsigned char)fuselage_guest_get_argument(2), results)
                            : 0;
        free(pairs);
        fuselage_guest_set_result((uint32_t)matches);
        return 1;
    }
    case MISL_SPR_FIND_PAIRS: {
        uint32_t count = fuselage_guest_get_argument(1), capacity = fuselage_guest_get_argument(4);
        if (!count) {
            fuselage_guest_set_result(0);
            return 1;
        }
        if (count > MISL_HOST_BATCH_MAX_ITEMS || capacity > MISL_HOST_BATCH_MAX_ITEMS ||
            (fuselage_guest_get_argument(0) & 1u) || (fuselage_guest_get_argument(3) & 1u)) {
            fuselage_guest_set_result(UINT32_MAX);
            return 1;
        }
        uint8_t* pair_output = capacity && fuselage_guest_get_argument(3)
                                   ? fuselage_guest_resolve_record_output(fuselage_guest_get_argument(3),
                                                                          capacity, sizeof(SpritePairRef))
                                   : NULL;
        uint8_t* type_output = capacity && fuselage_guest_get_argument(5)
                                   ? fuselage_guest_resolve_output(fuselage_guest_get_argument(5), capacity)
                                   : NULL;
        if ((capacity && fuselage_guest_get_argument(3) && !pair_output) ||
            (capacity && fuselage_guest_get_argument(5) && !type_output)) {
            fuselage_guest_set_result(UINT32_MAX);
            return 1;
        }
        short* indices = fuselage_guest_copy_records(fuselage_guest_get_argument(0), count, sizeof(short));
        SpritePairRef* pairs = pair_output ? malloc((size_t)capacity * sizeof *pairs) : NULL;
        uint8_t* types = pair_output && type_output ? malloc(capacity) : NULL;
        if (!indices || (pair_output && !pairs) || (pair_output && type_output && !types)) {
            free(indices);
            free(pairs);
            free(types);
            fuselage_guest_set_result(UINT32_MAX);
            return 1;
        }
        int matches =
            FindSpriteCollisionPairs(indices, (int)count, (unsigned char)fuselage_guest_get_argument(2),
                                     pairs, pair_output ? (int)capacity : 0, types ? types : type_output);
        uint32_t written = matches > 0 ? (uint32_t)matches : 0;
        if (written > capacity) {
            written = capacity;
        }
        if (pair_output) {
            for (uint32_t index = 0; index < written; index++) {
                memcpy(pair_output + index * sizeof *pairs, &pairs[index], sizeof *pairs);
                if (type_output) {
                    type_output[index] = types[index];
                }
            }
        }
        free(indices);
        free(pairs);
        free(types);
        fuselage_guest_set_result((uint32_t)matches);
        return 1;
    }
    case MISL_SPR_WORLD_POINT_ON:
        fuselage_guest_set_result(IsSpriteWorldPointInside((int)fuselage_guest_get_argument(0),
                                                           fuselage_guest_get_float_argument(1),
                                                           fuselage_guest_get_float_argument(2))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_SPR_TOGGLE_ATLAS:
        ToggleSpriteAtlasView();
        return 1;
    case MISL_SPR_GET_ATLAS_ACTIVE:
        fuselage_guest_set_result(GetSpriteAtlasViewActive() ? 1u : 0u);
        return 1;
    case MISL_SPR_GET_RENDERED_COUNT:
        fuselage_guest_set_result((uint32_t)GetSpriteRenderedCount());
        return 1;
    default:
        return fuselage_guest_unknown_ecall(call);
    }
}
