/* Native dice adapter. SEE: (MISL_HOST_SERVICES.md, Dice) */
#include "../runtime/misl_host_arguments_internal.h"
#include "../runtime/misl_host_pointers_internal.h"
#include "../runtime/misl_host_services_internal.h"
#include "../../misl_abi.h"
#include "../../../DICE/dice.h"
#include <string.h>
int fuselage_guest_service_dice(uint32_t call) {
    switch (call) {
    case MISL_DICE_INIT_RNG:
        fuselage_guest_set_result(DICE_InitRNG((uint8_t)fuselage_guest_get_argument(0),
                                               (DICE_RNGMode)fuselage_guest_get_argument(1),
                                               fuselage_guest_get_wide_argument(2))
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_DICE_RELEASE_RNG:
        fuselage_guest_set_result(DICE_ReleaseRNG((uint8_t)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;

    case MISL_DICE_RNG_MIX_ENTROPY: {
        const void* input_bytes =
            fuselage_guest_resolve_input(fuselage_guest_get_argument(1), fuselage_guest_get_argument(2));
        fuselage_guest_set_result(
            (input_bytes && DICE_MixRNGEntropy((uint8_t)fuselage_guest_get_argument(0), input_bytes,
                                               (size_t)fuselage_guest_get_argument(2)))
                ? 1u
                : 0u);
        return 1;
    }
    case MISL_DICE_RNG_GET_SEED:
        fuselage_guest_set_wide_result(DICE_GetRNGSeed((uint8_t)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_DICE_RAND_UINT:
        fuselage_guest_set_result(DICE_RandUint((uint8_t)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_DICE_RAND_INT:
        fuselage_guest_set_result((uint32_t)DICE_RandInt((uint8_t)fuselage_guest_get_argument(0),
                                                         (int)fuselage_guest_get_argument(1),
                                                         (int)fuselage_guest_get_argument(2)));
        return 1;
    case MISL_DICE_RAND_FLOAT:
        fuselage_guest_set_float_result(DICE_RandFloat((uint8_t)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_DICE_RAND_CHANCE:
        fuselage_guest_set_result(
            DICE_RandChance((uint8_t)fuselage_guest_get_argument(0), fuselage_guest_get_float_argument(1))
                ? 1u
                : 0u);
        return 1;
    case MISL_DICE_INIT_TIMER:
        fuselage_guest_set_result(DICE_InitTimerInterval((uint8_t)fuselage_guest_get_argument(0),
                                                         fuselage_guest_get_wide_argument(1),
                                                         fuselage_guest_get_argument(3) != 0, NULL, NULL)
                                      ? 1u
                                      : 0u);
        return 1;
    case MISL_DICE_RELEASE_TIMER:
        fuselage_guest_set_result(DICE_ReleaseTimer((uint8_t)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_DICE_TIMER_START:
        fuselage_guest_set_result(DICE_StartTimer((uint8_t)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_DICE_TIMER_STOP:
        fuselage_guest_set_result(DICE_StopTimer((uint8_t)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_DICE_TIMER_PAUSE:
        fuselage_guest_set_result(DICE_PauseTimer((uint8_t)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_DICE_TIMER_RESUME:
        fuselage_guest_set_result(DICE_ResumeTimer((uint8_t)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_DICE_TIMER_IS_RUNNING:
        fuselage_guest_set_result(DICE_IsTimerRunning((uint8_t)fuselage_guest_get_argument(0)) ? 1u : 0u);
        return 1;
    case MISL_DICE_TIMER_CONSUME:
        fuselage_guest_set_result(DICE_ConsumeTimer((uint8_t)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_DICE_TIMER_DELTA:
        fuselage_guest_set_double_result(DICE_GetTimerDelta((uint8_t)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_DICE_SEED_DIE:
        fuselage_guest_set_result(
            DICE_SeedDie((uint8_t)fuselage_guest_get_argument(0), fuselage_guest_get_wide_argument(1)) ? 1u
                                                                                                       : 0u);
        return 1;
    case MISL_DICE_DIE_RANGE:
        fuselage_guest_set_result((uint32_t)DICE_RollDieRange((uint8_t)fuselage_guest_get_argument(0),
                                                              (int)fuselage_guest_get_argument(1),
                                                              (int)fuselage_guest_get_argument(2)));
        return 1;
    case MISL_DICE_DIE_FLOAT:
        fuselage_guest_set_float_result(DICE_RollDieFloat((uint8_t)fuselage_guest_get_argument(0)));
        return 1;
    case MISL_DICE_DIE_CHANCE:
        fuselage_guest_set_result(
            DICE_RollDieChance((uint8_t)fuselage_guest_get_argument(0), fuselage_guest_get_float_argument(1))
                ? 1u
                : 0u);
        return 1;
    case MISL_DICE_SEED_SEQUENCE:
        fuselage_guest_set_result(
            DICE_SeedSequence((uint8_t)fuselage_guest_get_argument(0), fuselage_guest_get_wide_argument(1))
                ? 1u
                : 0u);
        return 1;
    case MISL_DICE_SEQ_UINT:
        fuselage_guest_set_result(DICE_GetSequenceUint((uint8_t)fuselage_guest_get_argument(0),
                                                       fuselage_guest_get_wide_argument(1)));
        return 1;
    case MISL_DICE_SEQ_INT:
        fuselage_guest_set_result((uint32_t)DICE_GetSequenceInt(
            (uint8_t)fuselage_guest_get_argument(0), fuselage_guest_get_wide_argument(1),
            (int)fuselage_guest_get_argument(3), (int)fuselage_guest_get_argument(4)));
        return 1;
    case MISL_DICE_SEQ_FLOAT:
        fuselage_guest_set_float_result(DICE_GetSequenceFloat((uint8_t)fuselage_guest_get_argument(0),
                                                              fuselage_guest_get_wide_argument(1)));
        return 1;
    case MISL_DICE_SEQ_CHANCE:
        fuselage_guest_set_result(DICE_GetSequenceChance((uint8_t)fuselage_guest_get_argument(0),
                                                         fuselage_guest_get_wide_argument(1),
                                                         fuselage_guest_get_float_argument(3))
                                      ? 1u
                                      : 0u);
        return 1;

    case MISL_DICE_ALLOC_TIMER:
        fuselage_guest_set_result((uint32_t)(int32_t)DICE_AllocTimer());
        return 1;

    case MISL_DICE_REAL_TIME: {
        void* output =
            fuselage_guest_resolve_output(fuselage_guest_get_argument(0), (uint32_t)sizeof(MislClock));
        if (output) {
            MislClock clock;
            fuselage_guest_fill_clock(&clock);
            memcpy(output, &clock, sizeof clock);
        }
        fuselage_guest_set_result(output ? 1u : 0u);
        return 1;
    }
    default:
        return fuselage_guest_unknown_ecall(call);
    }
}
