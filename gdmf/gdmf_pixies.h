#pragma once

// GDMF Pixies - BUTTOCKS pixie subsystem.
// See pixie_plans.txt (repo root) for the full design abstract.
//
// A Pixie is a programmable framebuffer unit, not a texture/sprite/tile. It
// owns private RAM and a persistent RGBA output buffer that composites into
// the frame at a given priority band, unchanged frame to frame until the
// game explicitly issues a command that touches it. "Call once, display
// forever until changed" is the defining property.
//
// Mode 0 behaves like an SVGA-style framebuffer: RAM is scratch space the
// game writes into, the output buffer is what actually displays, and
// PIXIE_OP_UNPACK moves data from one to the other. Persistent -- "call
// once, display forever" -- but PIXIE_OP_CLEAR/PLOT/DRAW each mutate the
// output buffer require a dirty-upload to the GPU image, which is a
// blocking round-trip; fine for content that rarely changes, a real cost
// for a pixie redrawn every frame.
//
// Mode 1 ("live") is the inverse tradeoff: no output buffer, no RAM-backed
// pixel state, no upload, ever. PIXIE_OP_CLEAR/PLOT/DRAW issued during a
// frame become that frame's GPU draw calls directly (built in prepare(),
// drawn in that pixie's record_band() turn, same per-frame flow sprites/
// tiles already use) and are gone -- nothing persists to the next frame,
// so a Mode 1 pixie must be redriven every frame it should show anything.
// Right choice specifically for content that was already redrawing every
// frame anyway, where Mode 0's occasional-but-blocking upload is worse
// than Mode 1's small-but-constant per-primitive cost. Deliberately NOT a
// retained command/display-list mode -- storing and replaying past
// commands starts to tread on the more Turing-complete territory reserved
// for later, opcode/VPU-execution-driven modes (see PIXIE_OP_EXECUTE
// below and VPU_Preface.md); Mode 1 remembers nothing between frames.
//
// Attributes (SET_ATTR/SHOW/HIDE) work identically in both modes. Which
// opcodes are valid in which mode(s) is decided per-opcode, not globally
// -- some opcodes may end up mode 0/1 only, others may extend to future
// higher modes; see each opcode's case in gdmf_pixies.c's PixieCommand.
// PIXIE_OP_EXECUTE is reserved for future VPU-driven modes and is a
// permanent no-op in both 0 and 1.
//
// The Pixie struct itself is private to gdmf_pixies.c, same convention as
// Sprite/TileMap -- always go through the functions below, never reach in
// directly. Every mutation (position, size, priority, enabled/shown, pixel
// data) is issued as a PixieCommand, matching the future ecall-style VPU
// bridge; the named Set*/Get* functions below are ergonomic wrappers around
// that same command path, not a separate mutation route.

#ifndef GDMF_PIXIES_H
#define GDMF_PIXIES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define GDMF_PIXIES_VERSION "0.2.2026070101 BUTTOCKS"

#define MAX_PIXIES        16         // matches MAX_TILE_LAYERS / SPRITE_PRIORITY_BANDS
#define PIXIE_RAM_SIZE    (4 * 1024 * 1024)   // 4MB per pixie, fixed at InitPixie
#define PIXIE_OUTPUT_STRING_LEN 256

// Stored per-pixie so future modes (VPU execution, etc.) can coexist and
// be dispatched on without a struct redesign. Set once at InitPixie;
// nothing currently changes it after.
typedef enum {
    PIXIE_MODE_TEXTURE = 0,  // SVGA-framebuffer: RAM + persistent RGBA output, no execution
    PIXIE_MODE_LIVE    = 1,  // no persistent output -- commands flush straight to this frame's draw calls
} PixieMode;

// Command opcodes (v1 / Mode 0 set). Fixed-size packet: opcode + flags +
// args[4], mirroring the RISC-V ecall convention (call number + argument
// registers) this is meant to grow into. Per-opcode meaning of flags/args
// is defined when that opcode is implemented -- see gdmf_pixies.c.
typedef enum {
    PIXIE_OP_SET_ATTR = 0,  // display x, y, w, h, priority, enabled
    PIXIE_OP_UNPACK,        // read packed bitmap from RAM, write into output buffer
    PIXIE_OP_DRAW,          // line primitive
    PIXIE_OP_PLOT,          // point primitive
    PIXIE_OP_CLEAR,         // fill output buffer (transparent or given color)
    PIXIE_OP_SHOW,          // enable output compositing
    PIXIE_OP_HIDE,          // disable output compositing (buffer preserved)
    PIXIE_OP_EXECUTE,       // stub -- future VPU MISL bridge; no-op/diagnostic in Mode 0
} PixieOpcode;

// PIXIE_OP_UNPACK source formats -- what PixieWrite'd data in RAM looks like
// before it's expanded into the RGBA8 output buffer. The RLE_* formats are
// produced by tools/PixiePacker (see that folder's pixiepackertool.txt for
// the exact packed-blob layout each one expects); this engine-side decoder
// must match that layout byte for byte.
typedef enum {
    PIXIE_FORMAT_RGBA8              = 0,  // raw RGBA8, no conversion
    PIXIE_FORMAT_PALETTE4BPP        = 1,  // 4bpp indexed, resolved against a GDMF palette slot
    PIXIE_FORMAT_RLE_PALETTE_SHARED = 2,  // RLE-packed indices, <=16 colors, resolved against a GDMF palette slot
    PIXIE_FORMAT_RLE_PALETTE_OWN    = 3,  // RLE-packed indices, <=256 colors, palette embedded in the blob itself
    PIXIE_FORMAT_RLE_RGBA8          = 4,  // true color, no palette -- 4 independent per-channel RLE streams
} PixieUnpackFormat;

// Compression passes applied on top of a PixieUnpackFormat's raw index/byte
// stream, in the order recorded in the packed blob's own header (see
// tools/PixiePacker/pixiepackertool.txt). This enum exists so more can be
// appended later without touching already-packed assets or this decoder's
// dispatch for existing pass IDs.
typedef enum {
    PIXIE_PASS_RLE     = 0,  // (count byte, value byte) run-length pairs
    PIXIE_PASS_HUFFMAN = 1,  // canonical Huffman, entropy-codes PIXIE_PASS_RLE's (or PIXIE_PASS_LZ's) output
    PIXIE_PASS_DELTA   = 2,  // a.k.a. URR (Unrepeated Runs): predicts each byte from the
                             // previous one in scan order and stores the residual --
                             // PIXIE_FORMAT_RLE_RGBA8's per-channel planes only, always
                             // the FIRST pass applied (wraps nothing, is wrapped by RLE/LZ)
    PIXIE_PASS_LZ      = 3,  // sliding-window back-reference matching (literal-run /
                             // match tokens) -- an ALTERNATIVE to PIXIE_PASS_RLE, never
                             // stacked with it; the packer tries both and keeps whichever
                             // compresses smaller. Strictly a superset of what RLE catches
                             // (a run of one repeated byte is just a match with offset 1),
                             // plus non-adjacent repeated patterns RLE can never see.
} PixiePackPass;

// Lifecycle
// InitPixie always allocates ram[PIXIE_RAM_SIZE] (zeroed). outputWidth/
// outputHeight set the pixie's logical coordinate space -- what PLOT/DRAW/
// CLEAR coordinates are expressed in, and (via SetPixieDisplaySize) the
// scale factor from that space into the pixie's actual display rect.
// mode == PIXIE_MODE_TEXTURE additionally allocates a real outputWidth *
// outputHeight RGBA8 pixel buffer (Mode 0's persistent framebuffer);
// mode == PIXIE_MODE_LIVE allocates none -- there is nothing to persist.
// Leaves the pixie disabled/hidden until the game issues SET_ATTR/SHOW.
// id must be in [0, MAX_PIXIES). Re-initializing a live pixie releases its
// old buffers first.
bool InitPixie(int id, PixieMode mode, int outputWidth, int outputHeight);
void ShutdownPixie(int id);
void ShutdownPixies(void);  // tears down every initialized pixie

// Command dispatch -- the only way to mutate a pixie's attrs or output
// buffer. args[4] meaning depends on opcode; see gdmf_pixies.c for the
// per-opcode layout as each is implemented. Returns false if id is invalid,
// uninitialized, or the opcode/args are malformed.
bool PixieCommand(int id, PixieOpcode opcode, uint16_t flags, const uint32_t args[4]);

// RAM access -- the only way data gets into a pixie's RAM. No raw pointer
// into engine-owned memory is ever handed back out (see GetPixieRAMSize for
// bounds-checking instead). offset+size must fit within PIXIE_RAM_SIZE.
bool   PixieWrite(int id, size_t offset, const void* data, size_t size);
size_t GetPixieRAMSize(int id);  // PIXIE_RAM_SIZE if id is initialized, 0 otherwise

// Pixie interpreters (future modes) can write status/error/return-value text
// into output_string; game code reads it back with this. Returns the number
// of bytes copied (excluding the null terminator), 0 if id is invalid.
size_t PixieReadString(int id, char* buf, size_t maxlen);

// Ergonomic wrappers -- each one is a thin PixieCommand(PIXIE_OP_SET_ATTR/
// SHOW/HIDE, ...) call under the hood, not a separate mutation path. Exist
// so game code doesn't have to hand-build command packets for the common
// case of moving/resizing/showing a pixie.
bool SetPixiePosition(int id, int x, int y);
bool SetPixieDisplaySize(int id, int w, int h);
bool SetPixiePriority(int id, unsigned char priority);
bool SetPixieEnabled(int id, bool enabled);
bool ShowPixie(int id);
bool HidePixie(int id);

// Read-only accessors. The struct backing these is private to
// gdmf_pixies.c, same convention as Sprite/TileMap -- these are the only
// way to read a pixie's current state back out.
bool           GetPixieInitialized(int id);
PixieMode      GetPixieMode(int id);
int            GetPixieOutputWidth(int id);
int            GetPixieOutputHeight(int id);
int            GetPixieX(int id);
int            GetPixieY(int id);
int            GetPixieDisplayWidth(int id);
int            GetPixieDisplayHeight(int id);
unsigned char  GetPixiePriority(int id);
bool           GetPixieEnabled(int id);
bool           GetPixieShown(int id);

#endif // GDMF_PIXIES_H