// Linux builds with strict -std=c11: clock_gettime must be requested
// before any header lands (macOS exposes it unasked, so this is Linux-only
// by necessity, harmless there).
#if defined(__linux__)
#define _POSIX_C_SOURCE 200809L
#endif

#include "VPU_roap.h"
#include "vpu.h"
#include "roap_abi.h"
#include "roap_mmio.h"
#include "fuselage.h"                 // FuselageSignal / mouse / vsync / fps; pulls windows.h (QPC)
#include <time.h>                     // clock_gettime on non-Windows (see roap_now_seconds)
#include "GDMF/textlayer/gdmf_textlayer.h"
#include "GDMF/sprites/gdmf_sprites.h"
#include "GDMF/sprites/gdmf_sprites_help.h"   // the delta ecalls (MoveSpriteBy, ...)
#include "GDMF/tiles/gdmf_tiles.h"
#include "GDMF/tiles/gdmf_tiles_help.h"       // ScrollTileMap
#include "GDMF/pixies/gdmf_pixies.h"
#include "GDMF/gdmf_interactions.h"
#include "GDMF/gdmf_colors.h"
#include "CAKE/cake.h"
#include "DICE/dice_timers.h"
#include "DICE/dice_rng.h"
#include "DICE/dice_help.h"
#include <string.h>

// ---------------------------------------------------------------------------
// The dispatch core + the full Fuselage ecall surface (see ROAP/roap_abi.h).
//
// A single VPU context lives here as a file-static singleton. ServiceEcall
// routes on the subsystem byte of a7 to a per-subsystem sub-dispatcher; each
// case marshals the a0..a6 arguments per the ABI and calls the real Fuselage
// function. Handlers return 1 to keep running within the slice, 0 to yield.
// ---------------------------------------------------------------------------

// ROAP_MEM_SIZE and the rest of the memory map live in roap_mmio.h -- the guest
// needs the same numbers, and there is only one map.
//
// Per-slice budget: a WALL-CLOCK cap, not a step count. A cooperative guest
// yields (SYS_YIELD/WAITVBL/SLEEP) when its frame is done and returns long
// before this -- so a legitimate heavy frame (DriftingPlots at its ceiling)
// runs to completion in one slice however long it takes. The cap only bounds a
// RUNAWAY (a guest that never yields). Being wall-clock, it means the same real
// time regardless of interpreter build (-O0 vs -O2), instruction mix (soft-float
// vs integer), or sim rate -- the thing a fixed step count could never promise,
// which is why the old 12M-step "budget" was a stopgap.
#define ROAP_SLICE_SECONDS   0.25       // runaway cap: ~a quarter second, no more
#define ROAP_STEP_CHUNK      200000u    // steps executed between wall-clock checks
#define ROAP_STR_MAX     512          // max bytes copied out of VPU mem per string

#define SPR_DIRTY_WORDS ((ROAP_SPRITE_SLOTS + 31u) / 32u)

typedef enum { ROAP_UNLOADED, ROAP_RUNNING, ROAP_HALTED } MonitorState;

static struct {
    VPUState      vpu;
    uint8_t       mem[ROAP_MEM_SIZE];
    MonitorState  state;
    const ROAP   *loaded;
    // One bit per sprite slot the guest has written to since the last flush.
    // A bitset, not a flag: re-staging all 1024 slots because one changed would
    // be 2 MiB of upload for 2 KiB of news.
    uint32_t      sprDirty[SPR_DIRTY_WORDS];
    int           sprAnyDirty;
} g;

enum { REG_SP = 2, REG_A0 = 10, REG_A7 = 17 };

// --- argument / return marshalling (ABI: a0..a6 args, a0[:a1] return) ------
static uint32_t A(int i)            { return g.vpu.regs[REG_A0 + i]; }
static void     R(uint32_t v)       { g.vpu.regs[REG_A0] = v; }
static void     R64(uint64_t v)     { g.vpu.regs[REG_A0] = (uint32_t)v;
                                      g.vpu.regs[REG_A0 + 1] = (uint32_t)(v >> 32); }
static float    Af(int i)           { uint32_t b = A(i); float f;  memcpy(&f, &b, sizeof f); return f; }
static void     Rf(float f)         { uint32_t b;        memcpy(&b, &f, sizeof b); R(b); }
static double   Ad(int i)           { uint64_t b = (uint64_t)A(i) | ((uint64_t)A(i + 1) << 32);
                                      double d; memcpy(&d, &b, sizeof d); return d; }
static void     Rd(double d)        { uint64_t b; memcpy(&b, &d, sizeof b); R64(b); }
static uint64_t A64(int i)          { return (uint64_t)A(i) | ((uint64_t)A(i + 1) << 32); }
static Color    Acol(int i)         { uint32_t p = A(i);
                                      Color c = { (unsigned char)p, (unsigned char)(p >> 8),
                                                  (unsigned char)(p >> 16), (unsigned char)(p >> 24) };
                                      return c; }
static void     Rcol(Color c)       { R(PackRGBA8(c)); }

// Bounds-checked pointer into VPU memory. Vptr requires [addr,addr+len); Vptr0
// only checks the start is in range (for variable-length reads whose length
// the host can't know here -- a gap the future MMIO model will close).
static void *Vptr(uint32_t addr, uint32_t len) {
    return ((uint64_t)addr + len <= g.vpu.memSize) ? (g.mem + addr) : NULL;
}
static void *Vptr0(uint32_t addr) { return (addr < g.vpu.memSize) ? (g.mem + addr) : NULL; }

static uint32_t ReadVPUString(uint32_t addr, char *dst, uint32_t cap) {
    uint32_t n = 0;
    while (n + 1 < cap && (uint64_t)addr + n < g.vpu.memSize) {
        char c = (char)g.vpu.mem[addr + n];
        if (c == '\0') break;
        dst[n++] = c;
    }
    dst[n] = '\0';
    return n;
}

static int ecall_unknown(uint32_t call) {
    (void)call;
    tlPrintC("[VPU] unknown/unimplemented ecall -- halted\n", RED);
    g.state = ROAP_HALTED;
    return 0;
}

// --- 0x01 SYS --------------------------------------------------------------
static int service_sys(uint32_t call) {
    switch (call) {
    case ROAP_SYS_SIGNAL:              FuselageSignal((FuselageSignalType)A(0)); return 1;
    case ROAP_SYS_WAITVBL:
    case ROAP_SYS_YIELD:
    case ROAP_SYS_SLEEP:               return 0;   // yielders: end this slice
    case ROAP_SYS_GET_FPS:             Rf(FuselageGetCurrentFPS()); return 1;
    case ROAP_SYS_SET_MOUSECAPTURE:    FuselageSetMouseCapture(A(0) != 0); return 1;
    case ROAP_SYS_GET_MOUSECAPTURE:    R(FuselageGetMouseCapture() ? 1u : 0u); return 1;
    case ROAP_SYS_TOGGLE_MOUSECAPTURE: R(FuselageToggleMouseCapture() ? 1u : 0u); return 1;
    case ROAP_SYS_SET_CURSORVISIBLE:   FuselageSetCursorVisible(A(0) != 0); return 1;
    case ROAP_SYS_GET_CURSORVISIBLE:   R(FuselageGetCursorVisible() ? 1u : 0u); return 1;
    case ROAP_SYS_SET_VSYNC:           FuselageSetVSync(A(0) != 0); return 1;
    case ROAP_SYS_GET_VSYNC:           R(FuselageGetVSync() ? 1u : 0u); return 1;
    case ROAP_SYS_SET_TARGETFPS:       FuselageSetTargetFPS(Ad(0)); return 1;
    case ROAP_SYS_GET_TARGETFPS:       Rd(FuselageGetTargetFPS()); return 1;
    case ROAP_SYS_GET_SIMRATE:         Rd(FuselageGetSimRate()); return 1;
    default:                           return ecall_unknown(call);
    }
}

// --- 0x10 TL (text layer) --------------------------------------------------
static int service_tl(uint32_t call) {
    char buf[ROAP_STR_MAX];
    switch (call) {
    case ROAP_TL_PRINT:        ReadVPUString(A(0), buf, sizeof buf); R((uint32_t)tlPrint(buf)); return 1;
    case ROAP_TL_PRINT_C:      ReadVPUString(A(0), buf, sizeof buf); R((uint32_t)tlPrintC(buf, Acol(1))); return 1;
    case ROAP_TL_PRINT_CP:     ReadVPUString(A(0), buf, sizeof buf); R((uint32_t)tlPrintCP(buf, Acol(1), (int)A(2), (int)A(3))); return 1;
    case ROAP_TL_PRINTCHAR:    R((uint32_t)tlPrintChar((unsigned char)A(0))); return 1;
    case ROAP_TL_PRINTCHAR_C:  R((uint32_t)tlPrintCharC((unsigned char)A(0), Acol(1))); return 1;
    case ROAP_TL_PRINTCHAR_CP: R((uint32_t)tlPrintCharCP((unsigned char)A(0), Acol(1), (int)A(2), (int)A(3))); return 1;
    case ROAP_TL_PRINTINT:     R((uint32_t)tlPrintInt((int)A(0))); return 1;
    case ROAP_TL_PRINTINT_C:   R((uint32_t)tlPrintIntC((int)A(0), Acol(1))); return 1;
    case ROAP_TL_PRINTINT_CP:  R((uint32_t)tlPrintIntCP((int)A(0), Acol(1), (int)A(2), (int)A(3))); return 1;
    case ROAP_TL_NEWLINE:      R((uint32_t)tlNewLine()); return 1;
    case ROAP_TL_CLS:          tlCLS(); return 1;
    case ROAP_TL_HOME:         tlHome(); return 1;
    case ROAP_TL_SET_COLOR:    tlSetColor(Acol(0)); return 1;
    case ROAP_TL_GET_COLOR:    Rcol(tlGetColor()); return 1;
    case ROAP_TL_SET_CURSOR:   tlSetCursor((int)A(0), (int)A(1)); return 1;
    case ROAP_TL_GET_CURSOR:   R((uint32_t)tlGetCursor()); return 1;
    case ROAP_TL_GET_CURSOR_X: R((uint32_t)tlGetCursorX()); return 1;
    case ROAP_TL_GET_CURSOR_Y: R((uint32_t)tlGetCursorY()); return 1;
    case ROAP_TL_SCROLL_UP:    R((uint32_t)tlScrollUp()); return 1;
    case ROAP_TL_ACTIVE:       R(tlActivate() ? 1u : 0u); return 1;
    case ROAP_TL_INACTIVE:     R(tlDeactivate() ? 1u : 0u); return 1;
    case ROAP_TL_TOGGLE:       R(tlToggle() ? 1u : 0u); return 1;
    case ROAP_TL_STATUS:       R(tlStatus() ? 1u : 0u); return 1;
    default:                   return ecall_unknown(call);
    }
}

// --- 0x11 SPR (sprites) ----------------------------------------------------
static int service_spr(uint32_t call) {
    switch (call) {
    case ROAP_SPR_UPLOAD_BITMAP:    { const unsigned char *p = Vptr(A(1), 2048); R((p && UploadSpriteBitmap((SpriteBitmapID)A(0), p)) ? 1u : 0u); return 1; }
    case ROAP_SPR_ASSIGN:           R(AssignSprite((int)A(0), (SpriteBitmapID)A(1)) ? 1u : 0u); return 1;
    case ROAP_SPR_ASSIGN_FROM:      R(AssignSpriteBitmapFromSprite((int)A(0), (int)A(1)) ? 1u : 0u); return 1;
    case ROAP_SPR_GET_BITMAP_ID:    R((uint32_t)(int)GetSpriteBitmapID((int)A(0))); return 1;
    case ROAP_SPR_CLEAR:            ClearSprite((int)A(0)); return 1;
    case ROAP_SPR_TEST_PATTERN:     AssignSpriteTestPattern((int)A(0)); return 1;
    case ROAP_SPR_GET_ENABLED:      R(GetSpriteEnabled((int)A(0)) ? 1u : 0u); return 1;
    case ROAP_SPR_SET_ENABLED:      R(SetSpriteEnabled((int)A(0), A(1) != 0) ? 1u : 0u); return 1;
    case ROAP_SPR_TOGGLE_ENABLED:   R(ToggleSpriteEnabled((int)A(0)) ? 1u : 0u); return 1;
    case ROAP_SPR_GET_VISIBLE:      R(GetSpriteVisible((int)A(0)) ? 1u : 0u); return 1;
    case ROAP_SPR_SET_VISIBLE:      R(SetSpriteVisible((int)A(0), A(1) != 0) ? 1u : 0u); return 1;
    case ROAP_SPR_TOGGLE_VISIBLE:   R(ToggleSpriteVisible((int)A(0)) ? 1u : 0u); return 1;
    case ROAP_SPR_SET_POSITION:     SetSpritePosition((int)A(0), Af(1), Af(2)); return 1;
    case ROAP_SPR_UPDATE_POSITION:  MoveSpriteBy((int)A(0), Af(1), Af(2)); return 1;
    case ROAP_SPR_GET_X:            Rf(GetSpriteX((int)A(0))); return 1;
    case ROAP_SPR_GET_Y:            Rf(GetSpriteY((int)A(0))); return 1;
    case ROAP_SPR_SET_SCALE:        SetSpriteScale((int)A(0), Af(1)); return 1;
    case ROAP_SPR_GET_SCALE:        Rf(GetSpriteScale((int)A(0))); return 1;
    case ROAP_SPR_CHANGE_SCALE:     R(ScaleSpriteBy((int)A(0), Af(1)) ? 1u : 0u); return 1;
    case ROAP_SPR_SET_ROTATION:     SetSpriteRotation((int)A(0), Af(1)); return 1;
    case ROAP_SPR_GET_ROTATION:     Rf(GetSpriteRotation((int)A(0))); return 1;
    case ROAP_SPR_CHANGE_ROTATION:  R(RotateSpriteBy((int)A(0), Af(1)) ? 1u : 0u); return 1;
    case ROAP_SPR_SET_SKEW:         SetSpriteSkew((int)A(0), Af(1), Af(2)); return 1;
    case ROAP_SPR_GET_SKEW:         { float *sx = Vptr(A(1), 4), *sy = Vptr(A(2), 4); if (sx && sy) GetSpriteSkew((int)A(0), sx, sy); return 1; }
    case ROAP_SPR_SET_HOTSPOT:      R(SetSpriteHotspot((int)A(0), Af(1), Af(2)) ? 1u : 0u); return 1;
    case ROAP_SPR_GET_HOTSPOT:      { float *hx = Vptr(A(1), 4), *hy = Vptr(A(2), 4); if (hx && hy) GetSpriteHotspot((int)A(0), hx, hy); return 1; }
    case ROAP_SPR_SET_ROT_HOTSPOT:  R(SetSpriteRotateAroundHotspot((int)A(0), A(1) != 0) ? 1u : 0u); return 1;
    case ROAP_SPR_GET_ROT_HOTSPOT:  R(GetSpriteRotateAroundHotspot((int)A(0)) ? 1u : 0u); return 1;
    case ROAP_SPR_SET_FLIP:         R((uint32_t)SetSpriteFlip((int)A(0), (unsigned char)A(1))); return 1;
    case ROAP_SPR_GET_FLIP:         R((uint32_t)GetSpriteFlip((int)A(0))); return 1;
    case ROAP_SPR_SET_PRIORITY:     SetSpritePriority((int)A(0), (unsigned char)A(1)); return 1;
    case ROAP_SPR_GET_PRIORITY:     R((uint32_t)GetSpritePriority((int)A(0))); return 1;
    case ROAP_SPR_SET_PALETTE:      R(SetSpriteColorPalette((int)A(0), (unsigned char)A(1)) ? 1u : 0u); return 1;
    case ROAP_SPR_GET_PALETTE:      R((uint32_t)GetSpriteColorPalette((int)A(0))); return 1;
    case ROAP_SPR_SET_TRANSPARENCY: SetSpriteTransparency((int)A(0), (unsigned char)A(1)); return 1;
    case ROAP_SPR_GET_TRANSPARENCY: R((uint32_t)GetSpriteTransparency((int)A(0))); return 1;
    case ROAP_SPR_SHOW_ZERO:        SetSpriteShowZero((int)A(0), A(1) != 0); return 1;
    case ROAP_SPR_SET_COLLIDABLE:   R((uint32_t)SetSpriteCollidableColors((int)A(0), (unsigned short)A(1))); return 1;
    case ROAP_SPR_GET_COLLIDABLE:   R((uint32_t)GetSpriteCollidableColors((int)A(0))); return 1;
    case ROAP_SPR_SET_COLLTYPES:    R((uint32_t)SetSpriteCollisionTypes((int)A(0), (unsigned char)A(1))); return 1;
    case ROAP_SPR_GET_COLLTYPES:    R((uint32_t)GetSpriteCollisionTypes((int)A(0))); return 1;
    case ROAP_SPR_HAS_COLLISION:    R(SpriteHasCollision((int)A(0)) ? 1u : 0u); return 1;
    case ROAP_SPR_GET_COLL_COUNT:   R((uint32_t)GetSpriteCollisionCount((int)A(0))); return 1;
    case ROAP_SPR_COPY_COLLISIONS: {
        int cnt = 0;
        const SpriteCollisionInfo *ci = GetSpriteCollisions((int)A(0), &cnt);
        uint32_t maxc = A(2);
        if ((uint32_t)cnt < maxc) maxc = (uint32_t)cnt;
        void *dst = Vptr(A(1), maxc * (uint32_t)sizeof(SpriteCollisionInfo));
        if (dst && ci) memcpy(dst, ci, maxc * sizeof(SpriteCollisionInfo));
        R(dst ? maxc : 0u);
        return 1;
    }
    case ROAP_SPR_CHECK_PAIR:        R((uint32_t)CheckSpritePairCollision((int)A(0), (int)A(1), (unsigned char)A(2))); return 1;
    case ROAP_SPR_WORLD_POINT_ON:    R(WorldPointOnSprite((int)A(0), Af(1), Af(2)) ? 1u : 0u); return 1;
    case ROAP_SPR_TOGGLE_ATLAS:      ToggleSpriteAtlasView(); return 1;
    case ROAP_SPR_GET_ATLAS_ACTIVE:  R(GetSpriteAtlasViewActive() ? 1u : 0u); return 1;
    case ROAP_SPR_GET_RENDERED_COUNT: R((uint32_t)GetRenderedSpriteCount()); return 1;
    default:                         return ecall_unknown(call);
    }
}

// --- 0x12 TILE -------------------------------------------------------------
static int service_tile(uint32_t call) {
    switch (call) {
    case ROAP_TILE_INIT:             R(InitTileLayer((uint8_t)A(0), (uint16_t)A(1), (uint16_t)A(2), (uint16_t)A(3), (uint16_t)A(4), (uint16_t)A(5), Af(6)) ? 1u : 0u); return 1;
    case ROAP_TILE_RELEASE:          R(ReleaseTileLayer((uint8_t)A(0)) ? 1u : 0u); return 1;
    case ROAP_TILE_UPLOAD_BITMAP:    { const unsigned char *p = Vptr0(A(2)); R((p && UploadTileBitmap((uint8_t)A(0), (int)A(1), p)) ? 1u : 0u); return 1; }
    case ROAP_TILE_GET_VISIBLE:      R(GetTileLayerVisible((uint8_t)A(0)) ? 1u : 0u); return 1;
    case ROAP_TILE_SET_VISIBLE:      R(SetTileLayerVisible((uint8_t)A(0), A(1) != 0) ? 1u : 0u); return 1;
    case ROAP_TILE_TOGGLE_VISIBLE:   R(ToggleTileLayerVisible((uint8_t)A(0)) ? 1u : 0u); return 1;
    case ROAP_TILE_GET_ENABLED:      R(GetTileLayerEnabled((uint8_t)A(0)) ? 1u : 0u); return 1;
    case ROAP_TILE_SET_ENABLED:      R(SetTileLayerEnabled((uint8_t)A(0), A(1) != 0) ? 1u : 0u); return 1;
    case ROAP_TILE_TOGGLE_ENABLED:   R(ToggleTileLayerEnabled((uint8_t)A(0)) ? 1u : 0u); return 1;
    case ROAP_TILE_PLACE:            R(PlaceTile((uint8_t)A(0), (uint16_t)A(1), (uint16_t)A(2), (uint16_t)A(3)) ? 1u : 0u); return 1;
    case ROAP_TILE_FILL:             R(FillTileLayer((uint8_t)A(0), (uint16_t)A(1)) ? 1u : 0u); return 1;
    case ROAP_TILE_SET_CELL_PALETTE: R(SetTileCellPalette((uint8_t)A(0), (uint16_t)A(1), (uint16_t)A(2), (uint8_t)A(3)) ? 1u : 0u); return 1;
    case ROAP_TILE_SET_PALETTE:      R(SetTileLayerPalette((uint8_t)A(0), (uint8_t)A(1)) ? 1u : 0u); return 1;
    case ROAP_TILE_SET_CELL_TRANSP:  R(SetTileCellTransparency((uint8_t)A(0), (uint16_t)A(1), (uint16_t)A(2), (uint8_t)A(3)) ? 1u : 0u); return 1;
    case ROAP_TILE_SET_TRANSP:       R(SetTileLayerTransparency((uint8_t)A(0), (uint8_t)A(1)) ? 1u : 0u); return 1;
    case ROAP_TILE_SET_CELL_SHOWZERO:R(SetTileCellShowZero((uint8_t)A(0), (uint16_t)A(1), (uint16_t)A(2), A(3) != 0) ? 1u : 0u); return 1;
    case ROAP_TILE_SET_SHOWZERO:     R(SetTileLayerShowZero((uint8_t)A(0), A(1) != 0) ? 1u : 0u); return 1;
    case ROAP_TILE_SET_FLIP:         R(SetTileFlip((uint8_t)A(0), (uint16_t)A(1), (uint16_t)A(2), A(3) != 0, A(4) != 0) ? 1u : 0u); return 1;
    case ROAP_TILE_SET_CELL_COLLISION: SetTileCellCollision((uint8_t)A(0), (uint16_t)A(1), (uint16_t)A(2), A(3) != 0); return 1;
    case ROAP_TILE_SET_WRAPPING:     R(SetTileMapWrapping((uint8_t)A(0), A(1) != 0, A(2) != 0) ? 1u : 0u); return 1;
    case ROAP_TILE_SET_SCALE:        R(SetTileLayerScale((uint8_t)A(0), Af(1)) ? 1u : 0u); return 1;
    case ROAP_TILE_SET_VIEWPORT:     R(SetTileViewport((uint8_t)A(0), (int)A(1), (int)A(2), (int)A(3), (int)A(4)) ? 1u : 0u); return 1;  /* x,y signed: may be off-canvas */
    case ROAP_TILE_SCROLL:           R(ScrollTileMap((uint8_t)A(0), Ad(1), Ad(3)) ? 1u : 0u); return 1;
    case ROAP_TILE_SET_MAP_OFFSET:   R(SetTileMapOffset((uint8_t)A(0), Ad(1), Ad(3)) ? 1u : 0u); return 1;
    case ROAP_TILE_SET_TILE_OFFSET:  R(SetTileOffset((uint8_t)A(0), (uint16_t)A(1), (uint16_t)A(2), (int8_t)A(3), (int8_t)A(4)) ? 1u : 0u); return 1;
    case ROAP_TILE_SET_COLLIDABLE:   SetTileLayerCollidableColors((uint8_t)A(0), (uint16_t)A(1)); return 1;
    case ROAP_TILE_GET_COLLIDABLE:   R((uint32_t)GetTileLayerCollidableColors((uint8_t)A(0))); return 1;
    case ROAP_TILE_TEST_PATTERN:     UploadTileTestPattern((uint8_t)A(0), (int)A(1)); return 1;
    case ROAP_TILE_BOX_PATTERN:      UploadTileBoxPattern((uint8_t)A(0), (int)A(1)); return 1;
    case ROAP_TILE_TOGGLE_ATLAS:     ToggleTileAtlasView((uint8_t)A(0)); return 1;
    case ROAP_TILE_GET_ATLAS_ACTIVE: R(GetTileAtlasViewActive((uint8_t)A(0)) ? 1u : 0u); return 1;
    case ROAP_TILE_SET_PRIORITY:     R(SetTileLayerPriority((uint8_t)A(0), (unsigned char)A(1)) ? 1u : 0u); return 1;
    case ROAP_TILE_GET_PRIORITY:     R((uint32_t)GetTileLayerPriority((uint8_t)A(0))); return 1;
    default:                         return ecall_unknown(call);
    }
}

// --- 0x13 PIX (pixies) -----------------------------------------------------
static int service_pix(uint32_t call) {
    switch (call) {
    case ROAP_PIX_INIT:             R(InitPixie((int)A(0), (PixieMode)A(1), (int)A(2), (int)A(3)) ? 1u : 0u); return 1;
    case ROAP_PIX_SHUTDOWN:         ReleasePixie((int)A(0)); return 1;
    case ROAP_PIX_COMMAND:          { const uint32_t *args = Vptr(A(3), 4 * sizeof(uint32_t)); R((args && IssuePixieCommand((int)A(0), (PixieOpcode)A(1), (uint16_t)A(2), args)) ? 1u : 0u); return 1; }
    case ROAP_PIX_WRITE:            { const void *p = Vptr(A(2), A(3)); R((p && WritePixieRAM((int)A(0), (size_t)A(1), p, (size_t)A(3))) ? 1u : 0u); return 1; }
    case ROAP_PIX_GET_RAM_SIZE:     R((uint32_t)GetPixieRAMSize((int)A(0))); return 1;
    case ROAP_PIX_READ_STRING:      { char *dst = Vptr(A(1), A(2)); R(dst ? (uint32_t)ReadPixieString((int)A(0), dst, (size_t)A(2)) : 0u); return 1; }
    case ROAP_PIX_SET_POSITION:     R(SetPixiePosition((int)A(0), (int)A(1), (int)A(2)) ? 1u : 0u); return 1;
    case ROAP_PIX_SET_DISPLAY_SIZE: R(SetPixieDisplaySize((int)A(0), (int)A(1), (int)A(2)) ? 1u : 0u); return 1;
    case ROAP_PIX_SET_PRIORITY:     R(SetPixiePriority((int)A(0), (unsigned char)A(1)) ? 1u : 0u); return 1;
    case ROAP_PIX_SET_ENABLED:      R(SetPixieEnabled((int)A(0), A(1) != 0) ? 1u : 0u); return 1;
    case ROAP_PIX_SHOW:             R(ShowPixie((int)A(0)) ? 1u : 0u); return 1;
    case ROAP_PIX_HIDE:             R(HidePixie((int)A(0)) ? 1u : 0u); return 1;
    case ROAP_PIX_SET_DRAW_PATTERN: R(SetPixieDrawPattern((int)A(0), (uint16_t)A(1)) ? 1u : 0u); return 1;
    case ROAP_PIX_GET_DRAW_PATTERN: R((uint32_t)GetPixieDrawPattern((int)A(0))); return 1;
    case ROAP_PIX_GET_INITIALIZED:  R(GetPixieInitialized((int)A(0)) ? 1u : 0u); return 1;
    case ROAP_PIX_GET_MODE:         R((uint32_t)GetPixieMode((int)A(0))); return 1;
    case ROAP_PIX_GET_OUT_WIDTH:    R((uint32_t)GetPixieOutputWidth((int)A(0))); return 1;
    case ROAP_PIX_GET_OUT_HEIGHT:   R((uint32_t)GetPixieOutputHeight((int)A(0))); return 1;
    case ROAP_PIX_GET_X:            R((uint32_t)GetPixieX((int)A(0))); return 1;
    case ROAP_PIX_GET_Y:            R((uint32_t)GetPixieY((int)A(0))); return 1;
    case ROAP_PIX_GET_DISPLAY_WIDTH: R((uint32_t)GetPixieDisplayWidth((int)A(0))); return 1;
    case ROAP_PIX_GET_DISPLAY_HEIGHT: R((uint32_t)GetPixieDisplayHeight((int)A(0))); return 1;
    case ROAP_PIX_GET_PRIORITY:     R((uint32_t)GetPixiePriority((int)A(0))); return 1;
    case ROAP_PIX_GET_ENABLED:      R(GetPixieEnabled((int)A(0)) ? 1u : 0u); return 1;
    case ROAP_PIX_GET_SHOWN:        R(GetPixieShown((int)A(0)) ? 1u : 0u); return 1;
    default:                        return ecall_unknown(call);
    }
}

// --- 0x14 PAL (colors / palettes) ------------------------------------------
static int service_pal(uint32_t call) {
    switch (call) {
    case ROAP_PAL_PACK_RGBA8:      R(PackRGBA8(Acol(0))); return 1;
    case ROAP_PAL_LOAD_FROM_SPRITE:{ Color *pal = Vptr(A(1), 16 * (uint32_t)sizeof(Color)); R((pal && SetColorsPalette((unsigned char)A(0), pal)) ? 1u : 0u); return 1; }
    case ROAP_PAL_GET_FROM_CHAR:   Rcol(GetColorsColorFromChar((unsigned char)A(0))); return 1;
    case ROAP_PAL_GET_COMMODORE:   Rcol(GetColorsCommodoreColor((unsigned char)A(0))); return 1;
    case ROAP_PAL_GET_TANDY:       Rcol(GetColorsTandyColor((unsigned char)A(0))); return 1;
    case ROAP_PAL_GET_ANSI:        Rcol(GetColorsANSIColor((unsigned char)A(0))); return 1;
    case ROAP_PAL_SET:             R(SetColorsPaletteColor((unsigned char)A(0), (unsigned char)A(1), Acol(2)) ? 1u : 0u); return 1;
    case ROAP_PAL_GET:             Rcol(GetColorsPaletteColor((unsigned char)A(0), (unsigned char)A(1))); return 1;
    default:                       return ecall_unknown(call);
    }
}

// --- 0x15 IACT (interactions) ----------------------------------------------
static int service_iact(uint32_t call) {
    switch (call) {
    case ROAP_IACT_DIST_SPRITES:      Rf(DistanceBetweenSprites((int)A(0), (int)A(1))); return 1;
    case ROAP_IACT_DIR_SPRITES:       Rf(DirectionBetweenSprites((int)A(0), (int)A(1))); return 1;
    case ROAP_IACT_LOCALPIX_TO_WORLD: { float *wx = Vptr(A(3), 4), *wy = Vptr(A(4), 4); R((wx && wy && LocalBitmapPixelToWorldPixel((int)A(0), (int)A(1), (int)A(2), wx, wy)) ? 1u : 0u); return 1; }
    default:                          return ecall_unknown(call);
    }
}

// --- 0x20 CAKE (input) -----------------------------------------------------
static int service_cake(uint32_t call) {
    switch (call) {
    case ROAP_CAKE_GET_KEY:          { uint32_t sc = A(0); R((sc < CAKE_KEY_TABLE_SIZE && CAKE_Keys[sc]) ? 1u : 0u); return 1; }
    case ROAP_CAKE_GET_MOUSE_X:      R((uint32_t)CAKE_MouseDeltaX); return 1;
    case ROAP_CAKE_GET_MOUSE_Y:      R((uint32_t)CAKE_MouseDeltaY); return 1;
    case ROAP_CAKE_GET_MOUSE_WHEEL:  R((uint32_t)CAKE_MouseWheel); return 1;
    case ROAP_CAKE_GET_MOUSE_BUTTON: { uint32_t b = A(0); R((b < CAKE_MOUSE_BUTTON_COUNT && CAKE_MouseButtons[b]) ? 1u : 0u); return 1; }
    case ROAP_CAKE_IS_SILENCED:      R((uint32_t)CAKE_IsSilenced()); return 1;
    case ROAP_CAKE_CTRL_CONNECTED:   R((uint32_t)CAKE_IsControllerConnected((int)A(0))); return 1;
    case ROAP_CAKE_CTRL_CONN_STATE:  R((uint32_t)CAKE_GetControllerConnectionState((int)A(0))); return 1;
    case ROAP_CAKE_CTRL_BACKEND:     R((uint32_t)CAKE_GetControllerBackend((int)A(0))); return 1;
    case ROAP_CAKE_CTRL_VENDOR:      R((uint32_t)CAKE_GetControllerVendorID((int)A(0))); return 1;
    case ROAP_CAKE_CTRL_PRODUCT:     R((uint32_t)CAKE_GetControllerProductID((int)A(0))); return 1;
    case ROAP_CAKE_CTRL_XINPUT_IDX:  R((uint32_t)CAKE_GetControllerXInputIndex((int)A(0))); return 1;
    case ROAP_CAKE_CTRL_VIBRATION:   R((uint32_t)CAKE_SetControllerVibration((int)A(0), (uint16_t)A(1), (uint16_t)A(2))); return 1;
    case ROAP_CAKE_CTRL_COPY_NAME: {
        const char *nm = CAKE_GetControllerName((int)A(0));
        uint32_t cap = A(2);
        char *dst = Vptr(A(1), cap);
        uint32_t n = 0;
        if (dst && nm) { while (n + 1 < cap && nm[n]) { dst[n] = nm[n]; n++; } dst[n] = '\0'; }
        R(n);
        return 1;
    }
    default:                         return ecall_unknown(call);
    }
}

// --- 0x40 DICE (RNG / timers / sequences) ----------------------------------
static int service_dice(uint32_t call) {
    switch (call) {
    case ROAP_DICE_INIT_RNG:        R(DICE_InitRNG((uint8_t)A(0), (DICE_RNGMode)A(1), A64(2)) ? 1u : 0u); return 1;
    case ROAP_DICE_RELEASE_RNG:     DICE_ReleaseRNG((uint8_t)A(0)); return 1;
    case ROAP_DICE_RNG_MIX_ENTROPY: { const void *p = Vptr(A(1), A(2)); if (p) DICE_MixRNGEntropy((uint8_t)A(0), p, (size_t)A(2)); return 1; }
    case ROAP_DICE_RNG_GET_SEED:    R64(DICE_GetRNGSeed((uint8_t)A(0))); return 1;
    case ROAP_DICE_RAND_UINT:       R(DICE_RandUint((uint8_t)A(0))); return 1;
    case ROAP_DICE_RAND_INT:        R((uint32_t)DICE_RandInt((uint8_t)A(0), (int)A(1), (int)A(2))); return 1;
    case ROAP_DICE_RAND_FLOAT:      Rf(DICE_RandFloat((uint8_t)A(0))); return 1;
    case ROAP_DICE_RAND_CHANCE:     R(DICE_RandChance((uint8_t)A(0), Af(1)) ? 1u : 0u); return 1;
    case ROAP_DICE_INIT_TIMER:      R(DICE_InitTimerInterval((uint8_t)A(0), A64(1), A(3) != 0, NULL, NULL) ? 1u : 0u); return 1;
    case ROAP_DICE_RELEASE_TIMER:   DICE_ReleaseTimer((uint8_t)A(0)); return 1;
    case ROAP_DICE_TIMER_START:     R(DICE_StartTimer((uint8_t)A(0)) ? 1u : 0u); return 1;
    case ROAP_DICE_TIMER_STOP:      R(DICE_StopTimer((uint8_t)A(0)) ? 1u : 0u); return 1;
    case ROAP_DICE_TIMER_PAUSE:     R(DICE_PauseTimer((uint8_t)A(0)) ? 1u : 0u); return 1;
    case ROAP_DICE_TIMER_RESUME:    R(DICE_ResumeTimer((uint8_t)A(0)) ? 1u : 0u); return 1;
    case ROAP_DICE_TIMER_IS_RUNNING:R(DICE_IsTimerRunning((uint8_t)A(0)) ? 1u : 0u); return 1;
    case ROAP_DICE_TIMER_CONSUME:   R(DICE_ConsumeTimer((uint8_t)A(0))); return 1;
    case ROAP_DICE_TIMER_DELTA:     Rd(DICE_TimerDelta((uint8_t)A(0))); return 1;
    case ROAP_DICE_SEED_DIE:        R(DICE_SeedDie((uint8_t)A(0), A64(1)) ? 1u : 0u); return 1;
    case ROAP_DICE_DIE_RANGE:       R((uint32_t)DICE_DieRange((uint8_t)A(0), (int)A(1), (int)A(2))); return 1;
    case ROAP_DICE_DIE_FLOAT:       Rf(DICE_DieFloat((uint8_t)A(0))); return 1;
    case ROAP_DICE_DIE_CHANCE:      R(DICE_DieChance((uint8_t)A(0), Af(1)) ? 1u : 0u); return 1;
    case ROAP_DICE_SEED_SEQUENCE:   R(DICE_SeedSequence((uint8_t)A(0), A64(1)) ? 1u : 0u); return 1;
    case ROAP_DICE_SEQ_UINT:        R(DICE_SequenceUint((uint8_t)A(0), A64(1))); return 1;
    case ROAP_DICE_SEQ_INT:         R((uint32_t)DICE_SequenceInt((uint8_t)A(0), A64(1), (int)A(3), (int)A(4))); return 1;
    case ROAP_DICE_SEQ_FLOAT:       Rf(DICE_SequenceFloat((uint8_t)A(0), A64(1))); return 1;
    case ROAP_DICE_SEQ_CHANCE:      R(DICE_SequenceChance((uint8_t)A(0), A64(1), Af(3)) ? 1u : 0u); return 1;
    default:                        return ecall_unknown(call);
    }
}

// Route on the subsystem byte of the call number (see ROAP_ECALL_SUBSYS).
static int ServiceEcall(void) {
    uint32_t call = g.vpu.regs[REG_A7];
    switch (call & 0xFF00u) {
    case 0x0100u: return service_sys(call);
    case 0x1000u: return service_tl(call);
    case 0x1100u: return service_spr(call);
    case 0x1200u: return service_tile(call);
    case 0x1300u: return service_pix(call);
    case 0x1400u: return service_pal(call);
    case 0x1500u: return service_iact(call);
    case 0x2000u: return service_cake(call);
    case 0x4000u: return service_dice(call);
    default:      return ecall_unknown(call);
    }
}

// ---------------------------------------------------------------------------
// Mapped sprite atlas.
//
// The guest writes 4-bit indexed pixels straight into slot N at
// ROAP_SPRITE_ADDR + N*ROAP_SPRITE_STRIDE -- no ecall, no trap, no upload call.
// Every store in that range comes back through here (vpu.c's store watch), and
// we set a dirty bit. After the slice, FlushSprites re-stages only what moved.
//
// This is the console trick: the guest just writes memory and the hardware
// notices, rather than the guest having to announce what it did. The VPU core
// stays ignorant -- it was handed a range and a callback and told nothing else.
// ---------------------------------------------------------------------------
static void SpriteStoreWatch(uint32_t addr, uint32_t size, void *user) {
    (void)user;

    // Clamp to the atlas: a store can straddle the edge of the watched range.
    uint32_t lo = (addr < ROAP_SPRITE_ADDR) ? ROAP_SPRITE_ADDR : addr;
    uint32_t hiEnd = addr + size;
    uint32_t atlasEnd = ROAP_SPRITE_ADDR + ROAP_SPRITE_BYTES;
    if (hiEnd > atlasEnd) { hiEnd = atlasEnd; }
    if (lo >= hiEnd) { return; }

    // A store can also span two slots (an unaligned 4-byte write across the
    // boundary), so mark every slot it touched, not just the one it started in.
    uint32_t first = (lo - ROAP_SPRITE_ADDR) / ROAP_SPRITE_STRIDE;
    uint32_t last  = (hiEnd - 1u - ROAP_SPRITE_ADDR) / ROAP_SPRITE_STRIDE;
    for (uint32_t s = first; s <= last && s < ROAP_SPRITE_SLOTS; s++) {
        g.sprDirty[s >> 5] |= (1u << (s & 31u));
        g.sprAnyDirty = 1;
    }
}

// Re-stage dirty slots to GDMF. Called after the guest's slice, never mid-write:
// a slot is only coherent once the guest has stopped writing it for the tick.
static void FlushSprites(void) {
    if (!g.sprAnyDirty) { return; }
    for (uint32_t w = 0; w < SPR_DIRTY_WORDS; w++) {
        uint32_t bits = g.sprDirty[w];
        if (!bits) { continue; }
        g.sprDirty[w] = 0;
        while (bits) {
            uint32_t b = (uint32_t)__builtin_ctz(bits);
            bits &= bits - 1u;
            uint32_t slot = w * 32u + b;
            if (slot < ROAP_SPRITE_SLOTS) {
                UploadSpriteBitmap((SpriteBitmapID)slot, &g.mem[ROAP_SPRITE_ADDR + slot * ROAP_SPRITE_STRIDE]);
            }
        }
    }
    g.sprAnyDirty = 0;
}

static void Load(const ROAP *roap) {
    // Claim the blob before any early return, so a rejected one is not
    // re-Loaded (and re-reported) on every tick that follows.
    g.loaded = roap;

    // The image has to fit under the MMIO window. Clamping to ROAP_MEM_SIZE
    // instead would let a large blob overwrite the very page the memset below
    // is careful to preserve -- and hand the guest a silently truncated
    // program. Refuse it instead.
    if (roap->size > ROAP_MAX_BLOB_BYTES) {
        tlPrintC("[VPU] blob larger than VPU memory -- halted\n", RED);
        g.state = ROAP_HALTED;
        return;
    }

    memset(g.mem, 0, ROAP_MMIO_BASE);   // zero program/bss/stack, but NOT the MMIO window

    // A new program inherits no one else's art. The input page is exempt: the
    // orchestrator rewrites it every tick anyway, and zeroing it would only
    // hand the guest one frame of "nothing is pressed" that isn't true.
    memset(&g.mem[ROAP_SPRITE_ADDR], 0, ROAP_SPRITE_BYTES);
    for (uint32_t w = 0; w < SPR_DIRTY_WORDS; w++) { g.sprDirty[w] = 0; }
    g.sprAnyDirty = 0;

    memcpy(g.mem, roap->code, roap->size);
    VPUInit(&g.vpu, g.mem, ROAP_MEM_SIZE, roap->entryPC);
    g.vpu.regs[REG_SP] = ROAP_MMIO_BASE;  // sp starts below the reserved MMIO window

    // VPUInit clears the watch, so arm it after. The core learns a range and a
    // callback; what lives there stays this layer's business.
    VPUSetStoreWatch(&g.vpu, ROAP_SPRITE_ADDR, ROAP_SPRITE_ADDR + ROAP_SPRITE_BYTES,
                     SpriteStoreWatch, 0);

    g.state = ROAP_RUNNING;
}

// The MMIO input page lives inside guest memory; the orchestrator fills it
// (roap.c), the guest reads it. vpu.c is unaware this region is special -- the
// ROAP layer simply reserves it (sp is set below it in Load, above) and hands
// out this pointer. Always valid: g.mem is a static buffer, live from startup
// and never reallocated, so this is safe to call before any blob is loaded.
void *VPUInputPage(void) {
    return &g.mem[ROAP_INPUT_ADDR];   // g.mem is a live static buffer from startup
}

// Monotonic wall-clock seconds. Windows via QPC (matching DICE's precedent);
// clock_gettime elsewhere, so the ROAP layer stays portable ahead of the
// Mac/Linux ports. Only the slice cap uses it, so precision beyond ~ms is moot.
static double roap_now_seconds(void) {
#if defined(_WIN32)
    LARGE_INTEGER freq, cnt;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&cnt);
    return (double)cnt.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#endif
}

// One time-slice. Separated from VPU() below purely so that every way out of
// it -- yield, halt, fault, runaway cap -- lands on the same flush, rather than
// having each of several returns remember.
//
// The guest runs to its own voluntary yield; the wall-clock cap only fires for a
// runaway. The clock is checked once per ROAP_STEP_CHUNK steps of *actual*
// execution -- accumulated across ecalls, not just when a whole chunk completes
// uninterrupted -- so a guest that spins issuing ecalls without ever yielding is
// caught too, not only a pure compute loop.
static void RunSlice(void) {
    double   start      = roap_now_seconds();
    uint32_t sinceCheck = 0;

    for (;;) {
        uint32_t used = 0;
        VPUStatus st = VPURun(&g.vpu, ROAP_STEP_CHUNK, &used);

        sinceCheck += used;
        if (sinceCheck >= ROAP_STEP_CHUNK) {
            sinceCheck = 0;
            if (roap_now_seconds() - start >= ROAP_SLICE_SECONDS) {
                return;   // runaway cap hit; stay RUNNING, resume next tick
            }
        }

        switch (st) {
        case VPU_OK:
            break;        // chunk ran clean -- keep going (cap checked above)
        case VPU_ECALL:
            if (!ServiceEcall()) {
                return;   // yielded, or halted inside a handler
            }
            break;        // serviced a non-yielding call -- keep running
        case VPU_HALT:
            g.state = ROAP_HALTED;
            return;
        default:
            tlPrintC("[VPU] fault -- halted\n", RED);
            g.state = ROAP_HALTED;
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// VPU() -- the production entry point.
// ---------------------------------------------------------------------------
void VPU(const ROAP *roap) {
    if (g.state == ROAP_UNLOADED || g.loaded != roap) {
        Load(roap);
    }
    if (g.state == ROAP_HALTED) {
        return;   // idempotent once the blob is done
    }

    RunSlice();

    // Post-slice, so a slot is only re-staged once the guest has finished
    // writing it for this tick. Mid-write staging would upload half a sprite.
    FlushSprites();
}

int VPUHalted(void) {
    return g.state == ROAP_HALTED;
}
