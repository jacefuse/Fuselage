/* Native text adapter. SEE: (MISL_HOST_GRAPHICS.md, Text) */
#include "../runtime/misl_host_arguments_internal.h"
#include "../runtime/misl_host_color_internal.h"
#include "../runtime/misl_host_pointers_internal.h"
#include "../runtime/misl_host_services_internal.h"
#include "../../misl_abi.h"
#include "../../../GDMF/textlayer/gdmf_textlayer.h"

int fuselage_guest_service_text(uint32_t call) {
    char text_buffer[512];
    switch (call) {
    case MISL_TL_PRINT:
        fuselage_guest_copy_string(fuselage_guest_get_argument(0), text_buffer, sizeof text_buffer);
        fuselage_guest_set_result((uint32_t)tlPrint(text_buffer));
        return 1;
    case MISL_TL_PRINT_C:
        fuselage_guest_copy_string(fuselage_guest_get_argument(0), text_buffer, sizeof text_buffer);
        fuselage_guest_set_result((uint32_t)tlPrintC(text_buffer, fuselage_guest_get_color_argument(1)));
        return 1;
    case MISL_TL_PRINT_CP:
        fuselage_guest_copy_string(fuselage_guest_get_argument(0), text_buffer, sizeof text_buffer);
        fuselage_guest_set_result((uint32_t)tlPrintCP(text_buffer, fuselage_guest_get_color_argument(1),
                                                      (int)fuselage_guest_get_argument(2),
                                                      (int)fuselage_guest_get_argument(3)));
        return 1;
    case MISL_TL_PRINTCHAR:
        fuselage_guest_set_result((uint32_t)tlPrintChar((unsigned char)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_TL_PRINTCHAR_C:
        fuselage_guest_set_result((uint32_t)tlPrintCharC((unsigned char)fuselage_guest_get_argument(0),
                                                         fuselage_guest_get_color_argument(1)));
        return 1;
    case MISL_TL_PRINTCHAR_CP:
        fuselage_guest_set_result((uint32_t)tlPrintCharCP(
            (unsigned char)fuselage_guest_get_argument(0), fuselage_guest_get_color_argument(1),
            (int)fuselage_guest_get_argument(2), (int)fuselage_guest_get_argument(3)));
        return 1;
    case MISL_TL_PRINTINT:
        fuselage_guest_set_result((uint32_t)tlPrintInt((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_TL_PRINTINT_C:
        fuselage_guest_set_result(
            (uint32_t)tlPrintIntC((int)fuselage_guest_get_argument(0), fuselage_guest_get_color_argument(1)));
        return 1;
    case MISL_TL_PRINTINT_CP:
        fuselage_guest_set_result(
            (uint32_t)tlPrintIntCP((int)fuselage_guest_get_argument(0), fuselage_guest_get_color_argument(1),
                                   (int)fuselage_guest_get_argument(2), (int)fuselage_guest_get_argument(3)));
        return 1;
    case MISL_TL_NEWLINE:
        fuselage_guest_set_result((uint32_t)tlNewLine());
        return 1;
    case MISL_TL_CLS:
        tlCLS();
        return 1;
    case MISL_TL_HOME:
        tlHome();
        return 1;
    case MISL_TL_SET_COLOR:
        tlSetColor(fuselage_guest_get_color_argument(0));
        return 1;
    case MISL_TL_GET_COLOR:
        fuselage_guest_set_color_result(tlGetColor());
        return 1;
    case MISL_TL_SET_CURSOR:
        tlSetCursor((int)fuselage_guest_get_argument(0), (int)fuselage_guest_get_argument(1));
        return 1;
    case MISL_TL_GET_CURSOR:
        fuselage_guest_set_result((uint32_t)tlGetCursor());
        return 1;
    case MISL_TL_GET_CURSOR_X:
        fuselage_guest_set_result((uint32_t)tlGetCursorX());
        return 1;
    case MISL_TL_GET_CURSOR_Y:
        fuselage_guest_set_result((uint32_t)tlGetCursorY());
        return 1;
    case MISL_TL_SCROLL_UP:
        fuselage_guest_set_result((uint32_t)tlScrollUp());
        return 1;
    case MISL_TL_ACTIVE:
        fuselage_guest_set_result(tlActivate() ? 1u : 0u);
        return 1;
    case MISL_TL_INACTIVE:
        fuselage_guest_set_result(tlDeactivate() ? 1u : 0u);
        return 1;
    case MISL_TL_TOGGLE:
        fuselage_guest_set_result(tlToggle() ? 1u : 0u);
        return 1;
    case MISL_TL_STATUS:
        fuselage_guest_set_result(tlStatus() ? 1u : 0u);
        return 1;
    case MISL_TL_GET_CELL_CHAR:
        fuselage_guest_set_result((uint32_t)tlGetCellCharacter((int)fuselage_guest_get_argument(0),
                                                               (int)fuselage_guest_get_argument(1)));
        return 1;
    case MISL_TL_GET_CELL_COLOR:
        fuselage_guest_set_color_result(
            tlGetCellColor((int)fuselage_guest_get_argument(0), (int)fuselage_guest_get_argument(1)));
        return 1;
    default:
        return fuselage_guest_unknown_ecall(call);
    }
}
