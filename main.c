// Fuselage - minimal entry point
// Game logic lives in game() -- see game.c and FuselageSetTickCallback below.

#include "fuselage.h"
#include "game.h"
#include "ROAP/VPU_roap.h"     // VPU_VERSION for the banner
#ifdef FUSELAGE_ROAP_LAUNCHER
#include "ROAP/roap_launcher.h"
#endif

// argc/argv are taken in every configuration but only read by MISLlauncher
// (make launcher), which resolves its blob from the command line. The other
// two -- a C game, or a ROAP baked in at compile time -- have nothing to read.
int main(int argc, char **argv) {
#ifdef FUSELAGE_ROAP_LAUNCHER
    // Load before bring-up: if there's no blob to play, say so on the console
    // and leave, rather than opening a window on nothing.
    if (!RoapLauncherInit(argc, argv)) { return 1; }
    FuselageSetTitle("MISLlauncher");
#else
    (void)argc; (void)argv;
    FuselageSetTitle("Fuselage");
#endif

    // Fuselage has a fixed 1280x720 / 16:9 native resolution, like a retro
    // console -- games normally shouldn't change it, so nothing is set here.
    // The window stays locked to the aspect ratio as the user resizes it, and
    // the native canvas scales to fill it. If your game deliberately wants a
    // different native resolution or aspect (a low-res pixel-art canvas scaled
    // up, TATE/vertical, 4:3, ultrawide, ...), set it here, before the loop:
    //
    //   FuselageSetCanvasResolution(256, 192); // native design resolution (default 1280x720)
    //   FuselageSetAspectRatio(4, 3);          // display aspect: 4:3, 9:16 (TATE), 21:9 (ultrawide), ...
    //   FuselageSetResolution(1920, 1080);     // initial window size (still kept to the aspect ratio)

    FuselageSetTickCallback(game);

#ifndef FUSELAGE_ROAP_LAUNCHER
    // Starter-template demo only: the text layer (Layer0) is INACTIVE by default;
    // this brings it up and prints the engine version banner so a fresh project
    // shows something on first run. The MISLlauncher deliberately SKIPS this --
    // it's a generic host for someone else's ROAP and must leave the text layer
    // in its default-off state for the ROAP to control. (Without this guard the
    // banner flashed over every ROAP at startup until the ROAP's own game() got a
    // tick in to deactivate it.)
    //
    // This Text Layer is not intended for application text -- use Sprites, Tiles,
    // or the Pixie Layers for that. tlPrint buffers text even before INIT; it
    // displays once the layer is live.
    tlActivate();
    tlPrintFormattedC(WHITE, "[Fuselage] Version %s\n\n", FUSELAGE_VERSION);
    tlPrintFormattedC(GRAY, "[Text Layer] Version %s\n", GDMF_TEXTLAYER_VERSION);
    tlPrintFormattedC(CYAN, "[GDMF] Version %s\n", GDMF_VERSION);
    tlPrintFormattedC(BLUE, "[Sprites] Version %s\n", GDMF_SPRITES_VERSION);
    tlPrintFormattedC(GREEN, "[Tiles] Version %s\n", GDMF_TILES_VERSION);
    tlPrintFormattedC(YELLOW, "[Pixies] Version %s\n", GDMF_PIXIES_VERSION);
    tlPrintFormattedC(ORANGE, "[DICE] Version %s\n", DICE_VERSION);
    tlPrintFormattedC(RED, "[CAKE] Version %s\n", CAKE_VERSION);
    tlPrintFormattedC(MAGENTA, "[Colors] Version %s\n", GDMF_COLORS_VERSION);
    tlPrintFormattedC(WHITE, "[VPU] Version %s\n", VPU_VERSION);
#endif

    while (fuselage()) { }

#ifdef FUSELAGE_ROAP_LAUNCHER
    RoapLauncherShutdown();
#endif

    return 0;
}
