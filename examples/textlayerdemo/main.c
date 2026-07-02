// ScreenSplat - Fuselage's very first test application, recreated.
// Every frame: fill all 3600 text-layer cells with a random printable
// character in a random palette color.
//
// F11 toggles fullscreen. F10 toggles the text layer on/off. Esc quits.

#include "fuselage.h"
#include <stdlib.h>
#include <time.h>

static const int screenWidth  = 1280;
static const int screenHeight = 720;

int main(void) {
    srand((unsigned int)time(NULL));

    FuselageSetTitle("Fuselage - ScreenSplat");
    FuselageSetResolution(screenWidth, screenHeight);
    FuselageSetAspectRatio(16, 9);
    TextLayerActive();

    bool fullscreen = false, f11WasHeld = false;
    bool f10WasHeld = false;

    while (fuselage()) {
        if (CAKE_Keys[CAKE_KEY_ESCAPE]) {
            FuselageSignal(FUSELAGE_QUIT);
        }

        bool f11Held = CAKE_Keys[CAKE_KEY_F11];
        if (f11Held && !f11WasHeld) {
            fullscreen = !fullscreen;
            FuselageSignal(fullscreen ? FUSELAGE_FULLSCREEN_EXCLUSIVE : FUSELAGE_WINDOWED);
        }
        f11WasHeld = f11Held;

        bool f10Held = CAKE_Keys[CAKE_KEY_F10];
        if (f10Held && !f10WasHeld) { TextLayerToggle(); }
        f10WasHeld = f10Held;

        tlHome();
        for (int i = 0; i < 3600; i++) {
            int showme = rand() % (128 - 32) + 32;
            if (showme == 127) { showme = 255; }
            tlSetColor(GetColorFromChar(rand() % 256));
            tlPrintChar((unsigned char)showme);
        }
    }

    return 0;
}
