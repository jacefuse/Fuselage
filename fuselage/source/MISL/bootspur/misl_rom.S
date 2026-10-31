// BOOTSPUR system-ROM header, entry table and register adapters.
// Entry indices are stable; callers load byte offsets from the current table.
// SEE: (MISL_ROM.md, Entry table and stub shapes)
#include "misl_rom.h"

    .section .text
    .globl misl_rom_start
misl_rom_start:
    .word MISL_ROM_MAGIC
    .word MISL_ROM_VERSION
    .word MISL_ROM_ENTRIES
    .word MISL_ROM_TABLE_OFFSET
.if (. - misl_rom_start) != MISL_ROM_TABLE_OFFSET
    .error "MISL ROM table does not follow the declared header"
.endif

.macro MISL_ROM_APPLY_ENTRIES operation
    \operation memcpy,       MISL_ROM_E_MEMCPY,      INTEGER_REGISTERS
    \operation memset,       MISL_ROM_E_MEMSET,      INTEGER_REGISTERS
    \operation memmove,      MISL_ROM_E_MEMMOVE,     INTEGER_REGISTERS
    \operation memcmp,       MISL_ROM_E_MEMCMP,      INTEGER_REGISTERS
    \operation strlen,       MISL_ROM_E_STRLEN,      INTEGER_REGISTERS
    \operation ashldi3,      MISL_ROM_E_ASHLDI3,     INTEGER_REGISTERS
    \operation lshrdi3,      MISL_ROM_E_LSHRDI3,     INTEGER_REGISTERS
    \operation ashrdi3,      MISL_ROM_E_ASHRDI3,     INTEGER_REGISTERS
    \operation udivdi3,      MISL_ROM_E_UDIVDI3,     INTEGER_REGISTERS
    \operation umoddi3,      MISL_ROM_E_UMODDI3,     INTEGER_REGISTERS
    \operation divdi3,       MISL_ROM_E_DIVDI3,      INTEGER_REGISTERS
    \operation moddi3,       MISL_ROM_E_MODDI3,      INTEGER_REGISTERS
    \operation floatundidf,  MISL_ROM_E_FLOATUNDIDF, INTEGER_TO_DOUBLE
    \operation floatdidf,    MISL_ROM_E_FLOATDIDF,   INTEGER_TO_DOUBLE
    \operation fixunsdfdi,   MISL_ROM_E_FIXUNSDFDI,  DOUBLE_TO_INTEGER
    \operation fixdfdi,      MISL_ROM_E_FIXDFDI,     DOUBLE_TO_INTEGER
    \operation floatundisf,  MISL_ROM_E_FLOATUNDISF, INTEGER_TO_FLOAT
    \operation floatdisf,    MISL_ROM_E_FLOATDISF,   INTEGER_TO_FLOAT
    \operation fixunssfdi,   MISL_ROM_E_FIXUNSSFDI,  FLOAT_TO_INTEGER
    \operation fixsfdi,      MISL_ROM_E_FIXSFDI,     FLOAT_TO_INTEGER
    \operation trace,        MISL_ROM_E_TRACE,       INTEGER_REGISTERS
    \operation appendstr,    MISL_ROM_E_APPENDSTR,   INTEGER_REGISTERS
    \operation appendint,    MISL_ROM_E_APPENDINT,   INTEGER_REGISTERS
    \operation appendfixed,  MISL_ROM_E_APPENDFIXED, INTEGER_REGISTERS
    \operation sinf,         MISL_ROM_E_SINF,        FLOAT_UNARY
    \operation cosf,         MISL_ROM_E_COSF,        FLOAT_UNARY
    \operation tanf,         MISL_ROM_E_TANF,        FLOAT_UNARY
    \operation atan2f,       MISL_ROM_E_ATAN2F,      FLOAT_BINARY
    \operation expf,         MISL_ROM_E_EXPF,        FLOAT_UNARY
    \operation logf,         MISL_ROM_E_LOGF,        FLOAT_UNARY
    \operation powf,         MISL_ROM_E_POWF,        FLOAT_BINARY
    \operation floorf,       MISL_ROM_E_FLOORF,      FLOAT_UNARY
    \operation ceilf,        MISL_ROM_E_CEILF,       FLOAT_UNARY
    \operation truncf,       MISL_ROM_E_TRUNCF,      FLOAT_UNARY
    \operation roundf,       MISL_ROM_E_ROUNDF,      FLOAT_UNARY
    \operation sin,          MISL_ROM_E_SIN,         DOUBLE_UNARY
    \operation cos,          MISL_ROM_E_COS,         DOUBLE_UNARY
    \operation tan,          MISL_ROM_E_TAN,         DOUBLE_UNARY
    \operation atan2,        MISL_ROM_E_ATAN2,       DOUBLE_BINARY
    \operation exp,          MISL_ROM_E_EXP,         DOUBLE_UNARY
    \operation log,          MISL_ROM_E_LOG,         DOUBLE_UNARY
    \operation pow,          MISL_ROM_E_POW,         DOUBLE_BINARY
    \operation floor,        MISL_ROM_E_FLOOR,       DOUBLE_UNARY
    \operation ceil,         MISL_ROM_E_CEIL,        DOUBLE_UNARY
    \operation trunc,        MISL_ROM_E_TRUNC,       DOUBLE_UNARY
    \operation round,        MISL_ROM_E_ROUND,       DOUBLE_UNARY
    \operation sramwrite,    MISL_ROM_E_SRAMWRITE,   INTEGER_REGISTERS
.endm

.set misl_rom_next_entry, 0
.macro MISL_ROM_EMIT_OFFSET entry_name, entry_index, entry_kind
    .if \entry_index != misl_rom_next_entry
        .error "MISL ROM entry order disagrees with its index"
    .endif
    .set misl_rom_next_entry, misl_rom_next_entry + 1
    .word misl_rom_stub_\entry_name - misl_rom_start
.endm
    MISL_ROM_APPLY_ENTRIES MISL_ROM_EMIT_OFFSET
.if misl_rom_next_entry != MISL_ROM_ENTRIES
    .error "MISL ROM entry count disagrees with the contract"
.endif

.macro MISL_ROM_EMIT_STUB entry_name, entry_index, entry_kind
    .p2align 2
misl_rom_stub_\entry_name:
    .set misl_rom_kind_supported, 0
.ifc \entry_kind, INTEGER_REGISTERS
    .set misl_rom_kind_supported, 1
    li   a7, MISL_ROM_ECALL_BASE + \entry_index
    ecall
    ret
.endif
.ifc \entry_kind, FLOAT_UNARY
    .set misl_rom_kind_supported, 1
    fmv.x.w a0, fa0
    li   a7, MISL_ROM_ECALL_BASE + \entry_index
    ecall
    fmv.w.x fa0, a0
    ret
.endif
.ifc \entry_kind, FLOAT_BINARY
    .set misl_rom_kind_supported, 1
    fmv.x.w a0, fa0
    fmv.x.w a1, fa1
    li   a7, MISL_ROM_ECALL_BASE + \entry_index
    ecall
    fmv.w.x fa0, a0
    ret
.endif
.ifc \entry_kind, DOUBLE_UNARY
    .set misl_rom_kind_supported, 1
    addi sp, sp, -8
    fsd  fa0, 0(sp)
    lw   a0, 0(sp)
    lw   a1, 4(sp)
    li   a7, MISL_ROM_ECALL_BASE + \entry_index
    ecall
    sw   a0, 0(sp)
    sw   a1, 4(sp)
    fld  fa0, 0(sp)
    addi sp, sp, 8
    ret
.endif
.ifc \entry_kind, DOUBLE_BINARY
    .set misl_rom_kind_supported, 1
    addi sp, sp, -16
    fsd  fa0, 0(sp)
    fsd  fa1, 8(sp)
    lw   a0, 0(sp)
    lw   a1, 4(sp)
    lw   a2, 8(sp)
    lw   a3, 12(sp)
    li   a7, MISL_ROM_ECALL_BASE + \entry_index
    ecall
    sw   a0, 0(sp)
    sw   a1, 4(sp)
    fld  fa0, 0(sp)
    addi sp, sp, 16
    ret
.endif
.ifc \entry_kind, INTEGER_TO_DOUBLE
    .set misl_rom_kind_supported, 1
    addi sp, sp, -8
    li   a7, MISL_ROM_ECALL_BASE + \entry_index
    ecall
    sw   a0, 0(sp)
    sw   a1, 4(sp)
    fld  fa0, 0(sp)
    addi sp, sp, 8
    ret
.endif
.ifc \entry_kind, DOUBLE_TO_INTEGER
    .set misl_rom_kind_supported, 1
    addi sp, sp, -8
    fsd  fa0, 0(sp)
    lw   a0, 0(sp)
    lw   a1, 4(sp)
    li   a7, MISL_ROM_ECALL_BASE + \entry_index
    ecall
    addi sp, sp, 8
    ret
.endif
.ifc \entry_kind, INTEGER_TO_FLOAT
    .set misl_rom_kind_supported, 1
    li   a7, MISL_ROM_ECALL_BASE + \entry_index
    ecall
    fmv.w.x fa0, a0
    ret
.endif
.ifc \entry_kind, FLOAT_TO_INTEGER
    .set misl_rom_kind_supported, 1
    fmv.x.w a0, fa0
    li   a7, MISL_ROM_ECALL_BASE + \entry_index
    ecall
    ret
.endif
    .if misl_rom_kind_supported == 0
        .error "Unsupported MISL ROM stub kind"
    .endif
.endm
    MISL_ROM_APPLY_ENTRIES MISL_ROM_EMIT_STUB

    .globl misl_rom_end
misl_rom_end:
