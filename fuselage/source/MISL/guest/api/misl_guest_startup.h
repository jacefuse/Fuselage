#pragma once
#include "misl_guest_memory.h"

// Guest ABI declaration and project manifest.
// SEE: (MISL_GUEST.md, Building and startup)

static inline void MISL_Halt(void) {
    __asm__ volatile("ebreak");
}

static inline bool MISL_DeclareABI(void) {
    return MISL_GUEST_CALL(MISL_SYS_ABI_DECLARE, MISL_ABI_VERSION, 0, 0, 0, 0, 0, 0) != 0;
}

#define MISL_PROGRAM_BEGIN()                \
    do {                                    \
        if (!MISL_DeclareABI()) {            \
            MISL_Halt();                    \
        }                                   \
        if (!MISL_DiscoverRegions()) {       \
            MISL_Halt();                    \
        }                                   \
    } while (0)

#define MISL_PROJECT(...)                                                   \
    __attribute__((section(".misl_manifest"), used, aligned(4)))               \
    const RoapManifest misl_project_manifest = {                             \
        .magic = ROAP_MANIFEST_MAGIC,                                        \
        .version = ROAP_MANIFEST_VERSION,                                    \
        .codeOffset = ROAP_MANIFEST_BYTES,                                   \
        __VA_ARGS__                                                         \
    }
