// roap_sprtest.c -- MMIO sprite atlas demo/test.
//
// Draws a sprite WITHOUT UploadSpriteBitmap: it writes 4-bit indexed pixels
// straight into the mapped atlas with SpriteSlot(), and the host notices via
// the store watch and re-stages the slot. No ecall carries the pixels -- the
// only ecalls here set up the instance and place it. If the box appears, a
// guest store reached the GPU with nothing but the store to announce it.
//
// It animates: every ~30 ticks it repaints the slot a different palette index,
// proving the dirty/flush path fires again on later writes, not just the first.
//
//   riscv32-unknown-elf-gcc -march=rv32im -mabi=ilp32 -nostdlib -ffreestanding \
//       -I. -Wl,-T,roap.ld -o roap_sprtest.elf roap_sprtest.c
//   objcopy -O binary -> .roap ; play in MISLlauncher.

#include "fuselage_misl.h"

#define BOX 7           // atlas slot to build the sprite in
#define PAL 30          // palette to colour it with

// Fill slot BOX with a solid 64x64 block of colour index `ci` (1..15). The
// atlas is 4bpp: two pixels per byte, so a solid fill is one repeated nibble.
static void paintBox(int ci) {
    unsigned char *px = SpriteSlot(BOX);
    unsigned char b = (unsigned char)((ci << 4) | ci);
    for (int i = 0; i < (int)ROAP_SPRITE_STRIDE; i++) {
        px[i] = b;                 // a plain store -- this is the whole point
    }
}

__attribute__((section(".text.init"))) void _start(void) {
    // A palette with something in every index, so any colour we pick shows.
    for (int i = 1; i < 16; i++) {
        Color col = { (unsigned char)(i * 17), (unsigned char)(i * 53), (unsigned char)(i * 97), 255 };
        SetColorsPaletteColor(PAL, i, col);
    }

    paintBox(1);                   // first paint -- via the mapped atlas
    AssignSprite(0, BOX);
    SetSpriteColorPalette(0, PAL);
    SetSpritePosition(0, 600.0f, 320.0f);
    SetSpriteEnabled(0, 1);
    SetSpriteVisible(0, 1);

    int t = 0, ci = 1;
    for (;;) {
        if (++t >= 30) {           // ~ twice a second at 60Hz
            t = 0;
            ci = (ci % 15) + 1;
            paintBox(ci);          // repaint -- must re-stage, or the box freezes
        }
        VPUYield();
    }
}
