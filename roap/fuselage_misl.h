#pragma once
// fuselage_misl.h -- the guest-side MISL API (the "inCeption" layer).
//
// Compiled by the RISC-V toolchain into a ROAP program (NOT the host). Each
// Fuselage call below is a tiny inline-asm stub that loads its arguments into
// a0..a6, the call number into a7, and traps with `ecall`; the host's
// ServiceEcall (ROAP/VPU_roap.c) dispatches to the real Fuselage function and
// returns in a0[:a1]. So a game writes ordinary C -- tlPrint("hi"),
// SetSpritePosition(0, x, y) -- and it becomes system calls into the engine.
//
// Self-contained and libcall-free: the float/double helpers only reinterpret
// bits (no soft-float arithmetic), so this header links with -nostdlib. A guest
// program that does float MATH still needs soft-float support (-lgcc); the shim
// itself does not.
//
//   riscv32-unknown-elf-gcc -march=rv32im -mabi=ilp32 -nostdlib -ffreestanding ...

#include <stdint.h>
#include <stdbool.h>
#include "roap_abi.h"   // the single source of truth for call numbers
#include "roap_mmio.h"  // the memory-mapped input-page contract

// --- guest-side types (must match the host RGBA8 / enum layouts) -----------
// Guarded so asset headers that also define Color (e.g. spriteconverter output)
// coexist without a redefinition clash.
#ifndef FUSELAGE_COLOR_TYPE
#define FUSELAGE_COLOR_TYPE
typedef struct { unsigned char r, g, b, a; } Color;
#endif
typedef int16_t SpriteBitmapID;
typedef enum { PIXIE_MODE_TEXTURE = 0, PIXIE_MODE_LIVE = 1 } PixieMode;
typedef enum {
    PIXIE_OP_SET_ATTR = 0, PIXIE_OP_UNPACK, PIXIE_OP_DRAW, PIXIE_OP_PLOT,
    PIXIE_OP_CLEAR, PIXIE_OP_SHOW, PIXIE_OP_HIDE, PIXIE_OP_EXECUTE,
    PIXIE_OP_SET_DRAW_PATTERN
} PixieOpcode;
typedef enum {
    PIXIE_FORMAT_RGBA8 = 0, PIXIE_FORMAT_PALETTE4BPP, PIXIE_FORMAT_RLE_PALETTE_SHARED,
    PIXIE_FORMAT_RLE_PALETTE_OWN, PIXIE_FORMAT_RLE_RGBA8
} PixieUnpackFormat;

// --- raw ecall + reinterpret helpers ---------------------------------------
static inline int32_t __ec(int32_t n, int32_t a0, int32_t a1, int32_t a2,
                           int32_t a3, int32_t a4, int32_t a5, int32_t a6) {
    register int32_t r0 __asm__("a0") = a0; register int32_t r1 __asm__("a1") = a1;
    register int32_t r2 __asm__("a2") = a2; register int32_t r3 __asm__("a3") = a3;
    register int32_t r4 __asm__("a4") = a4; register int32_t r5 __asm__("a5") = a5;
    register int32_t r6 __asm__("a6") = a6; register int32_t r7 __asm__("a7") = n;
    __asm__ volatile("ecall" : "+r"(r0)
        : "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5), "r"(r6), "r"(r7) : "memory");
    return r0;
}
static inline uint64_t __ec64(int32_t n, int32_t a0, int32_t a1, int32_t a2,
                              int32_t a3, int32_t a4, int32_t a5, int32_t a6) {
    register int32_t r0 __asm__("a0") = a0; register int32_t r1 __asm__("a1") = a1;
    register int32_t r2 __asm__("a2") = a2; register int32_t r3 __asm__("a3") = a3;
    register int32_t r4 __asm__("a4") = a4; register int32_t r5 __asm__("a5") = a5;
    register int32_t r6 __asm__("a6") = a6; register int32_t r7 __asm__("a7") = n;
    __asm__ volatile("ecall" : "+r"(r0), "+r"(r1)
        : "r"(r2), "r"(r3), "r"(r4), "r"(r5), "r"(r6), "r"(r7) : "memory");
    return ((uint64_t)(uint32_t)r1 << 32) | (uint32_t)r0;
}

static inline int32_t f2i(float f)   { union { float f; int32_t i; } u; u.f = f; return u.i; }
static inline float   i2f(int32_t i) { union { float f; int32_t i; } u; u.i = i; return u.f; }
static inline int32_t dlo(double d)  { union { double d; uint64_t u; } x; x.d = d; return (int32_t)(uint32_t)x.u; }
static inline int32_t dhi(double d)  { union { double d; uint64_t u; } x; x.d = d; return (int32_t)(uint32_t)(x.u >> 32); }
static inline double  u2d(uint64_t u){ union { double d; uint64_t u; } x; x.u = u; return x.d; }
static inline int32_t cpk(Color c)   { return (int32_t)((uint32_t)c.r | ((uint32_t)c.g << 8) | ((uint32_t)c.b << 16) | ((uint32_t)c.a << 24)); }
static inline Color   cunpk(int32_t p){ Color c = { (unsigned char)p, (unsigned char)(p>>8), (unsigned char)(p>>16), (unsigned char)(p>>24) }; return c; }
static inline int32_t P(const void *p){ return (int32_t)(intptr_t)p; }

#define EC(n,a,b,c,d,e,f,g)   __ec((int32_t)(n),(int32_t)(a),(int32_t)(b),(int32_t)(c),(int32_t)(d),(int32_t)(e),(int32_t)(f),(int32_t)(g))
#define EC64(n,a,b,c,d,e,f,g) __ec64((int32_t)(n),(int32_t)(a),(int32_t)(b),(int32_t)(c),(int32_t)(d),(int32_t)(e),(int32_t)(f),(int32_t)(g))
#define LO(x) ((int32_t)(uint32_t)(uint64_t)(x))
#define HI(x) ((int32_t)(uint32_t)((uint64_t)(x) >> 32))

// End the program (host transitions the VPU to HALTED).
static inline void VPUHalt(void) { __asm__ volatile("ebreak"); }

// ======================= MMIO input page ===================================
// Read host input straight from the memory-mapped page -- NO ecall. The
// Fuselage orchestrator refreshes it each tick, before this program runs.
static inline const volatile RoapInput *ROAP_input(void) { return (const volatile RoapInput *)ROAP_INPUT_ADDR; }
static inline int InputKey(int scancode)              { return ROAP_input()->keys[scancode & 0xFF]; }
static inline int InputMouseX(void)                   { return ROAP_input()->mouseX; }
static inline int InputMouseY(void)                   { return ROAP_input()->mouseY; }
static inline int InputMouseWheel(void)               { return ROAP_input()->mouseWheel; }
static inline int InputMouseButton(int b)             { return ROAP_input()->mouseButtons[b & 7]; }
static inline int InputPadConnected(int slot)         { return ROAP_input()->pads[slot].connected; }
static inline int InputPadButton(int slot, int mask)  { return (ROAP_input()->pads[slot].buttons & mask) != 0; }
static inline int InputPadTrigger(int slot, int which){ return which ? ROAP_input()->pads[slot].rt : ROAP_input()->pads[slot].lt; }
static inline int InputPadAxis(int slot, int axis) {
    const volatile RoapInput *in = ROAP_input();
    switch (axis) { case 0: return in->pads[slot].lx; case 1: return in->pads[slot].ly;
                    case 2: return in->pads[slot].rx; case 3: return in->pads[slot].ry; }
    return 0;
}

// ======================= MMIO sprite atlas =================================
// Write a sprite's pixels straight into the atlas -- NO ecall, no upload call.
// Slot N is always at a fixed address, so there is nothing to look up:
//
//     unsigned char *px = SpriteSlot(BM_PLAYER);
//     for (int i = 0; i < ROAP_SPRITE_STRIDE; i++) px[i] = ...;   // 4bpp, 2 px/byte
//
// Fuselage notices. Every store landing in the atlas marks that slot dirty, and
// the changed slots are re-staged to the GPU after your slice ends -- the same
// bargain a console makes: write to VRAM and the hardware keeps up. There is no
// commit call to forget.
//
// The layout is the one UploadSpriteBitmap has always taken: 64x64 4-bit
// indexed pixels, 2 per byte, row-major, ROAP_SPRITE_STRIDE (2048) bytes a slot.
// UploadSpriteBitmap still works and still costs a trap per upload; this is the
// same bytes without the trap.
//
// Not volatile: this is ordinary memory the guest owns until its slice ends,
// and marking it volatile would only stop the compiler optimising your own
// pixel loops. Nothing reads it back mid-slice.
static inline unsigned char *SpriteSlot(int id) {
    return (unsigned char *)(ROAP_SPRITE_ADDR + (unsigned)id * ROAP_SPRITE_STRIDE);
}

// ======================= 0x01 SYS ==========================================
static inline void   FuselageSignal(int s)              { EC(ROAP_SYS_SIGNAL, s,0,0,0,0,0,0); }
static inline void   FuselageWaitVBL(void)              { EC(ROAP_SYS_WAITVBL, 0,0,0,0,0,0,0); }
static inline void   VPUYield(void)                     { EC(ROAP_SYS_YIELD, 0,0,0,0,0,0,0); }
static inline void   VPUSleep(int ticks)                { EC(ROAP_SYS_SLEEP, ticks,0,0,0,0,0,0); }
static inline float  FuselageGetCurrentFPS(void)        { return i2f(EC(ROAP_SYS_GET_FPS,0,0,0,0,0,0,0)); }
static inline void   FuselageSetMouseCapture(bool b)    { EC(ROAP_SYS_SET_MOUSECAPTURE, b,0,0,0,0,0,0); }
static inline bool   FuselageGetMouseCapture(void)      { return EC(ROAP_SYS_GET_MOUSECAPTURE,0,0,0,0,0,0,0) != 0; }
static inline bool   FuselageToggleMouseCapture(void)   { return EC(ROAP_SYS_TOGGLE_MOUSECAPTURE,0,0,0,0,0,0,0) != 0; }
static inline void   FuselageSetCursorVisible(bool b)   { EC(ROAP_SYS_SET_CURSORVISIBLE, b,0,0,0,0,0,0); }
static inline bool   FuselageGetCursorVisible(void)     { return EC(ROAP_SYS_GET_CURSORVISIBLE,0,0,0,0,0,0,0) != 0; }
static inline void   FuselageSetVSync(bool b)           { EC(ROAP_SYS_SET_VSYNC, b,0,0,0,0,0,0); }
static inline bool   FuselageGetVSync(void)             { return EC(ROAP_SYS_GET_VSYNC,0,0,0,0,0,0,0) != 0; }
static inline void   FuselageSetTargetFPS(double f)     { EC(ROAP_SYS_SET_TARGETFPS, dlo(f),dhi(f),0,0,0,0,0); }
static inline double FuselageGetTargetFPS(void)         { return u2d(EC64(ROAP_SYS_GET_TARGETFPS,0,0,0,0,0,0,0)); }
static inline double FuselageGetSimRate(void)           { return u2d(EC64(ROAP_SYS_GET_SIMRATE,0,0,0,0,0,0,0)); }

// ======================= 0x10 TL (text layer) ==============================
static inline int   tlPrint(const char *s)                                 { return EC(ROAP_TL_PRINT, P(s),0,0,0,0,0,0); }
static inline int   tlPrintC(const char *s, Color c)                       { return EC(ROAP_TL_PRINT_C, P(s), cpk(c),0,0,0,0,0); }
static inline int   tlPrintCP(const char *s, Color c, int x, int y)        { return EC(ROAP_TL_PRINT_CP, P(s), cpk(c), x, y,0,0,0); }
static inline int   tlPrintChar(unsigned char ch)                          { return EC(ROAP_TL_PRINTCHAR, ch,0,0,0,0,0,0); }
static inline int   tlPrintCharC(unsigned char ch, Color c)                { return EC(ROAP_TL_PRINTCHAR_C, ch, cpk(c),0,0,0,0,0); }
static inline int   tlPrintCharCP(unsigned char ch, Color c, int x, int y) { return EC(ROAP_TL_PRINTCHAR_CP, ch, cpk(c), x, y,0,0,0); }
static inline int   tlPrintInt(int v)                                      { return EC(ROAP_TL_PRINTINT, v,0,0,0,0,0,0); }
static inline int   tlPrintIntC(int v, Color c)                            { return EC(ROAP_TL_PRINTINT_C, v, cpk(c),0,0,0,0,0); }
static inline int   tlPrintIntCP(int v, Color c, int x, int y)             { return EC(ROAP_TL_PRINTINT_CP, v, cpk(c), x, y,0,0,0); }
static inline int   tlNewLine(void)                                        { return EC(ROAP_TL_NEWLINE,0,0,0,0,0,0,0); }
static inline void  tlCLS(void)                                            { EC(ROAP_TL_CLS,0,0,0,0,0,0,0); }
static inline void  tlHome(void)                                           { EC(ROAP_TL_HOME,0,0,0,0,0,0,0); }
static inline void  tlSetColor(Color c)                                    { EC(ROAP_TL_SET_COLOR, cpk(c),0,0,0,0,0,0); }
static inline Color tlGetColor(void)                                       { return cunpk(EC(ROAP_TL_GET_COLOR,0,0,0,0,0,0,0)); }
static inline void  tlSetCursor(int x, int y)                              { EC(ROAP_TL_SET_CURSOR, x, y,0,0,0,0,0); }
static inline int   tlGetCursor(void)                                      { return EC(ROAP_TL_GET_CURSOR,0,0,0,0,0,0,0); }
static inline unsigned short tlGetCursorX(void)                            { return (unsigned short)EC(ROAP_TL_GET_CURSOR_X,0,0,0,0,0,0,0); }
static inline unsigned short tlGetCursorY(void)                            { return (unsigned short)EC(ROAP_TL_GET_CURSOR_Y,0,0,0,0,0,0,0); }
static inline unsigned short tlScrollUp(void)                              { return (unsigned short)EC(ROAP_TL_SCROLL_UP,0,0,0,0,0,0,0); }
static inline bool  tlActivate(void)                                       { return EC(ROAP_TL_ACTIVE,0,0,0,0,0,0,0) != 0; }
static inline bool  tlDeactivate(void)                                     { return EC(ROAP_TL_INACTIVE,0,0,0,0,0,0,0) != 0; }
static inline bool  tlToggle(void)                                         { return EC(ROAP_TL_TOGGLE,0,0,0,0,0,0,0) != 0; }
static inline bool  tlStatus(void)                                         { return EC(ROAP_TL_STATUS,0,0,0,0,0,0,0) != 0; }

// ======================= 0x11 SPR (sprites) ================================
static inline bool  UploadSpriteBitmap(SpriteBitmapID id, const unsigned char *bm) { return EC(ROAP_SPR_UPLOAD_BITMAP, id, P(bm),0,0,0,0,0) != 0; }
static inline bool  AssignSprite(int i, SpriteBitmapID id)                  { return EC(ROAP_SPR_ASSIGN, i, id,0,0,0,0,0) != 0; }
static inline bool  AssignSpriteBitmapFromSprite(int s, int d)             { return EC(ROAP_SPR_ASSIGN_FROM, s, d,0,0,0,0,0) != 0; }
static inline SpriteBitmapID GetSpriteBitmapID(int i)                      { return (SpriteBitmapID)EC(ROAP_SPR_GET_BITMAP_ID, i,0,0,0,0,0,0); }
static inline void  ClearSprite(int i)                                     { EC(ROAP_SPR_CLEAR, i,0,0,0,0,0,0); }
static inline void  AssignSpriteTestPattern(int i)                      { EC(ROAP_SPR_TEST_PATTERN, i,0,0,0,0,0,0); }
static inline bool  GetSpriteEnabled(int i)                               { return EC(ROAP_SPR_GET_ENABLED, i,0,0,0,0,0,0) != 0; }
static inline bool  SetSpriteEnabled(int i, bool e)                       { return EC(ROAP_SPR_SET_ENABLED, i, e,0,0,0,0,0) != 0; }
static inline bool  ToggleSpriteEnabled(int i)                           { return EC(ROAP_SPR_TOGGLE_ENABLED, i,0,0,0,0,0,0) != 0; }
static inline bool  GetSpriteVisible(int i)                              { return EC(ROAP_SPR_GET_VISIBLE, i,0,0,0,0,0,0) != 0; }
static inline bool  SetSpriteVisible(int i, bool v)                      { return EC(ROAP_SPR_SET_VISIBLE, i, v,0,0,0,0,0) != 0; }
static inline bool  ToggleSpriteVisible(int i)                          { return EC(ROAP_SPR_TOGGLE_VISIBLE, i,0,0,0,0,0,0) != 0; }
static inline void  SetSpritePosition(int i, float x, float y)          { EC(ROAP_SPR_SET_POSITION, i, f2i(x), f2i(y),0,0,0,0); }
static inline bool  MoveSpriteBy(int i, float dx, float dy)             { return EC(ROAP_SPR_UPDATE_POSITION, i, f2i(dx), f2i(dy),0,0,0,0) != 0; }
static inline float GetSpriteX(int i)                                   { return i2f(EC(ROAP_SPR_GET_X, i,0,0,0,0,0,0)); }
static inline float GetSpriteY(int i)                                   { return i2f(EC(ROAP_SPR_GET_Y, i,0,0,0,0,0,0)); }
static inline void  SetSpriteScale(int i, float s)                      { EC(ROAP_SPR_SET_SCALE, i, f2i(s),0,0,0,0,0); }
static inline float GetSpriteScale(int i)                               { return i2f(EC(ROAP_SPR_GET_SCALE, i,0,0,0,0,0,0)); }
static inline bool  ScaleSpriteBy(int i, float d)                       { return EC(ROAP_SPR_CHANGE_SCALE, i, f2i(d),0,0,0,0,0) != 0; }
static inline void  SetSpriteRotation(int i, float r)                   { EC(ROAP_SPR_SET_ROTATION, i, f2i(r),0,0,0,0,0); }
static inline float GetSpriteRotation(int i)                            { return i2f(EC(ROAP_SPR_GET_ROTATION, i,0,0,0,0,0,0)); }
static inline bool  RotateSpriteBy(int i, float d)                      { return EC(ROAP_SPR_CHANGE_ROTATION, i, f2i(d),0,0,0,0,0) != 0; }
static inline void  SetSpriteSkew(int i, float sx, float sy)            { EC(ROAP_SPR_SET_SKEW, i, f2i(sx), f2i(sy),0,0,0,0); }
static inline void  GetSpriteSkew(int i, float *sx, float *sy)          { EC(ROAP_SPR_GET_SKEW, i, P(sx), P(sy),0,0,0,0); }
static inline bool  SetSpriteHotspot(int i, float hx, float hy)         { return EC(ROAP_SPR_SET_HOTSPOT, i, f2i(hx), f2i(hy),0,0,0,0) != 0; }
static inline void  GetSpriteHotspot(int i, float *hx, float *hy)       { EC(ROAP_SPR_GET_HOTSPOT, i, P(hx), P(hy),0,0,0,0); }
static inline bool  SetSpriteRotateAroundHotspot(int i, bool on)        { return EC(ROAP_SPR_SET_ROT_HOTSPOT, i, on,0,0,0,0,0) != 0; }
static inline bool  GetSpriteRotateAroundHotspot(int i)                 { return EC(ROAP_SPR_GET_ROT_HOTSPOT, i,0,0,0,0,0,0) != 0; }
static inline unsigned char SetSpriteFlip(int i, unsigned char m)       { return (unsigned char)EC(ROAP_SPR_SET_FLIP, i, m,0,0,0,0,0); }
static inline unsigned char GetSpriteFlip(int i)                        { return (unsigned char)EC(ROAP_SPR_GET_FLIP, i,0,0,0,0,0,0); }
static inline void  SetSpritePriority(int i, unsigned char p)           { EC(ROAP_SPR_SET_PRIORITY, i, p,0,0,0,0,0); }
static inline unsigned char GetSpritePriority(int i)                    { return (unsigned char)EC(ROAP_SPR_GET_PRIORITY, i,0,0,0,0,0,0); }
static inline bool  SetSpriteColorPalette(int i, unsigned char p)       { return EC(ROAP_SPR_SET_PALETTE, i, p,0,0,0,0,0) != 0; }
static inline unsigned char GetSpriteColorPalette(int i)                { return (unsigned char)EC(ROAP_SPR_GET_PALETTE, i,0,0,0,0,0,0); }
static inline void  SetSpriteTransparency(int i, unsigned char t)       { EC(ROAP_SPR_SET_TRANSPARENCY, i, t,0,0,0,0,0); }
static inline unsigned char GetSpriteTransparency(int i)                { return (unsigned char)EC(ROAP_SPR_GET_TRANSPARENCY, i,0,0,0,0,0,0); }
static inline void  SetSpriteShowZero(int i, bool z)                    { EC(ROAP_SPR_SHOW_ZERO, i, z,0,0,0,0,0); }
static inline unsigned short SetSpriteCollidableColors(int i, unsigned short m) { return (unsigned short)EC(ROAP_SPR_SET_COLLIDABLE, i, m,0,0,0,0,0); }
static inline unsigned short GetSpriteCollidableColors(int i)           { return (unsigned short)EC(ROAP_SPR_GET_COLLIDABLE, i,0,0,0,0,0,0); }
static inline unsigned char SetSpriteCollisionTypes(int i, unsigned char m) { return (unsigned char)EC(ROAP_SPR_SET_COLLTYPES, i, m,0,0,0,0,0); }
static inline unsigned char GetSpriteCollisionTypes(int i)             { return (unsigned char)EC(ROAP_SPR_GET_COLLTYPES, i,0,0,0,0,0,0); }
static inline bool  SpriteHasCollision(int i)                          { return EC(ROAP_SPR_HAS_COLLISION, i,0,0,0,0,0,0) != 0; }
static inline int   GetSpriteCollisionCount(int i)                     { return EC(ROAP_SPR_GET_COLL_COUNT, i,0,0,0,0,0,0); }
static inline int   CopySpriteCollisions(int i, void *buf, int maxc)   { return EC(ROAP_SPR_COPY_COLLISIONS, i, P(buf), maxc,0,0,0,0); }
static inline unsigned char CheckSpritePairCollision(int a, int b, unsigned char t) { return (unsigned char)EC(ROAP_SPR_CHECK_PAIR, a, b, t,0,0,0,0); }
static inline bool  WorldPointOnSprite(int i, float x, float y)         { return EC(ROAP_SPR_WORLD_POINT_ON, i, f2i(x), f2i(y),0,0,0,0) != 0; }
static inline void  ToggleSpriteAtlasView(void)                        { EC(ROAP_SPR_TOGGLE_ATLAS,0,0,0,0,0,0,0); }
static inline bool  GetSpriteAtlasViewActive(void)                     { return EC(ROAP_SPR_GET_ATLAS_ACTIVE,0,0,0,0,0,0,0) != 0; }
static inline int   GetRenderedSpriteCount(void)                       { return EC(ROAP_SPR_GET_RENDERED_COUNT,0,0,0,0,0,0,0); }

// ======================= 0x12 TILE =========================================
static inline bool  InitTileLayer(uint8_t l, uint16_t mw, uint16_t mh, uint16_t tw, uint16_t th, uint16_t tc, float sc) { return EC(ROAP_TILE_INIT, l, mw, mh, tw, th, tc, f2i(sc)) != 0; }
static inline bool  ReleaseTileLayer(uint8_t l)                        { return EC(ROAP_TILE_RELEASE, l,0,0,0,0,0,0) != 0; }
static inline bool  UploadTileBitmap(uint8_t l, int id, const unsigned char *bm) { return EC(ROAP_TILE_UPLOAD_BITMAP, l, id, P(bm),0,0,0,0) != 0; }
static inline bool  GetTileLayerVisible(uint8_t l)                     { return EC(ROAP_TILE_GET_VISIBLE, l,0,0,0,0,0,0) != 0; }
static inline bool  SetTileLayerVisible(uint8_t l, bool v)             { return EC(ROAP_TILE_SET_VISIBLE, l, v,0,0,0,0,0) != 0; }
static inline bool  ToggleTileLayerVisible(uint8_t l)                  { return EC(ROAP_TILE_TOGGLE_VISIBLE, l,0,0,0,0,0,0) != 0; }
static inline bool  GetTileLayerEnabled(uint8_t l)                     { return EC(ROAP_TILE_GET_ENABLED, l,0,0,0,0,0,0) != 0; }
static inline bool  SetTileLayerEnabled(uint8_t l, bool e)             { return EC(ROAP_TILE_SET_ENABLED, l, e,0,0,0,0,0) != 0; }
static inline bool  ToggleTileLayerEnabled(uint8_t l)                  { return EC(ROAP_TILE_TOGGLE_ENABLED, l,0,0,0,0,0,0) != 0; }
static inline bool  PlaceTile(uint8_t l, uint16_t x, uint16_t y, uint16_t id) { return EC(ROAP_TILE_PLACE, l, x, y, id,0,0,0) != 0; }
static inline bool  FillTileLayer(uint8_t l, uint16_t id)              { return EC(ROAP_TILE_FILL, l, id,0,0,0,0,0) != 0; }
static inline bool  SetTileCellPalette(uint8_t l, uint16_t x, uint16_t y, uint8_t p) { return EC(ROAP_TILE_SET_CELL_PALETTE, l, x, y, p,0,0,0) != 0; }
static inline bool  SetTileLayerPalette(uint8_t l, uint8_t p)          { return EC(ROAP_TILE_SET_PALETTE, l, p,0,0,0,0,0) != 0; }
static inline bool  SetTileCellTransparency(uint8_t l, uint16_t x, uint16_t y, uint8_t t) { return EC(ROAP_TILE_SET_CELL_TRANSP, l, x, y, t,0,0,0) != 0; }
static inline bool  SetTileLayerTransparency(uint8_t l, uint8_t t)     { return EC(ROAP_TILE_SET_TRANSP, l, t,0,0,0,0,0) != 0; }
static inline bool  SetTileCellShowZero(uint8_t l, uint16_t x, uint16_t y, bool z) { return EC(ROAP_TILE_SET_CELL_SHOWZERO, l, x, y, z,0,0,0) != 0; }
static inline bool  SetTileLayerShowZero(uint8_t l, bool z)            { return EC(ROAP_TILE_SET_SHOWZERO, l, z,0,0,0,0,0) != 0; }
static inline bool  SetTileFlip(uint8_t l, uint16_t x, uint16_t y, bool h, bool v) { return EC(ROAP_TILE_SET_FLIP, l, x, y, h, v,0,0) != 0; }
static inline void  SetTileCellCollision(uint8_t l, uint16_t x, uint16_t y, bool e) { EC(ROAP_TILE_SET_CELL_COLLISION, l, x, y, e,0,0,0); }
static inline bool  SetTileMapWrapping(uint8_t l, bool wx, bool wy)    { return EC(ROAP_TILE_SET_WRAPPING, l, wx, wy,0,0,0,0) != 0; }
static inline bool  SetTileLayerScale(uint8_t l, float s)             { return EC(ROAP_TILE_SET_SCALE, l, f2i(s),0,0,0,0,0) != 0; }
static inline bool  SetTileViewport(uint8_t l, int x, int y, int w, int h) { return EC(ROAP_TILE_SET_VIEWPORT, l, x, y, w, h,0,0) != 0; }  /* x,y signed: may be off-canvas */
static inline bool  SetTileLayerPriority(uint8_t l, unsigned char p)   { return EC(ROAP_TILE_SET_PRIORITY, l, p,0,0,0,0,0) != 0; }
static inline unsigned char GetTileLayerPriority(uint8_t l)            { return (unsigned char)EC(ROAP_TILE_GET_PRIORITY, l,0,0,0,0,0,0); }
static inline bool  ScrollTileMap(uint8_t l, double dx, double dy)     { return EC(ROAP_TILE_SCROLL, l, dlo(dx), dhi(dx), dlo(dy), dhi(dy),0,0) != 0; }
static inline bool  SetTileMapOffset(uint8_t l, double x, double y)    { return EC(ROAP_TILE_SET_MAP_OFFSET, l, dlo(x), dhi(x), dlo(y), dhi(y),0,0) != 0; }
static inline bool  SetTileOffset(uint8_t l, uint16_t mx, uint16_t my, int8_t ox, int8_t oy) { return EC(ROAP_TILE_SET_TILE_OFFSET, l, mx, my, ox, oy,0,0) != 0; }
static inline void  SetTileLayerCollidableColors(uint8_t l, uint16_t m) { EC(ROAP_TILE_SET_COLLIDABLE, l, m,0,0,0,0,0); }
static inline uint16_t GetTileLayerCollidableColors(uint8_t l)        { return (uint16_t)EC(ROAP_TILE_GET_COLLIDABLE, l,0,0,0,0,0,0); }
static inline void  UploadTileTestPattern(uint8_t l, int id)           { EC(ROAP_TILE_TEST_PATTERN, l, id,0,0,0,0,0); }
static inline void  UploadTileBoxPattern(uint8_t l, int id)            { EC(ROAP_TILE_BOX_PATTERN, l, id,0,0,0,0,0); }
static inline void  ToggleTileAtlasView(uint8_t l)                   { EC(ROAP_TILE_TOGGLE_ATLAS, l,0,0,0,0,0,0); }
static inline bool  GetTileAtlasViewActive(uint8_t l)                { return EC(ROAP_TILE_GET_ATLAS_ACTIVE, l,0,0,0,0,0,0) != 0; }

// ======================= 0x13 PIX (pixies) =================================
static inline bool  InitPixie(int id, PixieMode m, int w, int h)     { return EC(ROAP_PIX_INIT, id, m, w, h,0,0,0) != 0; }
static inline void  ReleasePixie(int id)                             { EC(ROAP_PIX_SHUTDOWN, id,0,0,0,0,0,0); }
static inline bool  IssuePixieCommand(int id, PixieOpcode op, uint16_t fl, const uint32_t a[4]) { return EC(ROAP_PIX_COMMAND, id, op, fl, P(a),0,0,0) != 0; }
static inline bool  WritePixieRAM(int id, uint32_t off, const void *d, uint32_t n) { return EC(ROAP_PIX_WRITE, id, off, P(d), n,0,0,0) != 0; }
static inline uint32_t GetPixieRAMSize(int id)                       { return (uint32_t)EC(ROAP_PIX_GET_RAM_SIZE, id,0,0,0,0,0,0); }
static inline uint32_t ReadPixieString(int id, char *buf, uint32_t n) { return (uint32_t)EC(ROAP_PIX_READ_STRING, id, P(buf), n,0,0,0,0); }
static inline bool  SetPixiePosition(int id, int x, int y)           { return EC(ROAP_PIX_SET_POSITION, id, x, y,0,0,0,0) != 0; }
static inline bool  SetPixieDisplaySize(int id, int w, int h)        { return EC(ROAP_PIX_SET_DISPLAY_SIZE, id, w, h,0,0,0,0) != 0; }
static inline bool  SetPixiePriority(int id, unsigned char p)        { return EC(ROAP_PIX_SET_PRIORITY, id, p,0,0,0,0,0) != 0; }
static inline bool  SetPixieEnabled(int id, bool e)                  { return EC(ROAP_PIX_SET_ENABLED, id, e,0,0,0,0,0) != 0; }
static inline bool  ShowPixie(int id)                                { return EC(ROAP_PIX_SHOW, id,0,0,0,0,0,0) != 0; }
static inline bool  HidePixie(int id)                                { return EC(ROAP_PIX_HIDE, id,0,0,0,0,0,0) != 0; }
static inline bool  SetPixieDrawPattern(int id, uint16_t p)          { return EC(ROAP_PIX_SET_DRAW_PATTERN, id, p,0,0,0,0,0) != 0; }
static inline uint16_t GetPixieDrawPattern(int id)                   { return (uint16_t)EC(ROAP_PIX_GET_DRAW_PATTERN, id,0,0,0,0,0,0); }
static inline bool  GetPixieInitialized(int id)                      { return EC(ROAP_PIX_GET_INITIALIZED, id,0,0,0,0,0,0) != 0; }
static inline PixieMode GetPixieMode(int id)                         { return (PixieMode)EC(ROAP_PIX_GET_MODE, id,0,0,0,0,0,0); }
static inline int   GetPixieOutputWidth(int id)                      { return EC(ROAP_PIX_GET_OUT_WIDTH, id,0,0,0,0,0,0); }
static inline int   GetPixieOutputHeight(int id)                     { return EC(ROAP_PIX_GET_OUT_HEIGHT, id,0,0,0,0,0,0); }
static inline int   GetPixieX(int id)                                { return EC(ROAP_PIX_GET_X, id,0,0,0,0,0,0); }
static inline int   GetPixieY(int id)                                { return EC(ROAP_PIX_GET_Y, id,0,0,0,0,0,0); }
static inline int   GetPixieDisplayWidth(int id)                     { return EC(ROAP_PIX_GET_DISPLAY_WIDTH, id,0,0,0,0,0,0); }
static inline int   GetPixieDisplayHeight(int id)                    { return EC(ROAP_PIX_GET_DISPLAY_HEIGHT, id,0,0,0,0,0,0); }
static inline unsigned char GetPixiePriority(int id)                 { return (unsigned char)EC(ROAP_PIX_GET_PRIORITY, id,0,0,0,0,0,0); }
static inline bool  GetPixieEnabled(int id)                          { return EC(ROAP_PIX_GET_ENABLED, id,0,0,0,0,0,0) != 0; }
static inline bool  GetPixieShown(int id)                            { return EC(ROAP_PIX_GET_SHOWN, id,0,0,0,0,0,0) != 0; }

// ======================= 0x14 PAL (colors) =================================
static inline uint32_t PackRGBA8(Color c)                            { return (uint32_t)EC(ROAP_PAL_PACK_RGBA8, cpk(c),0,0,0,0,0,0); }
static inline bool  SetColorsPalette(unsigned char p, Color *pal16)  { return EC(ROAP_PAL_LOAD_FROM_SPRITE, p, P(pal16),0,0,0,0,0) != 0; }
static inline Color GetColorsColorFromChar(unsigned char c)          { return cunpk(EC(ROAP_PAL_GET_FROM_CHAR, c,0,0,0,0,0,0)); }
static inline Color GetColorsCommodoreColor(unsigned char c)         { return cunpk(EC(ROAP_PAL_GET_COMMODORE, c,0,0,0,0,0,0)); }
static inline Color GetColorsTandyColor(unsigned char c)             { return cunpk(EC(ROAP_PAL_GET_TANDY, c,0,0,0,0,0,0)); }
static inline Color GetColorsANSIColor(unsigned char c)              { return cunpk(EC(ROAP_PAL_GET_ANSI, c,0,0,0,0,0,0)); }
static inline bool  SetColorsPaletteColor(unsigned char p, unsigned char i, Color c) { return EC(ROAP_PAL_SET, p, i, cpk(c),0,0,0,0) != 0; }
static inline Color GetColorsPaletteColor(unsigned char p, unsigned char i) { return cunpk(EC(ROAP_PAL_GET, p, i,0,0,0,0,0)); }

// ======================= 0x15 IACT (interactions) ==========================
static inline float DistanceBetweenSprites(int a, int b)             { return i2f(EC(ROAP_IACT_DIST_SPRITES, a, b,0,0,0,0,0)); }
static inline float DirectionBetweenSprites(int a, int b)            { return i2f(EC(ROAP_IACT_DIR_SPRITES, a, b,0,0,0,0,0)); }
static inline bool  LocalBitmapPixelToWorldPixel(int i, int lx, int ly, float *wx, float *wy) { return EC(ROAP_IACT_LOCALPIX_TO_WORLD, i, lx, ly, P(wx), P(wy),0,0) != 0; }

// ======================= 0x20 CAKE (input) =================================
static inline bool  CAKE_GetKey(int scancode)                        { return EC(ROAP_CAKE_GET_KEY, scancode,0,0,0,0,0,0) != 0; }
static inline int   CAKE_GetMouseX(void)                             { return EC(ROAP_CAKE_GET_MOUSE_X,0,0,0,0,0,0,0); }
static inline int   CAKE_GetMouseY(void)                             { return EC(ROAP_CAKE_GET_MOUSE_Y,0,0,0,0,0,0,0); }
static inline int   CAKE_GetMouseWheel(void)                         { return EC(ROAP_CAKE_GET_MOUSE_WHEEL,0,0,0,0,0,0,0); }
static inline bool  CAKE_GetMouseButton(int b)                       { return EC(ROAP_CAKE_GET_MOUSE_BUTTON, b,0,0,0,0,0,0) != 0; }
static inline int   CAKE_IsSilenced(void)                            { return EC(ROAP_CAKE_IS_SILENCED,0,0,0,0,0,0,0); }
static inline int   CAKE_IsControllerConnected(int s)                { return EC(ROAP_CAKE_CTRL_CONNECTED, s,0,0,0,0,0,0); }
static inline int   CAKE_GetControllerConnectionState(int s)         { return EC(ROAP_CAKE_CTRL_CONN_STATE, s,0,0,0,0,0,0); }
static inline int   CAKE_GetControllerBackend(int s)                 { return EC(ROAP_CAKE_CTRL_BACKEND, s,0,0,0,0,0,0); }
static inline int   CAKE_GetControllerVendorID(int s)                { return EC(ROAP_CAKE_CTRL_VENDOR, s,0,0,0,0,0,0); }
static inline int   CAKE_GetControllerProductID(int s)               { return EC(ROAP_CAKE_CTRL_PRODUCT, s,0,0,0,0,0,0); }
static inline int   CAKE_GetControllerXInputIndex(int s)             { return EC(ROAP_CAKE_CTRL_XINPUT_IDX, s,0,0,0,0,0,0); }
static inline int   CAKE_SetControllerVibration(int s, uint16_t l, uint16_t r) { return EC(ROAP_CAKE_CTRL_VIBRATION, s, l, r,0,0,0,0); }
static inline int   CAKE_GetControllerName(int s, char *buf, int n)  { return EC(ROAP_CAKE_CTRL_COPY_NAME, s, P(buf), n,0,0,0,0); }

// ======================= 0x40 DICE (RNG / timers / sequences) ==============
static inline bool     DICE_InitRNG(uint8_t h, unsigned mode, uint64_t seed) { return EC(ROAP_DICE_INIT_RNG, h, mode, LO(seed), HI(seed),0,0,0) != 0; }
static inline void     DICE_ReleaseRNG(uint8_t h)                    { EC(ROAP_DICE_RELEASE_RNG, h,0,0,0,0,0,0); }
static inline void     DICE_MixRNGEntropy(uint8_t h, const void *d, uint32_t n) { EC(ROAP_DICE_RNG_MIX_ENTROPY, h, P(d), n,0,0,0,0); }
static inline uint64_t DICE_GetRNGSeed(uint8_t h)                    { return EC64(ROAP_DICE_RNG_GET_SEED, h,0,0,0,0,0,0); }
static inline uint32_t DICE_RandUint(uint8_t h)                      { return (uint32_t)EC(ROAP_DICE_RAND_UINT, h,0,0,0,0,0,0); }
static inline int      DICE_RandInt(uint8_t h, int lo, int hi)       { return EC(ROAP_DICE_RAND_INT, h, lo, hi,0,0,0,0); }
static inline float    DICE_RandFloat(uint8_t h)                     { return i2f(EC(ROAP_DICE_RAND_FLOAT, h,0,0,0,0,0,0)); }
static inline bool     DICE_RandChance(uint8_t h, float p)           { return EC(ROAP_DICE_RAND_CHANCE, h, f2i(p),0,0,0,0,0) != 0; }
static inline bool     DICE_InitTimer(uint8_t h, uint64_t usec, bool autostart) { return EC(ROAP_DICE_INIT_TIMER, h, LO(usec), HI(usec), autostart,0,0,0) != 0; }
static inline void     DICE_ReleaseTimer(uint8_t h)                  { EC(ROAP_DICE_RELEASE_TIMER, h,0,0,0,0,0,0); }
static inline bool     DICE_StartTimer(uint8_t h)                    { return EC(ROAP_DICE_TIMER_START, h,0,0,0,0,0,0) != 0; }
static inline bool     DICE_StopTimer(uint8_t h)                     { return EC(ROAP_DICE_TIMER_STOP, h,0,0,0,0,0,0) != 0; }
static inline bool     DICE_PauseTimer(uint8_t h)                    { return EC(ROAP_DICE_TIMER_PAUSE, h,0,0,0,0,0,0) != 0; }
static inline bool     DICE_ResumeTimer(uint8_t h)                   { return EC(ROAP_DICE_TIMER_RESUME, h,0,0,0,0,0,0) != 0; }
static inline bool     DICE_IsTimerRunning(uint8_t h)                { return EC(ROAP_DICE_TIMER_IS_RUNNING, h,0,0,0,0,0,0) != 0; }
static inline uint32_t DICE_ConsumeTimer(uint8_t h)                  { return (uint32_t)EC(ROAP_DICE_TIMER_CONSUME, h,0,0,0,0,0,0); }
static inline double   DICE_TimerDelta(uint8_t h)                    { return u2d(EC64(ROAP_DICE_TIMER_DELTA, h,0,0,0,0,0,0)); }
static inline bool     DICE_SeedDie(uint8_t d, uint64_t seed)        { return EC(ROAP_DICE_SEED_DIE, d, LO(seed), HI(seed),0,0,0,0) != 0; }
static inline int      DICE_DieRange(uint8_t d, int lo, int hi)      { return EC(ROAP_DICE_DIE_RANGE, d, lo, hi,0,0,0,0); }
static inline float    DICE_DieFloat(uint8_t d)                      { return i2f(EC(ROAP_DICE_DIE_FLOAT, d,0,0,0,0,0,0)); }
static inline bool     DICE_DieChance(uint8_t d, float p)            { return EC(ROAP_DICE_DIE_CHANCE, d, f2i(p),0,0,0,0,0) != 0; }
static inline bool     DICE_SeedSequence(uint8_t s, uint64_t seed)   { return EC(ROAP_DICE_SEED_SEQUENCE, s, LO(seed), HI(seed),0,0,0,0) != 0; }
static inline uint32_t DICE_SequenceUint(uint8_t s, uint64_t pos)    { return (uint32_t)EC(ROAP_DICE_SEQ_UINT, s, LO(pos), HI(pos),0,0,0,0); }
static inline int      DICE_SequenceInt(uint8_t s, uint64_t pos, int lo, int hi) { return EC(ROAP_DICE_SEQ_INT, s, LO(pos), HI(pos), lo, hi,0,0); }
static inline float    DICE_SequenceFloat(uint8_t s, uint64_t pos)   { return i2f(EC(ROAP_DICE_SEQ_FLOAT, s, LO(pos), HI(pos),0,0,0,0)); }
static inline bool     DICE_SequenceChance(uint8_t s, uint64_t pos, float p) { return EC(ROAP_DICE_SEQ_CHANCE, s, LO(pos), HI(pos), f2i(p),0,0,0) != 0; }
