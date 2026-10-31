/* Native audio adapter. SEE: (MISL_HOST_SERVICES.md, Audio) */
#include "../runtime/misl_host_arguments_internal.h"
#include "../runtime/misl_host_pointers_internal.h"
#include "../runtime/misl_host_services_internal.h"
#include "../../misl_abi.h"
#include "../../../SHARP/sharp.h"
int fuselage_guest_service_audio(uint32_t call) {
    switch (call) {
    case MISL_SHARP_AVAILABLE:
        fuselage_guest_set_result(SHARP_IsAvailable() ? 1u : 0u);
        return 1;
    case MISL_SHARP_SAMPLE_RATE:
        fuselage_guest_set_result(SHARP_GetSampleRate());
        return 1;
    case MISL_SHARP_POSITION:
        fuselage_guest_set_wide_result(SHARP_GetRenderPosition());
        return 1;
    case MISL_SHARP_CREATE_CHIP:
        fuselage_guest_set_result((uint32_t)SHARP_CreateChip((SharpChipKind)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_SHARP_CHIP_COUNT:
        fuselage_guest_set_result((uint32_t)SHARP_GetChipCount());
        return 1;
    case MISL_SHARP_POOL_SIZE:
        fuselage_guest_set_result(SHARP_GetPoolSize());
        return 1;
    case MISL_SHARP_POOL_WRITE: {

        const void* input_bytes =
            fuselage_guest_resolve_input(fuselage_guest_get_argument(1), fuselage_guest_get_argument(2));
        fuselage_guest_set_result((input_bytes && SHARP_WritePool(fuselage_guest_get_argument(0), input_bytes,
                                                                  fuselage_guest_get_argument(2)))
                                      ? 1u
                                      : 0u);
        return 1;
    }
    case MISL_SHARP_WRITE:
        fuselage_guest_set_result(SHARP_Write((int)fuselage_guest_get_argument(0),
                                              (uint16_t)fuselage_guest_get_argument(1),
                                              (uint16_t)fuselage_guest_get_argument(2))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_SHARP_WRITE_AT:
        fuselage_guest_set_result(
            SHARP_WriteAt((int)fuselage_guest_get_argument(0), (uint16_t)fuselage_guest_get_argument(1),
                          (uint16_t)fuselage_guest_get_argument(2), fuselage_guest_get_wide_argument(3))
                ? 1u
                : 0u);
        return 1;
    case MISL_SHARP_EVENTS_DROPPED:
        fuselage_guest_set_result(SHARP_GetDroppedEventCount());
        return 1;
    case MISL_SHARP_EVENTS_LATE:
        fuselage_guest_set_result(SHARP_GetLateEventCount());
        return 1;

    case MISL_SHARP_SET_CHIP_BUS:
        fuselage_guest_set_result(
            SHARP_SetChipBus((int)fuselage_guest_get_argument(0), (int)fuselage_guest_get_argument(1)) ? 1u
                                                                                                       : 0u);
        return 1;
    case MISL_SHARP_CHIP_BUS:
        fuselage_guest_set_result((uint32_t)SHARP_GetChipBus((int)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_SHARP_ENABLE_BUS:
        fuselage_guest_set_result(
            SHARP_EnableBus((int)fuselage_guest_get_argument(0), fuselage_guest_get_argument(1) != 0) ? 1u
                                                                                                      : 0u);
        return 1;
    case MISL_SHARP_BUS_VOLUME:
        fuselage_guest_set_result(
            SHARP_SetBusVolume((int)fuselage_guest_get_argument(0), fuselage_guest_get_float_argument(1))
                ? 1u
                : 0u);
        return 1;
    case MISL_SHARP_BUS_BALANCE:
        fuselage_guest_set_result(
            SHARP_SetBusBalance((int)fuselage_guest_get_argument(0), fuselage_guest_get_float_argument(1))
                ? 1u
                : 0u);
        return 1;
    case MISL_SHARP_MASTER_VOLUME:
        SHARP_SetMasterVolume(fuselage_guest_get_float_argument(0));
        return 1;
    default:
        return fuselage_guest_unknown_ecall(call);
    }
}
