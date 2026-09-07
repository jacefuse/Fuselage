# VPU Preface — Fuselage Virtual Processing Unit

## Overview

The VPU is a software-emulated CPU built into the Fuselage engine. It executes programs called **ROAP** (Run On Any Platform) blobs, which are produced by assembling **MISL** (Machine Independent System Language) — standard RISC-V assembly written with real toolchains.

The goal is to let game developers write games for Fuselage the way programmers wrote games for classic consoles: close to the metal, with total control. The difference is that the "metal" is the VPU, which runs identically on every platform Fuselage is ported to. Write the game once in MISL, assemble it to a ROAP blob, bake it into the Fuselage executable — done.

> **Status:** this document describes what is built and running as of `0.4.2026071702 DERRIERE`. It was originally written (2026-06-27) as a plan, before any of it existed; the plan's four phases are now done, and what follows is a description rather than a proposal. The **F and D floating-point extensions were added 2026-07-17** (see [ISA](#isa-rv32imfd)), validated against the official `rv32uf`/`rv32ud` compliance suites. Audio is the notable exception — see [Not built yet](#not-built-yet).

---

## ISA: RV32IMFD

The VPU implements **RV32I** — the RISC-V 32-bit base integer instruction set — **plus the M, F, and D extensions** (integer multiply/divide, and single- and double-precision floating point).

- 47 base instructions covering integer arithmetic, loads/stores, branches, and jumps
- `MUL` / `MULH` / `MULHSU` / `MULHU` / `DIV` / `DIVU` / `REM` / `REMU` from M
- **F** — single-precision float: the full RV32F set (FLW/FSW, arithmetic, FMA, compares, conversions, sign-injection, FCLASS, FMV)
- **D** — double-precision float: the full RV32D set, sharing the same float registers widened to 64 bits
- 32 integer registers (x0–x31) and 32 float registers (f0–f31), both following standard RISC-V ABI naming (zero, ra, sp, a0–a7; ft0–ft11, fa0–fa7, fs0–fs11)
- MISL is standard RV32IMFD assembly — any RISC-V assembler (LLVM, GNU riscv32) produces valid ROAP input

M is not optional in practice: compiled C reaches for `mul` immediately (the mandelbrot sample's fixed-point math is `mul`/`mulh`), and a guest without it pays a libcall for every multiply.

### Floating point, and how it crosses the ABI

The float register file is **FLEN=64**: a double fills all 64 bits, a single is **NaN-boxed** into the low 32 (upper bits all ones), exactly as the RISC-V spec requires. Arithmetic borrows the host FPU, so results are IEEE-754 and bit-identical across every platform Fuselage targets — the one rule is that doubles stay true 64-bit IEEE, never x87 80-bit extended (a non-issue on the SSE2 / NEON targets in play).

Float is a **compute** extension, not an ABI change. Guests are built `-march=rv32imfd`, but the **ecall boundary stays integer-only** (`-mabi=ilp32`): a guest computes in float/double internally, and any float value that must cross into Fuselage does so as bit patterns in integer registers — a double as an `a0:a1` pair. This keeps the host↔guest contract width-agnostic, so a soft-float guest and a hardware-float guest call the exact same ecalls.

The FCSR tracks the five IEEE **exception flags** (`fflags`: NV/DZ/OF/UF/NX), accumulated from the host FPU on every operation — so the VPU passes the official `rv32uf` and `rv32ud` compliance suites (NaN results are canonicalised, FCVT-to-integer saturates, FMIN/FMAX signal NV on a signaling NaN). The rounding mode (`frm`) is fixed at round-nearest-even, the RISC-V default and all a guest needs.

**Note:** do not confuse RV32I with Sv32. Sv32 is the RISC-V virtual memory scheme and is not relevant to the VPU.

---

## Two ways in: ecall and MMIO

A guest reaches Fuselage two ways, and the split matters for performance.

### ecall — actions and commands

The standard RISC-V `ecall` instruction, the same mechanism RISC-V Linux uses for syscalls. Use it to *do* something: draw, allocate, configure.

**Convention** (see [roap_abi.h](roap_abi.h), the single shared truth for host and guest):
- Call number in `a7`, numbered `0xSSNN` — `SS` = subsystem, `NN` = index within it
- Arguments in `a0`–`a6`; return in `a0` (`a0:a1` pair for 64-bit/double)

```asm
la   a0, msg
li   a7, 0x1000    # ROAP_TL_PRINT
ecall
```

**190 calls are implemented**, across nine subsystems:

| Subsystem | Block | Calls | Role |
|-----------|-------|-------|------|
| SYS | `0x01` | 15 | engine / window / loop control |
| TL | `0x10` | 23 | GDMF text layer |
| SPR | `0x11` | 45 | GDMF sprites |
| TILE | `0x12` | 31 | GDMF tiles |
| PIX | `0x13` | 25 | GDMF pixies |
| PAL | `0x14` | 8 | GDMF colors / palettes |
| IACT | `0x15` | 3 | GDMF interactions |
| CAKE | `0x20` | 14 | input (keyboard / mouse / controllers) |
| DICE | `0x40` | 26 | RNG, timers, deterministic sequences |

`ServiceEcall` in [VPU_roap.c](VPU_roap.c) routes on the subsystem byte to a per-subsystem sub-dispatcher. Guests don't hand-roll these: [fuselage_misl.h](fuselage_misl.h) wraps every call as a named `static inline`, so MISL C reads exactly like Fuselage C — `tlPrint("hi")`, not `ecall` with registers loaded by hand.

Call numbers are stable. `0x1013` is **retired** (it was `PLACE_CELL`, whose host function became private in 0.3) and must not be reused.

### MMIO — bulk state

`ecall` costs a trap. A guest polling the keyboard through `ROAP_CAKE_GET_KEY` pays one **per key, per tick**. So per-frame state is *mapped* instead: fixed regions of the VPU's flat memory that the host fills and the guest reads with plain loads.

```asm
li    t0, 0x000F0000     # ROAP_INPUT_ADDR
lbu   t1, 0x2C(t0)       # keys[KEY_SPACE] -- no trap, just a load
```

**Built and running:** the input page ([roap_mmio.h](roap_mmio.h)) — `RoapInput` at `ROAP_MMIO_BASE` (`0x000F0000`), carrying 256 keys, mouse position/wheel/buttons, and four gamepads with buttons, triggers, and sticks. It is fixed-width and naturally aligned, so its layout is identical on the host (x86-64) and the guest (rv32 ilp32), both little-endian. `sp` starts below the window and `Load()` preserves it when zeroing guest memory, so the page survives blob reloads.

The 0x20 CAKE ecall block still works, but it is the slow path for state reads. Only *actions* (vibration) belong there permanently.

---

## The ROAP blob format

A raw binary. No header, no metadata, no relocation.

- Linked flat at address `0` by [roap.ld](roap.ld), `.text` first, entry at offset 0
- The VPU loads it at 0 and sets `pc` there — what `objcopy` emits is exactly the image that executes
- `entryPC` is 0 for anything the standard script links
- Maximum size is `ROAP_MAX_BLOB_BYTES` — everything below the MMIO window. A blob larger than that is **refused**, not truncated

Guest memory is a 1 MiB flat buffer: program, data, and stack below `0x000F0000`, the MMIO window above it.

---

## The MISL pipeline

```
game.s  --(riscv32-unknown-elf-as -march=rv32imfd -mabi=ilp32)-->  game.o
        --(ld -T roap.ld)---------------------------------->  game.elf
        --(objcopy -O binary)------------------------------>  game.roap
        --(bin2h)------------------------------------------>  game_blob.h
```

The Makefile drives all of it. Generated `*_blob.h` files are checked in, so an ordinary build of Fuselage needs **no cross toolchain** — those rules only fire when a MISL source actually changes.

The build target is named `.roap`, not `.bin`: it is the thing MISLlauncher plays and `bin2h` bakes, and "binary" describes nothing.

MISL can be assembly or compiled C — `riscv32-unknown-elf-gcc -march=rv32imfd -mabi=ilp32 -nostdlib` targeting `fuselage_misl.h` works, and is how the larger samples are written. (`rv32im` still assembles — the float extensions are a superset; `ilp32` keeps the integer ecall ABI.) `_start` needs `__attribute__((section(".text.init")))` to land at address 0.

---

## Shipping a game

Three shapes, one engine, selected by the Makefile:

| Mode | Command | What you get |
|------|---------|--------------|
| **cgame** | `make` (no ROAP present) | `game.c` is the game, in C. |
| **roap** | `make` (a `game.roap` is present) | The blob is baked into `fuselage.exe` at compile time and handed to `VPU()` from `game.c`. One file, no external assets. **The default way to ship MISL.** |
| **launcher** | `make launcher` | `MISLlauncher`, a generic host that loads a `.roap` from disk at runtime — `MISLlauncher.exe game.roap`, or `GAME.ROAP` if given no argument. |

Dropping a `game.roap` beside the Makefile is enough to switch from cgame to roap. `make cgame` / `make launcher` name a mode outright.

In a shipping MISL game, `game()` is little more than:

```c
void game(void) {
    RoapPumpInput();      // publish this tick's input to the guest
    VPU(&game_roap);      // spend one time-slice in the blob
}
```

`VPU()` runs a per-tick slice (a step budget), services ecalls inline, and returns early when the guest yields (`SYS_YIELD` / `WAITVBL` / `SLEEP`) or halts. Once a blob halts or faults it stays halted and further calls are no-ops, so calling it unconditionally is safe.

---

## Layering

The rule: **CAKE and the VPU are completely unaware of each other.**

- [vpu.c](vpu.c) is a pure RISC-V machine — a fetch/decode/execute loop over a flat buffer. It knows nothing of Fuselage, ecall meanings, or which bytes are "special". Keeping it pure is what lets it be validated against riscv-tests.
- [VPU_roap.c](VPU_roap.c) is the dispatcher: it reserves the MMIO window and turns ecalls into real Fuselage calls.
- [roap.c](roap.c) is the orchestrator — the one place that reads CAKE and writes the guest's input page. It is the only file that includes both worlds.

`fuselage.c` is deliberately VPU-agnostic. The orchestrator is called from `game()` while the DERRIERE branch is still taking base-system fixes from COLON; it folds into the engine once `main` switches over.

---

## Not built yet

- **SHARP** — audio (Software Handled Audio Rendering Pipeline). The subsystem doesn't exist yet, in the engine or the ABI; `0x30` is free for it. Designed, though — see [SHARP_Preface.md](../SHARP/SHARP_Preface.md). The guest side is an MMIO event ring of timestamped register writes, not per-write ecalls; `0x30` is for configuration actions only.
- **Bulk-asset MMIO.** Sprite bitmaps and tilemaps still cross as pointer-passing ecalls (`UploadSpriteBitmap`) and per-cell calls (`PlaceTile`). The plan: sprites at fixed-address slots (the atlas is already a fixed structure), tiles via a reserved range plus a host-published descriptor block (they're sized at runtime, so the guest has to discover where its data landed).
- **VRAM dirty-tracking.** Once assets are mapped, the interpreter's store path watches for writes landing in VRAM ranges and sets per-slot dirty bits; Fuselage re-stages only dirty slots after each slice. As an optional store-observer hook, so `vpu.c` stays pure.
- **SRAM** — a persistent guest-writable region that Fuselage transparently saves and restores. Fuselage has no general file I/O by design; writing to the region *is* saving.
- `Vptr0` (variable-length reads, e.g. tile-bitmap upload) only bounds-checks the start address. Mapped assets are what close that gap.
- `SLEEP` currently just yields; there is no tick-count behind it.
- **Project-named ROAP output.** The guest pipeline hardcodes `game.roap` / `game_roap_blob.h`, so every standalone blob a user builds ships under the same generic name regardless of project. The output *file* should follow the project (`screensplat.roap`) — a `ROAP_NAME ?= game` variable plus `$(ROAP_NAME).roap` in the example Makefiles. What keeps this trivial is that the artifact name and the *symbol* name are separable: `bin2h` already takes the symbol as an argument, so the baked path can keep the fixed `game_roap_code` / `game_roap` contract that `roap_embedded.h` and every `game.c` rely on. Only the file name needs to move. MISLlauncher's no-arg `GAME.ROAP` fallback stays as-is — it is the convention for "the blob beside the launcher", not a project name.

---

## Resources

- RISC-V Unprivileged Specification — RV32I base (ch. 2), the M extension (ch. 7), and the F and D single-/double-precision floating-point extensions
- riscv-tests — the official compliance suite, for validating the core
- LLVM or GNU riscv32-unknown-elf toolchain — for assembling and compiling MISL
