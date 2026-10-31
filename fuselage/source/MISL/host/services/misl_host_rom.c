/* BOOTSPUR dispatch. SEE: (MISL_HOST_SERVICES.md, ROM) */
#include "../runtime/misl_host_arguments_internal.h"
#include "../runtime/misl_host_pointers_internal.h"
#include "../runtime/misl_host_services_internal.h"
#include "../../misl_abi.h"
#include "misl_host_rom_internal.h"
#include "../../bootspur/misl_rom.h"
#include "../../math/misl_math.h"
#include "../../compiler/misl_helpers.h"
int fuselage_guest_service_rom(uint32_t call) {
    switch (call) {
    case MISL_ROM_MEMCPY:
    case MISL_ROM_MEMMOVE:
    case MISL_ROM_MEMSET:
    case MISL_ROM_MEMCMP:
    case MISL_ROM_STRLEN:
    case MISL_ROM_APPENDSTR:
    case MISL_ROM_APPENDINT:
    case MISL_ROM_APPENDFIXED:
        return fuselage_guest_rom_memory(call);
    case MISL_ROM_ASHLDI3:
        fuselage_guest_set_wide_result((uint64_t)misl_helpers_shift_left(
            (long long)fuselage_guest_get_wide_argument(0), (int)fuselage_guest_get_argument(2)));
        return 1;
    case MISL_ROM_LSHRDI3:
        fuselage_guest_set_wide_result(misl_helpers_shift_right(fuselage_guest_get_wide_argument(0),
                                                                (int)fuselage_guest_get_argument(2)));
        return 1;
    case MISL_ROM_ASHRDI3:
        fuselage_guest_set_wide_result((uint64_t)misl_helpers_shift_right_signed(
            (long long)fuselage_guest_get_wide_argument(0), (int)fuselage_guest_get_argument(2)));
        return 1;
    case MISL_ROM_UDIVDI3:
        fuselage_guest_set_wide_result(misl_helpers_divide_unsigned(fuselage_guest_get_wide_argument(0),
                                                                    fuselage_guest_get_wide_argument(2)));
        return 1;
    case MISL_ROM_UMODDI3:
        fuselage_guest_set_wide_result(misl_helpers_remainder_unsigned(fuselage_guest_get_wide_argument(0),
                                                                       fuselage_guest_get_wide_argument(2)));
        return 1;
    case MISL_ROM_DIVDI3:
        fuselage_guest_set_wide_result((uint64_t)misl_helpers_divide_signed(
            (long long)fuselage_guest_get_wide_argument(0), (long long)fuselage_guest_get_wide_argument(2)));
        return 1;
    case MISL_ROM_MODDI3:
        fuselage_guest_set_wide_result((uint64_t)misl_helpers_remainder_signed(
            (long long)fuselage_guest_get_wide_argument(0), (long long)fuselage_guest_get_wide_argument(2)));
        return 1;
    case MISL_ROM_FLOATUNDIDF:
        fuselage_guest_set_double_result(
            misl_helpers_unsigned_to_double(fuselage_guest_get_wide_argument(0)));
        return 1;
    case MISL_ROM_FLOATDIDF:
        fuselage_guest_set_double_result(
            misl_helpers_signed_to_double((long long)fuselage_guest_get_wide_argument(0)));
        return 1;
    case MISL_ROM_FIXUNSDFDI:
        fuselage_guest_set_wide_result(
            misl_helpers_double_to_unsigned(fuselage_guest_get_double_argument(0)));
        return 1;
    case MISL_ROM_FIXDFDI:
        fuselage_guest_set_wide_result(
            (uint64_t)misl_helpers_double_to_signed(fuselage_guest_get_double_argument(0)));
        return 1;
    case MISL_ROM_FLOATUNDISF:
        fuselage_guest_set_float_result(misl_helpers_unsigned_to_float(fuselage_guest_get_wide_argument(0)));
        return 1;
    case MISL_ROM_FLOATDISF:
        fuselage_guest_set_float_result(
            misl_helpers_signed_to_float((long long)fuselage_guest_get_wide_argument(0)));
        return 1;
    case MISL_ROM_FIXUNSSFDI:
        fuselage_guest_set_wide_result(misl_helpers_float_to_unsigned(fuselage_guest_get_float_argument(0)));
        return 1;
    case MISL_ROM_FIXSFDI:
        fuselage_guest_set_wide_result(
            (uint64_t)misl_helpers_float_to_signed(fuselage_guest_get_float_argument(0)));
        return 1;

    case MISL_ROM_TRACE:
        return fuselage_guest_rom_trace(call, fuselage_guest_get_argument(0));

    case MISL_ROM_SRAMWRITE:
        return fuselage_guest_rom_write_save(call);
    case MISL_ROM_SINF:
        fuselage_guest_set_float_result(MISL_CalculateSineFloat(fuselage_guest_get_float_argument(0)));
        return 1;
    case MISL_ROM_COSF:
        fuselage_guest_set_float_result(MISL_CalculateCosineFloat(fuselage_guest_get_float_argument(0)));
        return 1;
    case MISL_ROM_TANF:
        fuselage_guest_set_float_result(MISL_CalculateTangentFloat(fuselage_guest_get_float_argument(0)));
        return 1;
    case MISL_ROM_ATAN2F:
        fuselage_guest_set_float_result(MISL_CalculateArcTangent2Float(fuselage_guest_get_float_argument(0),
                                                                       fuselage_guest_get_float_argument(1)));
        return 1;
    case MISL_ROM_EXPF:
        fuselage_guest_set_float_result(MISL_CalculateExponentialFloat(fuselage_guest_get_float_argument(0)));
        return 1;
    case MISL_ROM_LOGF:
        fuselage_guest_set_float_result(
            MISL_CalculateNaturalLogarithmFloat(fuselage_guest_get_float_argument(0)));
        return 1;
    case MISL_ROM_POWF:
        fuselage_guest_set_float_result(MISL_CalculatePowerFloat(fuselage_guest_get_float_argument(0),
                                                                 fuselage_guest_get_float_argument(1)));
        return 1;
    case MISL_ROM_FLOORF:
        fuselage_guest_set_float_result(MISL_RoundDownFloat(fuselage_guest_get_float_argument(0)));
        return 1;
    case MISL_ROM_CEILF:
        fuselage_guest_set_float_result(MISL_RoundUpFloat(fuselage_guest_get_float_argument(0)));
        return 1;
    case MISL_ROM_TRUNCF:
        fuselage_guest_set_float_result(MISL_TruncateFloat(fuselage_guest_get_float_argument(0)));
        return 1;
    case MISL_ROM_ROUNDF:
        fuselage_guest_set_float_result(MISL_RoundFloat(fuselage_guest_get_float_argument(0)));
        return 1;
    case MISL_ROM_SIN:
        fuselage_guest_set_double_result(MISL_CalculateSine(fuselage_guest_get_double_argument(0)));
        return 1;
    case MISL_ROM_COS:
        fuselage_guest_set_double_result(MISL_CalculateCosine(fuselage_guest_get_double_argument(0)));
        return 1;
    case MISL_ROM_TAN:
        fuselage_guest_set_double_result(MISL_CalculateTangent(fuselage_guest_get_double_argument(0)));
        return 1;
    case MISL_ROM_ATAN2:
        fuselage_guest_set_double_result(MISL_CalculateArcTangent2(fuselage_guest_get_double_argument(0),
                                                                   fuselage_guest_get_double_argument(2)));
        return 1;
    case MISL_ROM_EXP:
        fuselage_guest_set_double_result(MISL_CalculateExponential(fuselage_guest_get_double_argument(0)));
        return 1;
    case MISL_ROM_LOG:
        fuselage_guest_set_double_result(
            MISL_CalculateNaturalLogarithm(fuselage_guest_get_double_argument(0)));
        return 1;
    case MISL_ROM_POW:
        fuselage_guest_set_double_result(MISL_CalculatePower(fuselage_guest_get_double_argument(0),
                                                             fuselage_guest_get_double_argument(2)));
        return 1;
    case MISL_ROM_FLOOR:
        fuselage_guest_set_double_result(MISL_RoundDown(fuselage_guest_get_double_argument(0)));
        return 1;
    case MISL_ROM_CEIL:
        fuselage_guest_set_double_result(MISL_RoundUp(fuselage_guest_get_double_argument(0)));
        return 1;
    case MISL_ROM_TRUNC:
        fuselage_guest_set_double_result(MISL_Truncate(fuselage_guest_get_double_argument(0)));
        return 1;
    case MISL_ROM_ROUND:
        fuselage_guest_set_double_result(MISL_Round(fuselage_guest_get_double_argument(0)));
        return 1;

    default:

        if (call >= MISL_ROM_ECALL_BASE && call < MISL_ROM_ECALL_BASE + MISL_ROM_ENTRIES) {
            return fuselage_guest_rom_unimplemented(call);
        }
        return fuselage_guest_unknown_ecall(call);
    }
}
