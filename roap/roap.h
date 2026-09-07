#pragma once

// The ROAP orchestrator -- the one place that sees both worlds.
//
// CAKE and the VPU never reference each other. CAKE knows nothing about
// guests; the VPU core (vpu.c) is a generic RISC-V machine with a flat memory
// buffer and no idea any of it is special. This file is the seam: it reads
// host input and writes it into the MMIO page reserved inside guest memory
// (see roap_mmio.h), which the guest then reads with plain loads.
//
// Why a mapped page rather than an ecall per query: a guest that polls the
// keyboard through ROAP_CAKE_GET_KEY pays a trap for every key, every tick.
// Reading the same state out of the MMIO page is an `lw`. The ecall input
// surface still works and still exists; this is the fast path beside it.

// Copy this tick's host input into the guest's MMIO input page. Call once per
// tick, before handing the blob to VPU() -- the guest sees whatever was last
// written here, so skipping it leaves the guest reading stale input with no
// indication anything is wrong.
//
// Safe to call before any blob is loaded: the page lives in a static buffer
// that is live from startup, and Load() preserves the MMIO window when it
// zeroes guest memory.
void RoapPumpInput(void);
