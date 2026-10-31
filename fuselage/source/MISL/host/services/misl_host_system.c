/* Native system-call adapter. SEE: (MISL_HOST.md, System services) */
#include "../runtime/misl_host_arguments_internal.h"
#include "../runtime/misl_host_color_internal.h"
#include "../runtime/misl_host_pointers_internal.h"
#include "../runtime/misl_host_services_internal.h"
#include "../../misl_abi.h"
#include "../../../BASE/fuselage_runtime.h"
#include "../../../GDMF/gdmf.h"
#include "../../../DICE/dice.h"
#include <stdio.h>
#include <string.h>

static int fuselage_guest_capture_name_valid(const char* name) {
    size_t length = strlen(name);
    if (!length || length > 64) {
        return 0;
    }
    char folded[65];
    for (size_t index = 0; index < length; index++) {
        unsigned char character = (unsigned char)name[index];
        if (!((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
              (character >= '0' && character <= '9') || character == '_' || character == '-')) {
            return 0;
        }
        folded[index] =
            character >= 'A' && character <= 'Z' ? (char)(character + ('a' - 'A')) : (char)character;
    }
    folded[length] = '\0';
    if (!strcmp(folded, "con") || !strcmp(folded, "prn") || !strcmp(folded, "aux") ||
        !strcmp(folded, "nul")) {
        return 0;
    }
    if (length == 4 && (!memcmp(folded, "com", 3) || !memcmp(folded, "lpt", 3)) && folded[3] >= '1' &&
        folded[3] <= '9') {
        return 0;
    }
    return 1;
}

static uint32_t fuselage_guest_request_capture(void) {
    if (!fuselage_guest_session.capture_directory[0]) {
        return 0;
    }
    char name[128];
    MislHostStringRead read = fuselage_guest_copy_string(fuselage_guest_get_argument(0), name, sizeof name);
    if (read.status != MISL_HOST_STRING_COMPLETE || !fuselage_guest_capture_name_valid(name)) {
        return 0;
    }
    char path[512];
    int length = snprintf(path, sizeof path, "%s/%s.ppm", fuselage_guest_session.capture_directory, name);
    if (length <= 0 || (size_t)length >= sizeof path) {
        return 0;
    }
    return GDMF_CaptureFrame(path) ? 1u : 0u;
}

int fuselage_guest_service_system(uint32_t call) {
    switch (call) {
    case MISL_SYS_SIGNAL:
        FuselageSignal((FuselageSignalType)fuselage_guest_get_argument(0));
        break;
    case MISL_SYS_WAITVBL:
    case MISL_SYS_YIELD:
        return 0;
    case MISL_SYS_SLEEP:
        fuselage_guest_session.sleep_ticks = fuselage_guest_get_argument(0);
        return 0;
    case MISL_SYS_GET_FPS:
        fuselage_guest_set_float_result(FuselageGetCurrentFPS());
        break;
    case MISL_SYS_SET_MOUSECAPTURE:
        FuselageSetMouseCapture(fuselage_guest_get_argument(0) != 0);
        break;
    case MISL_SYS_GET_MOUSECAPTURE:
        fuselage_guest_set_result(FuselageGetMouseCapture() ? 1u : 0u);
        break;
    case MISL_SYS_TOGGLE_MOUSECAPTURE:
        fuselage_guest_set_result(FuselageToggleMouseCapture() ? 1u : 0u);
        break;
    case MISL_SYS_SET_CURSORVISIBLE:
        FuselageSetCursorVisible(fuselage_guest_get_argument(0) != 0);
        break;
    case MISL_SYS_GET_CURSORVISIBLE:
        fuselage_guest_set_result(FuselageGetCursorVisible() ? 1u : 0u);
        break;
    case MISL_SYS_SET_VSYNC:
        FuselageSetVSync(fuselage_guest_get_argument(0) != 0);
        break;
    case MISL_SYS_GET_VSYNC:
        fuselage_guest_set_result(FuselageGetVSync() ? 1u : 0u);
        break;
    case MISL_SYS_SET_TARGETFPS:
        FuselageSetTargetFPS(fuselage_guest_get_double_argument(0));
        break;
    case MISL_SYS_GET_TARGETFPS:
        fuselage_guest_set_double_result(FuselageGetTargetFPS());
        break;
    case MISL_SYS_GET_SIMRATE:
        fuselage_guest_set_double_result(FuselageGetSimRate());
        break;
    case MISL_SYS_SET_CLEARCOLOR:
        GDMF_SetClearColor(fuselage_guest_get_color_argument(0));
        break;
    case MISL_SYS_DESCRIPTOR:
        fuselage_guest_set_result(MISL_DESC_ADDR);
        break;
    case MISL_SYS_GET_CANVAS_W:
        fuselage_guest_set_result((uint32_t)GDMF_GetCanvasWidth());
        break;
    case MISL_SYS_GET_CANVAS_H:
        fuselage_guest_set_result((uint32_t)GDMF_GetCanvasHeight());
        break;
    case MISL_SYS_GET_CLEARCOLOR:
        fuselage_guest_set_color_result(GDMF_GetClearColor());
        break;
    case MISL_SYS_ELAPSED_US:
        fuselage_guest_set_wide_result(DICE_GetElapsedMicroseconds());
        break;
    case MISL_SYS_CAPTURE:
        fuselage_guest_set_result(fuselage_guest_request_capture());
        break;
    case MISL_SYS_CAPTURE_PENDING:
        fuselage_guest_set_result(GDMF_CapturePending() ? 1u : 0u);
        break;
    case MISL_SYS_IS_FOCUSED:
        fuselage_guest_set_result(GDMF_IsFocused() ? 1u : 0u);
        break;
    case MISL_SYS_ABI_DECLARE:
        return fuselage_guest_declare_abi();
    default:
        return fuselage_guest_unknown_ecall(call);
    }
    return 1;
}
