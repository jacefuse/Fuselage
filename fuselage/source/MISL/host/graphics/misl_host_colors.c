/* Native colors adapter. SEE: (MISL_HOST_GRAPHICS.md, Colors) */
#include "../runtime/misl_host_arguments_internal.h"
#include "../runtime/misl_host_color_internal.h"
#include "../runtime/misl_host_pointers_internal.h"
#include "../runtime/misl_host_services_internal.h"
#include "../../misl_abi.h"
#include <string.h>

int fuselage_guest_service_colors(uint32_t call) {
    switch (call) {
    case MISL_PAL_PACK_RGBA8:
        fuselage_guest_set_result(PackColorsRGBA8(fuselage_guest_get_color_argument(0)));
        return 1;
    case MISL_PAL_LOAD_FROM_SPRITE: {
        Color palette[FUSELAGE_PALETTE_SIZE];
        const void* input = fuselage_guest_resolve_input(fuselage_guest_get_argument(1), sizeof palette);
        if (input) {
            memcpy(palette, input, sizeof palette);
        }
        fuselage_guest_set_result(
            input && SetColorsPalette((unsigned char)fuselage_guest_get_argument(0), palette) ? 1u : 0u);
        return 1;
    }
    case MISL_PAL_GET_FROM_CHAR:
        fuselage_guest_set_color_result(
            GetColorsColorFromChar((unsigned char)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_PAL_GET_COMMODORE:
        fuselage_guest_set_color_result(
            GetColorsCommodoreColor((unsigned char)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_PAL_GET_TANDY:
        fuselage_guest_set_color_result(GetColorsTandyColor((unsigned char)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_PAL_GET_ANSI:
        fuselage_guest_set_color_result(GetColorsANSIColor((unsigned char)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_PAL_SET:
        fuselage_guest_set_result(SetColorsPaletteColor((unsigned char)fuselage_guest_get_argument(0),
                                                        (unsigned char)fuselage_guest_get_argument(1),
                                                        fuselage_guest_get_color_argument(2))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_PAL_GET:
        fuselage_guest_set_color_result(GetColorsPaletteColor((unsigned char)fuselage_guest_get_argument(0),
                                                              (unsigned char)fuselage_guest_get_argument(1)));
        return 1;
    case MISL_PAL_GET_GENERATION:
        fuselage_guest_set_result(GetColorsGeneration());
        return 1;
    case MISL_PAL_DEEP_SET_ALL: {
        Color palette[FUSELAGE_DEEP_PALETTE_SIZE];
        const void* input = fuselage_guest_resolve_input(fuselage_guest_get_argument(1), sizeof palette);
        if (input) {
            memcpy(palette, input, sizeof palette);
        }
        fuselage_guest_set_result(
            input && SetColorsDeepPalette((unsigned char)fuselage_guest_get_argument(0), palette) ? 1u : 0u);
        return 1;
    }
    case MISL_PAL_DEEP_SET:
        fuselage_guest_set_result(SetColorsDeepPaletteColor((unsigned char)fuselage_guest_get_argument(0),
                                                            (unsigned char)fuselage_guest_get_argument(1),
                                                            fuselage_guest_get_color_argument(2))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_PAL_DEEP_GET:
        fuselage_guest_set_color_result(GetColorsDeepPaletteColor(
            (unsigned char)fuselage_guest_get_argument(0), (unsigned char)fuselage_guest_get_argument(1)));
        return 1;
    case MISL_PAL_DEEP_GENERATION:
        fuselage_guest_set_result(GetColorsDeepGeneration());
        return 1;
    default:
        return fuselage_guest_unknown_ecall(call);
    }
}
