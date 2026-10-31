/* Native guest session. SEE: (MISL_HOST.md, Session interface) */
#ifndef MISL_HOST_H
#define MISL_HOST_H
#include <stdint.h>
#include "runtime/misl_host_limits.h"

#define FUSELAGE_GUEST_VERSION "0.5.2026103001 ENTERON"

typedef struct {
    const uint8_t* bytes;
    uint32_t byte_count;
    uint32_t entry_offset; /* Zero derives the entry from the image. */
} MislGuestImage;

typedef enum {
    MISL_GUEST_HALT_NONE = 0,
    MISL_GUEST_HALT_FINISHED,
    MISL_GUEST_HALT_FAULT,
    MISL_GUEST_HALT_SRAM
} MislGuestHaltKind;

typedef struct {
    MislGuestHaltKind kind;
    const char* reason;
    uint32_t program_counter;
    uint32_t instruction;
    uint32_t stack_pointer;
    uint32_t return_address;
} MislGuestHaltInfo;

#ifdef __cplusplus
extern "C" {
#endif

/* Keep the descriptor and its bytes alive and unchanged while loaded. */
void FuselageRunGuest(const MislGuestImage* image);
int FuselageIsGuestHalted(void);

/* Returns 1 if stopped; otherwise zeroes output and returns 0. NULL is a query. */
int FuselageGetGuestHaltInfo(MislGuestHaltInfo* output);

typedef enum {
    FUSELAGE_GUEST_HALT_ESCAPE = 0,
    FUSELAGE_GUEST_HALT_IGNORE,
    FUSELAGE_GUEST_HALT_MONITOR /* Reserved; currently uses escape presentation. */
} FuselageGuestHaltAction;
void FuselageSetGuestHaltAction(FuselageGuestHaltAction action);
FuselageGuestHaltAction FuselageGetGuestHaltAction(void);
void FuselageHandleGuestHalt(void);
uint64_t FuselageGetGuestStepCount(void);

/* Set before loading. NULL/empty or an overlong path clears the stored path. */
void FuselageSetGuestCaptureDirectory(const char* directory);
void FuselageSetGuestProgramPath(const char* path);

/* Cap program plus extra-pool RAM in MiB; zero imposes no additional cap. */
void FuselageSetGuestMemoryCapMB(uint32_t memory_cap_mib);

#ifdef __cplusplus
}
#endif
#endif
