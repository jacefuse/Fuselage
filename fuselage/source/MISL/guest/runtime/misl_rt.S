// Guest BOOTSPUR tail jumps. Discover regions before calling a ROM service.
// Weak symbols allow application replacements; unused sections can be discarded.
// SEE: (MISL_RUNTIME.md, ROM thunks)
#include "../../bootspur/misl_rom.h"

.macro MISL_GUEST_THUNK symbol, entry_index
    .section .text.\symbol, "ax", @progbits
    .weak   \symbol
    .type   \symbol, @function
    .p2align 2
\symbol:
    lui  t0, %hi(misl_guest_region_bases + 4 * MISL_ROM_REGION_ID)
    lw   t0, %lo(misl_guest_region_bases + 4 * MISL_ROM_REGION_ID)(t0)
    lw   t1, (MISL_ROM_TABLE_OFFSET + 4 * \entry_index)(t0)
    add  t0, t0, t1
    jr   t0
    .size \symbol, . - \symbol
.endm

    MISL_GUEST_THUNK memcpy,        MISL_ROM_E_MEMCPY
    MISL_GUEST_THUNK memset,        MISL_ROM_E_MEMSET
    MISL_GUEST_THUNK memmove,       MISL_ROM_E_MEMMOVE
    MISL_GUEST_THUNK memcmp,        MISL_ROM_E_MEMCMP
    MISL_GUEST_THUNK strlen,        MISL_ROM_E_STRLEN

    MISL_GUEST_THUNK __ashldi3,     MISL_ROM_E_ASHLDI3
    MISL_GUEST_THUNK __lshrdi3,     MISL_ROM_E_LSHRDI3
    MISL_GUEST_THUNK __ashrdi3,     MISL_ROM_E_ASHRDI3
    MISL_GUEST_THUNK __udivdi3,     MISL_ROM_E_UDIVDI3
    MISL_GUEST_THUNK __umoddi3,     MISL_ROM_E_UMODDI3
    MISL_GUEST_THUNK __divdi3,      MISL_ROM_E_DIVDI3
    MISL_GUEST_THUNK __moddi3,      MISL_ROM_E_MODDI3

    MISL_GUEST_THUNK MISL_Trace,       MISL_ROM_E_TRACE
    MISL_GUEST_THUNK MISL_AppendStr,   MISL_ROM_E_APPENDSTR
    MISL_GUEST_THUNK MISL_AppendInt,   MISL_ROM_E_APPENDINT
    MISL_GUEST_THUNK MISL_AppendFixed, MISL_ROM_E_APPENDFIXED

    MISL_GUEST_THUNK MISL_WriteSram,   MISL_ROM_E_SRAMWRITE

#if defined(__riscv_float_abi_single) || defined(__riscv_float_abi_double)

    MISL_GUEST_THUNK __floatundisf, MISL_ROM_E_FLOATUNDISF
    MISL_GUEST_THUNK __floatdisf,   MISL_ROM_E_FLOATDISF
    MISL_GUEST_THUNK __fixunssfdi,  MISL_ROM_E_FIXUNSSFDI
    MISL_GUEST_THUNK __fixsfdi,     MISL_ROM_E_FIXSFDI
    MISL_GUEST_THUNK sinf,          MISL_ROM_E_SINF
    MISL_GUEST_THUNK cosf,          MISL_ROM_E_COSF
    MISL_GUEST_THUNK tanf,          MISL_ROM_E_TANF
    MISL_GUEST_THUNK atan2f,        MISL_ROM_E_ATAN2F
    MISL_GUEST_THUNK expf,          MISL_ROM_E_EXPF
    MISL_GUEST_THUNK logf,          MISL_ROM_E_LOGF
    MISL_GUEST_THUNK powf,          MISL_ROM_E_POWF
    MISL_GUEST_THUNK floorf,        MISL_ROM_E_FLOORF
    MISL_GUEST_THUNK ceilf,         MISL_ROM_E_CEILF
    MISL_GUEST_THUNK truncf,        MISL_ROM_E_TRUNCF
    MISL_GUEST_THUNK roundf,        MISL_ROM_E_ROUNDF
#endif

#if defined(__riscv_float_abi_double)

    MISL_GUEST_THUNK __floatundidf, MISL_ROM_E_FLOATUNDIDF
    MISL_GUEST_THUNK __floatdidf,   MISL_ROM_E_FLOATDIDF
    MISL_GUEST_THUNK __fixunsdfdi,  MISL_ROM_E_FIXUNSDFDI
    MISL_GUEST_THUNK __fixdfdi,     MISL_ROM_E_FIXDFDI
    MISL_GUEST_THUNK sin,           MISL_ROM_E_SIN
    MISL_GUEST_THUNK cos,           MISL_ROM_E_COS
    MISL_GUEST_THUNK tan,           MISL_ROM_E_TAN
    MISL_GUEST_THUNK atan2,         MISL_ROM_E_ATAN2
    MISL_GUEST_THUNK exp,           MISL_ROM_E_EXP
    MISL_GUEST_THUNK log,           MISL_ROM_E_LOG
    MISL_GUEST_THUNK pow,           MISL_ROM_E_POW
    MISL_GUEST_THUNK floor,         MISL_ROM_E_FLOOR
    MISL_GUEST_THUNK ceil,          MISL_ROM_E_CEIL
    MISL_GUEST_THUNK trunc,         MISL_ROM_E_TRUNC
    MISL_GUEST_THUNK round,         MISL_ROM_E_ROUND
#endif
