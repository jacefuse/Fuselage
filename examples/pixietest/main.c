// PixieTest - dedicated pixie subsystem test.
// Three pixies stacked by priority: a Mode 0 "space" background (packed
// PALETTE4BPP, stretched fullscreen), a Mode 0 "rt" foreground (packed
// RLE_PALETTE_SHARED, shown at its native 128x128 in the center), and a
// Mode 1 "live" fireworks layer in between the two, driven entirely by
// PIXIE_OP_DRAW's zero-length-line "point" trick every frame.
//
// F11 toggles fullscreen; 1/2/3 toggle the space/fireworks/rt pixies'
// visibility independently, so each layer can be inspected on its own.

#include "fuselage.h"
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "space.h"
#include "rt.h"

static const int screenWidth  = 1280;
static const int screenHeight = 720;

#define PIXIE_SPACE_ID     0
#define PIXIE_FIREWORKS_ID 1
#define PIXIE_RT_ID        2

// Priority 0 is closest/frontmost (drawn last); higher priority is
// further back (drawn first) -- see gdmf_vulkan.c's band-interleaved
// render loop. Space is the backdrop, rt sits on top, fireworks occupy
// the space between the two.
#define SPACE_PRIORITY     255
#define FIREWORKS_PRIORITY 128
#define RT_PRIORITY        0

// Arbitrary GDMF shared-palette slots -- nothing else in this standalone
// test uses palettes, so any two distinct slots are fine.
#define SPACE_PALETTE_SLOT 1
#define RT_PALETTE_SLOT    2

#define FIREWORKS_OUTPUT_W screenWidth
#define FIREWORKS_OUTPUT_H screenHeight

#define FIREWORK_TWO_PI 6.28318530718f

#define MAX_FIREWORKS          20
#define FIREWORK_MAX_PARTICLES 100
#define FIREWORK_RISE_SPEED    8.0f
#define FIREWORK_GRAVITY       0.15f
#define FIREWORK_RISER_SIZE    6

// "Mostly opaque, but a little see-through" -- not a full 255, so the
// space/rt pixies behind and in front of this flash still show through
// faintly for one frame.
#define FIREWORK_FLASH_ALPHA   128

typedef enum {
    FIREWORK_RISING,
    FIREWORK_EXPLODING,
    FIREWORK_DONE,       // exploded and faded out -- counting down to respawn
} FireworkState;

typedef struct {
    float x, y;
    float vx, vy;
    float life;     // frames remaining
    float maxLife;
    int   size;
} Particle;

typedef struct {
    FireworkState state;
    float    x, y;          // riser position while RISING; burst origin while EXPLODING
    float    targetY;       // altitude (smaller y) the riser explodes at
    Color    color;         // this burst's color, picked at launch
    Particle particles[FIREWORK_MAX_PARTICLES];
    int      particleCount;
    int      cooldownFrames; // DONE only -- frames left before respawn
} Firework;

static Firework fireworks[MAX_FIREWORKS];

static const Color kFireworkColors[] = {
    { 255,  80,  80, 255 },  // red
    { 255, 200,  60, 255 },  // gold
    { 100, 200, 255, 255 },  // sky blue
    { 180, 100, 255, 255 },  // violet
    { 120, 255, 140, 255 },  // green
    { 255, 140, 220, 255 },  // pink
};
#define FIREWORK_COLOR_COUNT (int)(sizeof(kFireworkColors) / sizeof(kFireworkColors[0]))

// 0..1 with the same rand()%1000 precision the other examples use
// (see DriftingPlots' entity angle/phase randomization).
static float RandomUnit(void) {
    return (float)(rand() % 1000) / 1000.0f;
}
static float RandomRange(float lo, float hi) {
    return lo + RandomUnit() * (hi - lo);
}

// Packs a signed 2D point the same way gdmf_pixies.c's DRAW opcode
// expects: high 16 bits = x, low 16 bits = y (see pixie_unpack_xy).
static uint32_t PackXY(int x, int y) {
    return ((uint32_t)(uint16_t)x << 16) | (uint32_t)(uint16_t)y;
}

// Resets fireworks[i] to a fresh RISING launch: random x, random
// explosion altitude, random burst color -- called both at startup
// (staggered via cooldownFrames below) and every time a burst finishes.
static void SpawnFirework(int i) {
    Firework* fw = &fireworks[i];
    fw->state         = FIREWORK_RISING;
    fw->x             = RandomRange((float)FIREWORKS_OUTPUT_W * 0.1f, (float)FIREWORKS_OUTPUT_W * 0.9f);
    fw->y             = (float)FIREWORKS_OUTPUT_H - 10.0f;
    fw->targetY       = RandomRange((float)FIREWORKS_OUTPUT_H * 0.10f, (float)FIREWORKS_OUTPUT_H * 0.40f);
    fw->color         = kFireworkColors[rand() % FIREWORK_COLOR_COUNT];
    fw->particleCount = 0;
    fw->cooldownFrames = 0;
}

// Called the instant a riser reaches its target altitude -- spawns a
// radial burst of smaller particles, each with its own random direction/
// speed/lifetime, all sharing this firework's one launch color.
static void ExplodeFirework(Firework* fw) {
    fw->state = FIREWORK_EXPLODING;
    fw->particleCount = FIREWORK_MAX_PARTICLES;
    for (int p = 0; p < FIREWORK_MAX_PARTICLES; p++) {
        Particle* pt = &fw->particles[p];
        float angle = RandomRange(0.0f, FIREWORK_TWO_PI);
        float speed = RandomRange(3.0f, 8.0f);
        pt->x = fw->x;
        pt->y = fw->y;
        pt->vx = cosf(angle) * speed;
        pt->vy = sinf(angle) * speed;
        pt->maxLife = RandomRange(35.0f, 65.0f);
        pt->life = pt->maxLife;
        pt->size = 2 + rand() % 5; // 1-3, deliberately smaller than the riser
    }
}

// Advances every firework one frame. Sets *explodedThisFrame if any
// firework transitioned RISING -> EXPLODING this frame, so the caller can
// trigger the one-frame flash on the fireworks pixie itself.
static void UpdateFireworks(bool* explodedThisFrame) {
    *explodedThisFrame = false;

    for (int i = 0; i < MAX_FIREWORKS; i++) {
        Firework* fw = &fireworks[i];
        switch (fw->state) {
            case FIREWORK_RISING:
                fw->y -= FIREWORK_RISE_SPEED;
                if (fw->y <= fw->targetY) {
                    ExplodeFirework(fw);
                    *explodedThisFrame = true;
                }
                break;

            case FIREWORK_EXPLODING: {
                bool anyAlive = false;
                for (int p = 0; p < fw->particleCount; p++) {
                    Particle* pt = &fw->particles[p];
                    if (pt->life <= 0.0f) { continue; }
                    pt->vy += FIREWORK_GRAVITY;
                    pt->x  += pt->vx;
                    pt->y  += pt->vy;
                    pt->life -= 1.0f;
                    if (pt->life > 0.0f) { anyAlive = true; }
                }
                if (!anyAlive) {
                    fw->state = FIREWORK_DONE;
                    fw->cooldownFrames = 20 + rand() % 80;
                }
                break;
            }

            case FIREWORK_DONE:
                fw->cooldownFrames--;
                if (fw->cooldownFrames <= 0) {
                    SpawnFirework(i);
                }
                break;
        }
    }
}

// Rebuilds this frame's fireworks pixie contents from scratch -- Mode 1
// remembers nothing between frames, so every riser/particle still alive
// has to be redrawn every single frame regardless of whether anything
// about it changed.
static void RenderFireworks(bool explodedThisFrame) {
    if (explodedThisFrame) {
        uint32_t flashArgs[4] = { PackRGBA8((Color){ 255, 255, 255, FIREWORK_FLASH_ALPHA }), 0, 0, 0 };
        PixieCommand(PIXIE_FIREWORKS_ID, PIXIE_OP_CLEAR, 0x0001 /* use-color bit */, flashArgs);
    } else {
        uint32_t clearArgs[4] = { 0, 0, 0, 0 };
        PixieCommand(PIXIE_FIREWORKS_ID, PIXIE_OP_CLEAR, 0 /* transparent, ignores args */, clearArgs);
    }

    for (int i = 0; i < MAX_FIREWORKS; i++) {
        Firework* fw = &fireworks[i];

        if (fw->state == FIREWORK_RISING) {
            uint32_t point = PackXY((int)fw->x, (int)fw->y);
            uint32_t args[4] = { point, point, PackRGBA8(WHITE), FIREWORK_RISER_SIZE };
            PixieCommand(PIXIE_FIREWORKS_ID, PIXIE_OP_DRAW, 0, args);
        } else if (fw->state == FIREWORK_EXPLODING) {
            for (int p = 0; p < fw->particleCount; p++) {
                Particle* pt = &fw->particles[p];
                if (pt->life <= 0.0f) { continue; }

                Color c = fw->color;
                c.a = (unsigned char)(255.0f * (pt->life / pt->maxLife)); // fade out over lifetime

                uint32_t point = PackXY((int)pt->x, (int)pt->y);
                uint32_t args[4] = { point, point, PackRGBA8(c), (uint32_t)pt->size };
                PixieCommand(PIXIE_FIREWORKS_ID, PIXIE_OP_DRAW, 0, args);
            }
        }
    }
}

// Mode 0, "call once, display forever" -- background, stretched from its
// native 320x200 up to the full 1280x720 screen (significantly upscaled,
// on purpose: see pixie_plans.txt's Mode 0 blocky-upscale success case).
static void InitSpacePixie(void) {
    InitPixie(PIXIE_SPACE_ID, PIXIE_MODE_TEXTURE, SPACE_PIXIE_WIDTH, SPACE_PIXIE_HEIGHT);
    SetPixiePosition(PIXIE_SPACE_ID, 0, 0);
    SetPixieDisplaySize(PIXIE_SPACE_ID, screenWidth, screenHeight);
    SetPixiePriority(PIXIE_SPACE_ID, SPACE_PRIORITY);
    SetPixieEnabled(PIXIE_SPACE_ID, true);

    PixieWrite(PIXIE_SPACE_ID, SPACE_OFFSET, space_pixie_data, sizeof(space_pixie_data));

    // PALETTE4BPP resolves indices against a GDMF shared palette slot the
    // same way RLE_PALETTE_SHARED does -- load the packer-emitted colors
    // into one before calling UNPACK.
    for (int i = 0; i < SPACE_PIXIE_PALETTE_COUNT; i++) {
        SetPalette(SPACE_PALETTE_SLOT, (unsigned char)i, space_pixie_palette[i]);
    }

    uint32_t unpackArgs[4] = { SPACE_OFFSET, 0 /* dst (0,0) */, SPACE_PALETTE_SLOT, 0 };
    PixieCommand(PIXIE_SPACE_ID, PIXIE_OP_UNPACK, SPACE_PIXIE_FORMAT, unpackArgs);

    ShowPixie(PIXIE_SPACE_ID);
}

// Mode 0, shown at its native 128x128 -- no scaling, centered on screen.
static void InitRTPixie(void) {
    InitPixie(PIXIE_RT_ID, PIXIE_MODE_TEXTURE, RT_PIXIE_WIDTH, RT_PIXIE_HEIGHT);
    SetPixiePosition(PIXIE_RT_ID, (screenWidth - RT_PIXIE_WIDTH) / 2, (screenHeight - RT_PIXIE_HEIGHT) / 2);
    SetPixieDisplaySize(PIXIE_RT_ID, RT_PIXIE_WIDTH, RT_PIXIE_HEIGHT);
    SetPixiePriority(PIXIE_RT_ID, RT_PRIORITY);
    SetPixieEnabled(PIXIE_RT_ID, true);

    PixieWrite(PIXIE_RT_ID, RT_OFFSET, rt_pixie_data, sizeof(rt_pixie_data));

    for (int i = 0; i < RT_PIXIE_PALETTE_COUNT; i++) {
        SetPalette(RT_PALETTE_SLOT, (unsigned char)i, rt_pixie_palette[i]);
    }

    uint32_t unpackArgs[4] = { RT_OFFSET, 0 /* dst (0,0) */, RT_PALETTE_SLOT, 0 };
    PixieCommand(PIXIE_RT_ID, PIXIE_OP_UNPACK, RT_PIXIE_FORMAT, unpackArgs);

    ShowPixie(PIXIE_RT_ID);
}

// Mode 1 ("live") -- covers the full screen 1:1 (no scaling) so DRAW's
// canvas coordinates line up directly with screen pixels. Fireworks are
// given staggered initial cooldowns rather than all launching on frame 1,
// so they read as continuous and independent rather than lockstep.
static void InitFireworksPixie(void) {
    InitPixie(PIXIE_FIREWORKS_ID, PIXIE_MODE_LIVE, FIREWORKS_OUTPUT_W, FIREWORKS_OUTPUT_H);
    SetPixiePosition(PIXIE_FIREWORKS_ID, 0, 0);
    SetPixieDisplaySize(PIXIE_FIREWORKS_ID, FIREWORKS_OUTPUT_W, FIREWORKS_OUTPUT_H);
    SetPixiePriority(PIXIE_FIREWORKS_ID, FIREWORKS_PRIORITY);
    SetPixieEnabled(PIXIE_FIREWORKS_ID, true);
    ShowPixie(PIXIE_FIREWORKS_ID);

    for (int i = 0; i < MAX_FIREWORKS; i++) {
        fireworks[i].state = FIREWORK_DONE;
        fireworks[i].cooldownFrames = rand() % 150;
    }
}

int main(void) {
    srand((unsigned int)time(NULL));

    FuselageSetTitle("Fuselage - PixieTest");
    FuselageSetResolution(screenWidth, screenHeight);
    FuselageSetAspectRatio(16, 9);

    bool initialized = false;

    bool fullscreen  = false;
    bool f11WasHeld  = false;

    bool spaceVisible     = true;
    bool fireworksVisible = true;
    bool rtVisible        = true;
    bool key1WasHeld = false, key2WasHeld = false, key3WasHeld = false;

    TextLayerInactive();

    while (fuselage()) {
        if (CAKE_Keys[CAKE_KEY_ESCAPE]) {
            FuselageSignal(FUSELAGE_QUIT);
        }

        // Edge-triggered so holding F11 doesn't toggle every single frame.
        bool f11Held = CAKE_Keys[CAKE_KEY_F11];
        if (f11Held && !f11WasHeld) {
            fullscreen = !fullscreen;
            FuselageSignal(fullscreen ? FUSELAGE_FULLSCREEN_EXCLUSIVE : FUSELAGE_WINDOWED);
        }
        f11WasHeld = f11Held;

        // The render pass clears to black on its own every frame --
        // nothing to call explicitly for that.
        if (!initialized) {
            InitSpacePixie();
            InitFireworksPixie();
            InitRTPixie();
            initialized = true;
        }

        bool key1Held = CAKE_Keys[CAKE_KEY_1];
        if (key1Held && !key1WasHeld) {
            spaceVisible = !spaceVisible;
            if (spaceVisible) { ShowPixie(PIXIE_SPACE_ID); } else { HidePixie(PIXIE_SPACE_ID); }
        }
        key1WasHeld = key1Held;

        bool key2Held = CAKE_Keys[CAKE_KEY_2];
        if (key2Held && !key2WasHeld) {
            fireworksVisible = !fireworksVisible;
            if (fireworksVisible) { ShowPixie(PIXIE_FIREWORKS_ID); } else { HidePixie(PIXIE_FIREWORKS_ID); }
        }
        key2WasHeld = key2Held;

        bool key3Held = CAKE_Keys[CAKE_KEY_3];
        if (key3Held && !key3WasHeld) {
            rtVisible = !rtVisible;
            if (rtVisible) { ShowPixie(PIXIE_RT_ID); } else { HidePixie(PIXIE_RT_ID); }
        }
        key3WasHeld = key3Held;

        if (initialized) {
            bool explodedThisFrame = false;
            UpdateFireworks(&explodedThisFrame);
            RenderFireworks(explodedThisFrame);
        }
    }

    return 0;
}
