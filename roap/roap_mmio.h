#pragma once
#include <stdint.h>

// ROAP MMIO -- memory-mapped I/O between the Fuselage host and a MISL guest.
//
// The VPU core (vpu.c) knows nothing about any of this: it is a generic
// RISC-V machine with a flat memory buffer. The ROAP layer reserves a region
// at the top of that buffer (dropping sp below it) and the Fuselage-owned
// orchestrator (roap.c) copies host state into it each tick. The guest reads
// it with plain loads. CAKE and the VPU never reference each other -- only the
// orchestrator, which is the one place that includes both worlds.

// ---------------------------------------------------------------------------
// The guest memory map.
//
//   0x00000000  program / .data / .bss / stack   (sp starts at ROAP_MMIO_BASE)
//   0x00800000  input page       -- host writes, guest reads, every tick
//   0x00801000  descriptor block -- reserved: what the host actually allocated
//   0x00810000  sprite atlas     -- 1024 slots x 2048 bytes = 2 MiB
//   0x00A10000  tile pool        -- reserved for tile atlases/maps
//   0x01000000  end (16 MiB)
//
// Everything mapped lives ABOVE ROAP_MMIO_BASE and everything the program owns
// lives below it. That split is what lets Load() zero the program without
// touching a mapped region, and lets the regions survive a blob reload.
//
// 16 MiB is a WAYPOINT. The VPU is what 0.4 is for, and that means MISL has to
// be able to carry large applications -- so guest memory is headed for a
// lazily-committed 4 GB (the whole RV32 address space), where a game costs only
// the pages it touches. Nothing here may assume guest memory is small, or that
// this size is final.
#define ROAP_MEM_SIZE    (16u * 1024u * 1024u)   // total guest memory
#define ROAP_MMIO_BASE   0x00800000u             // the mapped window starts here
#define ROAP_INPUT_ADDR  ROAP_MMIO_BASE
#define ROAP_DESC_ADDR   (ROAP_MMIO_BASE + 0x1000u)

// --- sprite atlas ----------------------------------------------------------
// Slot N is ALWAYS at ROAP_SPRITE_ADDR + N * ROAP_SPRITE_STRIDE. The atlas is a
// fixed structure, so there is nothing to discover and nothing to publish: a
// guest writes 4-bit indexed pixels straight into the slot and the host notices.
// (Tiles are sized at runtime, so they get a descriptor instead -- see above.)
#define ROAP_SPRITE_ADDR   (ROAP_MMIO_BASE + 0x10000u)
#define ROAP_SPRITE_STRIDE 2048u    // 64x64 pixels, 2 per byte -- one atlas slot
#define ROAP_SPRITE_SLOTS  1024u    // mirrors MAX_SPRITE_BITMAPS
#define ROAP_SPRITE_BYTES  (ROAP_SPRITE_SLOTS * ROAP_SPRITE_STRIDE)

// --- tile pool (reserved, not yet wired) -----------------------------------
#define ROAP_TILE_ADDR   (ROAP_SPRITE_ADDR + ROAP_SPRITE_BYTES)
#define ROAP_TILE_BYTES  (ROAP_MEM_SIZE - ROAP_TILE_ADDR)

#define ROAP_NKEYS      256
#define ROAP_NMOUSEBTN  8
#define ROAP_NPADS      4

// Per-frame input snapshot. Fixed-width, naturally aligned -- identical layout
// on the host (x86-64) and the guest (rv32 ilp32), both little-endian.
typedef struct {
    uint8_t  keys[ROAP_NKEYS];                 // 0/1 per USB HID scancode
    int32_t  mouseX, mouseY, mouseWheel;
    uint8_t  mouseButtons[ROAP_NMOUSEBTN];
    struct {
        uint8_t  connected;
        uint8_t  lt, rt;                       // triggers 0..255
        uint8_t  _pad;
        uint16_t buttons;                      // ROAP_PAD_* bitmask
        int16_t  lx, ly, rx, ry;               // thumbsticks -32768..32767
    } pads[ROAP_NPADS];
} RoapInput;

// Pad button masks (mirror CAKE_BUTTON_* so the guest needs no CAKE header).
#define ROAP_PAD_DPAD_UP     0x0001
#define ROAP_PAD_DPAD_DOWN   0x0002
#define ROAP_PAD_DPAD_LEFT   0x0004
#define ROAP_PAD_DPAD_RIGHT  0x0008
#define ROAP_PAD_START       0x0010
#define ROAP_PAD_BACK        0x0020
#define ROAP_PAD_LTHUMB      0x0040
#define ROAP_PAD_RTHUMB      0x0080
#define ROAP_PAD_LSHOULDER   0x0100
#define ROAP_PAD_RSHOULDER   0x0200
#define ROAP_PAD_A           0x1000
#define ROAP_PAD_B           0x2000
#define ROAP_PAD_X           0x4000
#define ROAP_PAD_Y           0x8000
