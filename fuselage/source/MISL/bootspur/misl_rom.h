/* BOOTSPUR system-ROM contract. SEE: (ROAP_FORMAT.md, BOOTSPUR system ROM) */

#ifndef MISL_ROM_H
#define MISL_ROM_H

#define MISL_ROM_MAGIC 0x544F4F42u
#define MISL_ROM_VERSION 2026091001u /* Contract version; unchanged without an entry change. */
#define MISL_ROM_HEADER_BYTES 16u
#define MISL_ROM_TABLE_OFFSET 16u
#define MISL_ROM_ENTRIES 47u

#define MISL_ROM_ECALL_BASE 0x5000u

/* Mirrors the MMIO region identifier; consumers must keep the two equal. */
#define MISL_ROM_REGION_ID 14

#define MISL_ROM_E_MEMCPY 0
#define MISL_ROM_E_MEMSET 1
#define MISL_ROM_E_MEMMOVE 2
#define MISL_ROM_E_MEMCMP 3
#define MISL_ROM_E_STRLEN 4

#define MISL_ROM_E_ASHLDI3 5
#define MISL_ROM_E_LSHRDI3 6
#define MISL_ROM_E_ASHRDI3 7
#define MISL_ROM_E_UDIVDI3 8
#define MISL_ROM_E_UMODDI3 9
#define MISL_ROM_E_DIVDI3 10
#define MISL_ROM_E_MODDI3 11
#define MISL_ROM_E_FLOATUNDIDF 12
#define MISL_ROM_E_FLOATDIDF 13
#define MISL_ROM_E_FIXUNSDFDI 14
#define MISL_ROM_E_FIXDFDI 15
#define MISL_ROM_E_FLOATUNDISF 16
#define MISL_ROM_E_FLOATDISF 17
#define MISL_ROM_E_FIXUNSSFDI 18
#define MISL_ROM_E_FIXSFDI 19

#define MISL_ROM_E_TRACE 20

#define MISL_ROM_E_APPENDSTR 21
#define MISL_ROM_E_APPENDINT 22
#define MISL_ROM_E_APPENDFIXED 23

#define MISL_ROM_E_SINF 24
#define MISL_ROM_E_COSF 25
#define MISL_ROM_E_TANF 26
#define MISL_ROM_E_ATAN2F 27
#define MISL_ROM_E_EXPF 28
#define MISL_ROM_E_LOGF 29
#define MISL_ROM_E_POWF 30
#define MISL_ROM_E_FLOORF 31
#define MISL_ROM_E_CEILF 32
#define MISL_ROM_E_TRUNCF 33
#define MISL_ROM_E_ROUNDF 34

#define MISL_ROM_E_SIN 35
#define MISL_ROM_E_COS 36
#define MISL_ROM_E_TAN 37
#define MISL_ROM_E_ATAN2 38
#define MISL_ROM_E_EXP 39
#define MISL_ROM_E_LOG 40
#define MISL_ROM_E_POW 41
#define MISL_ROM_E_FLOOR 42
#define MISL_ROM_E_CEIL 43
#define MISL_ROM_E_TRUNC 44
#define MISL_ROM_E_ROUND 45

#define MISL_ROM_E_SRAMWRITE 46

#ifndef __ASSEMBLER__
#include <stdint.h>

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t entries;
    uint32_t tableOffset;
} MislRomHeader;
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(MislRomHeader) == MISL_ROM_HEADER_BYTES, "BOOTSPUR header size changed");
#endif
#endif

#endif
