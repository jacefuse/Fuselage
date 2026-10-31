/* Native input adapter. SEE: (MISL_HOST_SERVICES.md, Input) */
#include "../runtime/misl_host_arguments_internal.h"
#include "../runtime/misl_host_pointers_internal.h"
#include "../runtime/misl_host_services_internal.h"
#include "../../misl_abi.h"
#include "../../../CAKE/cake.h"
int fuselage_guest_service_input(uint32_t call) {
    switch (call) {
    case MISL_CAKE_GET_KEY: {
        uint32_t key_code = fuselage_guest_get_argument(0);
        fuselage_guest_set_result((key_code < CAKE_KEY_TABLE_SIZE && CAKE_Keys[key_code]) ? 1u : 0u);
        return 1;
    }
    case MISL_CAKE_GET_MOUSE_X:
        fuselage_guest_set_result((uint32_t)CAKE_MouseDeltaX);
        return 1;
    case MISL_CAKE_GET_MOUSE_Y:
        fuselage_guest_set_result((uint32_t)CAKE_MouseDeltaY);
        return 1;
    case MISL_CAKE_GET_MOUSE_WHEEL:
        fuselage_guest_set_result((uint32_t)CAKE_MouseWheel);
        return 1;
    case MISL_CAKE_GET_MOUSE_BUTTON: {
        uint32_t button = fuselage_guest_get_argument(0);
        fuselage_guest_set_result((button < CAKE_MOUSE_BUTTON_COUNT && CAKE_MouseButtons[button]) ? 1u : 0u);
        return 1;
    }
    case MISL_CAKE_IS_SILENCED:
        fuselage_guest_set_result((uint32_t)CAKE_IsSilenced());
        return 1;
    case MISL_CAKE_CTRL_CONNECTED:
        fuselage_guest_set_result((uint32_t)CAKE_IsControllerConnected((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_CAKE_CTRL_CONN_STATE:
        fuselage_guest_set_result(
            (uint32_t)CAKE_GetControllerConnectionState((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_CAKE_CTRL_BACKEND:
        fuselage_guest_set_result((uint32_t)CAKE_GetControllerBackend((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_CAKE_CTRL_VENDOR:
        fuselage_guest_set_result((uint32_t)CAKE_GetControllerVendorID((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_CAKE_CTRL_PRODUCT:
        fuselage_guest_set_result((uint32_t)CAKE_GetControllerProductID((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_CAKE_CTRL_XINPUT_IDX:
        fuselage_guest_set_result(
            (uint32_t)CAKE_GetControllerXInputIndex((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_CAKE_CTRL_VIBRATION:
        fuselage_guest_set_result((uint32_t)CAKE_SetControllerVibration(
            (int)fuselage_guest_get_argument(0), (uint16_t)fuselage_guest_get_argument(1),
            (uint16_t)fuselage_guest_get_argument(2)));
        return 1;
    case MISL_CAKE_CTRL_COPY_NAME: {
        const char* name = CAKE_GetControllerName((int)fuselage_guest_get_argument(0));
        uint32_t capacity = fuselage_guest_get_argument(2);
        char* output = fuselage_guest_resolve_output(fuselage_guest_get_argument(1), capacity);
        uint32_t copied = 0;
        if (output && name) {
            while (copied + 1 < capacity && name[copied]) {
                output[copied] = name[copied];
                copied++;
            }
            output[copied] = '\0';
        }
        fuselage_guest_set_result(copied);
        return 1;
    }
    default:
        return fuselage_guest_unknown_ecall(call);
    }
}
