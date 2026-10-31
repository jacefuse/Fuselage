/* Native pixies adapter. SEE: (MISL_HOST_GRAPHICS.md, Pixies) */
#include "../runtime/misl_host_arguments_internal.h"
#include "../runtime/misl_host_pointers_internal.h"
#include "../runtime/misl_host_services_internal.h"
#include "misl_host_graphics_internal.h"
#include "../../misl_abi.h"
#include "../../../GDMF/pixies/gdmf_pixies.h"
#include <stdlib.h>
#include <string.h>

int fuselage_guest_service_pixies(uint32_t call) {
    switch (call) {
    case MISL_PIX_INIT: {
        bool initialized =
            InitPixie((int)fuselage_guest_get_argument(0), (PixieMode)fuselage_guest_get_argument(1),
                      (int)fuselage_guest_get_argument(2), (int)fuselage_guest_get_argument(3));
        fuselage_guest_refresh_pixie_banks();
        fuselage_guest_set_result(initialized ? 1u : 0u);
        return 1;
    }
    case MISL_PIX_SHUTDOWN:
        fuselage_guest_set_result(ReleasePixie((int)fuselage_guest_get_argument(0)) ? 1u : 0u);
        fuselage_guest_refresh_pixie_banks();
        return 1;
    case MISL_PIX_COMMAND: {
        const void* input =
            fuselage_guest_resolve_input(fuselage_guest_get_argument(3), 4 * sizeof(uint32_t));
        uint32_t arguments[4];
        if (input) {
            memcpy(arguments, input, sizeof arguments);
        }
        fuselage_guest_set_result(input && IssuePixieCommand((int)fuselage_guest_get_argument(0),
                                                             (PixieOpcode)fuselage_guest_get_argument(1),
                                                             (uint16_t)fuselage_guest_get_argument(2),
                                                             arguments)
                                      ? 1u
                                      : 0u);
        return 1;
    }
    case MISL_PIX_WRITE: {
        const void* input_bytes =
            fuselage_guest_resolve_input(fuselage_guest_get_argument(2), fuselage_guest_get_argument(3));
        fuselage_guest_set_result(
            (input_bytes &&
             WritePixieRAM((int)fuselage_guest_get_argument(0), (size_t)fuselage_guest_get_argument(1),
                           input_bytes, (size_t)fuselage_guest_get_argument(3)))
                ? 1u
                : 0u);
        return 1;
    }
    case MISL_PIX_GET_RAM_SIZE:
        fuselage_guest_set_result((uint32_t)GetPixieRAMSize((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_PIX_READ_STRING: {
        char* output_string =
            fuselage_guest_resolve_output(fuselage_guest_get_argument(1), fuselage_guest_get_argument(2));
        fuselage_guest_set_result(
            output_string ? (uint32_t)ReadPixieString((int)fuselage_guest_get_argument(0), output_string,
                                                      (size_t)fuselage_guest_get_argument(2))
                          : 0u);
        return 1;
    }
    case MISL_PIX_SET_POSITION:
        fuselage_guest_set_result(SetPixiePosition((int)fuselage_guest_get_argument(0),
                                                   fuselage_guest_get_float_argument(1),
                                                   fuselage_guest_get_float_argument(2))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_PIX_SET_DISPLAY_SIZE:
        fuselage_guest_set_result(SetPixieDisplaySize((int)fuselage_guest_get_argument(0),
                                                      fuselage_guest_get_float_argument(1),
                                                      fuselage_guest_get_float_argument(2))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_PIX_SET_PRIORITY:
        fuselage_guest_set_result(SetPixiePriority((int)fuselage_guest_get_argument(0),
                                                   (unsigned char)fuselage_guest_get_argument(1))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_PIX_SET_ENABLED:
        fuselage_guest_set_result(
            SetPixieEnabled((int)fuselage_guest_get_argument(0), fuselage_guest_get_argument(1) != 0) ? 1u
                                                                                                      : 0u);
        return 1;
    case MISL_PIX_SHOW:
        fuselage_guest_set_result(ShowPixie((int)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_PIX_HIDE:
        fuselage_guest_set_result(HidePixie((int)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_PIX_SET_DRAW_PATTERN:
        fuselage_guest_set_result(
            SetPixieDrawPattern((int)fuselage_guest_get_argument(0), (uint16_t)fuselage_guest_get_argument(1))
                ? 1u
                : 0u);
        return 1;
    case MISL_PIX_GET_DRAW_PATTERN:
        fuselage_guest_set_result((uint32_t)GetPixieDrawPattern((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_PIX_SET_PATTERN_BITMAP:
        fuselage_guest_set_result(SetPixiePatternBitmap((int)fuselage_guest_get_argument(0),
                                                        (uint32_t)fuselage_guest_get_argument(1),
                                                        (uint16_t)fuselage_guest_get_argument(2),
                                                        (uint16_t)fuselage_guest_get_argument(3))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_PIX_CLEAR_PATTERN_BITMAP:
        fuselage_guest_set_result(ClearPixiePatternBitmap((int)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_PIX_GET_INITIALIZED:
        fuselage_guest_set_result(GetPixieInitialized((int)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_PIX_GET_MODE:
        fuselage_guest_set_result((uint32_t)GetPixieMode((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_PIX_GET_OUT_WIDTH:
        fuselage_guest_set_result((uint32_t)GetPixieOutputWidth((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_PIX_GET_OUT_HEIGHT:
        fuselage_guest_set_result((uint32_t)GetPixieOutputHeight((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_PIX_GET_X:
        fuselage_guest_set_float_result(GetPixieX((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_PIX_GET_Y:
        fuselage_guest_set_float_result(GetPixieY((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_PIX_GET_DISPLAY_WIDTH:
        fuselage_guest_set_float_result(GetPixieDisplayWidth((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_PIX_GET_DISPLAY_HEIGHT:
        fuselage_guest_set_float_result(GetPixieDisplayHeight((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_PIX_GET_PRIORITY:
        fuselage_guest_set_result((uint32_t)GetPixiePriority((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_PIX_GET_ENABLED:
        fuselage_guest_set_result(GetPixieEnabled((int)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_PIX_GET_SHOWN:
        fuselage_guest_set_result(GetPixieShown((int)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_PIX_SET_TRANSFORM:
        fuselage_guest_set_result(
            SetPixieTransform((int)fuselage_guest_get_argument(0), (int)fuselage_guest_get_argument(1),
                              fuselage_guest_get_float_argument(2), fuselage_guest_get_float_argument(3),
                              fuselage_guest_get_float_argument(4), fuselage_guest_get_float_argument(5),
                              fuselage_guest_get_float_argument(6))
                ? 1u
                : 0u);
        return 1;
    case MISL_PIX_SET_TRANSFORM_M: {
        const void* input =
            fuselage_guest_resolve_input(fuselage_guest_get_argument(2), sizeof(PixieTransform));
        PixieTransform matrix;
        if (input) {
            memcpy(&matrix, input, sizeof matrix);
        }
        fuselage_guest_set_result(input && SetPixieTransformMatrix((int)fuselage_guest_get_argument(0),
                                                                   (int)fuselage_guest_get_argument(1),
                                                                   &matrix)
                                      ? 1u
                                      : 0u);
        return 1;
    }
    case MISL_PIX_GET_TRANSFORM_M: {
        void* output = fuselage_guest_resolve_output(fuselage_guest_get_argument(2), sizeof(PixieTransform));
        PixieTransform matrix;
        int accepted = output && GetPixieTransformMatrix((int)fuselage_guest_get_argument(0),
                                                         (int)fuselage_guest_get_argument(1), &matrix);
        if (accepted) {
            memcpy(output, &matrix, sizeof matrix);
        }
        fuselage_guest_set_result(accepted ? 1u : 0u);
        return 1;
    }
    case MISL_PIX_RESET_TRANSFORM:
        fuselage_guest_set_result(
            ResetPixieTransform((int)fuselage_guest_get_argument(0), (int)fuselage_guest_get_argument(1))
                ? 1u
                : 0u);
        return 1;
    case MISL_PIX_SET_CUR_TRANSFORM:
        fuselage_guest_set_result(
            SetPixieCurrentTransform((int)fuselage_guest_get_argument(0), (int)fuselage_guest_get_argument(1))
                ? 1u
                : 0u);
        return 1;
    case MISL_PIX_SET_MASK_INVERT:
        fuselage_guest_set_result(
            SetPixieMaskInvert((int)fuselage_guest_get_argument(0), fuselage_guest_get_argument(1) != 0)
                ? 1u
                : 0u);
        return 1;
    case MISL_PIX_GET_MASK_INVERT:
        fuselage_guest_set_result(GetPixieMaskInvert((int)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_PIX_SET_ROTATION:
        fuselage_guest_set_result(
            SetPixieRotation((int)fuselage_guest_get_argument(0), fuselage_guest_get_float_argument(1)) ? 1u
                                                                                                        : 0u);
        return 1;
    case MISL_PIX_GET_ROTATION:
        fuselage_guest_set_float_result(GetPixieRotation((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_PIX_SET_HOTSPOT:
        fuselage_guest_set_result(SetPixieHotspot((int)fuselage_guest_get_argument(0),
                                                  fuselage_guest_get_float_argument(1),
                                                  fuselage_guest_get_float_argument(2))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_PIX_GET_HOTSPOT_X:
        fuselage_guest_set_float_result(GetPixieHotspotX((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_PIX_GET_HOTSPOT_Y:
        fuselage_guest_set_float_result(GetPixieHotspotY((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_PIX_SET_ROT_HOTSPOT:
        fuselage_guest_set_result(SetPixieRotateAroundHotspot((int)fuselage_guest_get_argument(0),
                                                              fuselage_guest_get_argument(1) != 0)
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_PIX_GET_ROT_HOTSPOT:
        fuselage_guest_set_result(GetPixieRotateAroundHotspot((int)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_PIX_SET_FILTER_LINEAR:
        fuselage_guest_set_result(
            SetPixieFilterLinear((int)fuselage_guest_get_argument(0), fuselage_guest_get_argument(1) != 0)
                ? 1u
                : 0u);
        return 1;
    case MISL_PIX_GET_FILTER_LINEAR:
        fuselage_guest_set_result(GetPixieFilterLinear((int)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_PIX_SUBMIT: {
        uint32_t count = fuselage_guest_get_argument(0);
        if (count > MISL_PIXIE_LIST_MAX) {
            count = MISL_PIXIE_LIST_MAX;
        }
        PixieCommand* commands =
            count ? fuselage_guest_copy_records(MISL_PIXIE_LIST_ADDR, count, sizeof(PixieCommand)) : NULL;
        PixieListResult result = {.firstRejectIndex = PIXIE_LIST_NO_REJECT};
        PixieCommand empty_command = {0};
        uint32_t accepted =
            !count || commands ? RunPixieCommandList(count ? commands : &empty_command, count, &result) : 0;
        free(commands);
        MislStatus* status = fuselage_guest_get_window_pointer(MISL_STATUS_ADDR, sizeof(MislStatus));
        status->pixieList.executed = result.executed;
        status->pixieList.rejected = result.rejected;
        status->pixieList.firstRejectIndex = result.firstRejectIndex;
        status->pixieList.firstRejectOpcode = result.firstRejectOpcode;
        status->pixieList.firstRejectPixie = result.firstRejectPixie;
        status->pixieList.submits++;
        status->pixieList.totalExecuted += result.executed;
        status->pixieList.totalRejected += result.rejected;
        fuselage_guest_set_result(accepted);
        return 1;
    }
    default:
        return fuselage_guest_unknown_ecall(call);
    }
}
