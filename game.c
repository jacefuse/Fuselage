
#include "fuselage/fuselage.h"
#include "game.h"
#include <stdlib.h>   // rand() below (was reaching game.c transitively on Windows)

#ifdef FUSELAGE_ROAP_EMBEDDED
// A guest source (misl.c) is present, so game.c's tick flips from "run myself"
// to "spend a time-slice in the baked ROAP." misl.c was cross-compiled to
// game.roap and embedded as game_roap_blob.h; roap_embedded.h wraps that into
// the game_roap the VPU runs. game.c keeps one stable identity either way --
// only this branch changes.
#include "fuselage/ROAP/roap_embedded.h"
#include "fuselage/ROAP/roap.h"

void game(void) {
    RoapPumpInput();   // publish this tick's input into the guest's MMIO page
    VPU(&game_roap);   // spend one time-slice in the baked ROAP
}
#else
void game(void) {
    if (CAKE_Keys[CAKE_KEY_ESCAPE]) { FuselageSignal(FUSELAGE_QUIT); }

    // Text layer test
    // This clip of code gets the current cursor position and stores it.
    // It then generates a random printable character and chooses a color
    // for it. This is what our console input cursor should look like.
    int loc=tlGetCursor();

    int showme = rand() % (128 - 32) + 32;      // This rand() call should be
                                                // replaced with a DICE call.
    if (showme==127) showme = 255;
    tlSetColor(GetColorsColorFromChar(rand() % 256)); // This rand(), too.
    tlPrintChar(showme);

    if (loc >= 0 && loc < 3600){                // This will create a constantly
        int locx = loc % 80;                    // changing character.
        int locy = loc / 80;
        tlSetCursor(locx, locy);
    }
    tlSetColor(GRAY);

    return; // End of main execution loop for a C program.
            // For a MISL program this loop typically only
            // invoke the VPU's time slice.
}
#endif
