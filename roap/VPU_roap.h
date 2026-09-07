#pragma once
#include <stdint.h>
#include "roap_mmio.h"   // ROAP_MMIO_BASE -- the ceiling a blob has to fit under

#define VPU_VERSION "0.4.2026071601 DERRIERE"

// The largest blob the VPU will load. Guest memory is a flat buffer with the
// MMIO window reserved at the top (see roap_mmio.h) and sp starting just below
// it, so the loadable image is everything under that window. A blob bigger than
// this is refused outright rather than truncated: half a program is not a
// program, and truncating one would silently overwrite the MMIO page the guest
// reads its input from.
#define ROAP_MAX_BLOB_BYTES  ((uint32_t)ROAP_MMIO_BASE)

// ROAP -- Run On Any Platform. A blob of RV32I machine code assembled from
// MISL and baked directly into the Fuselage executable. In a shipping game a
// ROAP is held as a single constant and handed to VPU() every game() tick;
// game() itself does little more than that call plus housekeeping.
typedef struct {
    const uint8_t *code;    // raw RV32I bytes, little-endian
    uint32_t       size;    // length of code in bytes
    uint32_t       entryPC; // starting program counter (byte offset into code)
} ROAP;

// Run one per-tick time-slice of the given blob.
//
// The first call lazily loads the blob into VPU memory and starts it RUNNING.
// Each call spends up to a fixed step budget ("pass-time-limit"), servicing
// ecalls inline (e.g. tlPrint) and returning early when the VPU yields
// (voluntary release, sleep, waitVBL) or halts. Once the blob has halted or
// faulted it stays HALTED and further calls are no-ops -- so calling this
// unconditionally from game() is safe and idempotent.
//
// This is the entire production surface. The in-engine assembly monitor, when
// it exists, is a wrapper built over the same dispatch core -- a learning and
// scoping tool, never a replacement for this call.
void VPU(const ROAP *roap);

// True once the loaded blob has halted or faulted. Useful for game()-level
// housekeeping (e.g. deciding when to tear down or restart).
int VPUHalted(void);

// Pointer to the VPU's MMIO input page inside guest memory (see roap_mmio.h).
// The Fuselage orchestrator writes host input here each tick, before VPU();
// the guest reads it with plain loads. Always valid (guest memory is a live
// static buffer); the MMIO window is preserved across blob (re)loads.
void *VPUInputPage(void);
