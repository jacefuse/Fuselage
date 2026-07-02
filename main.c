// Fuselage BUTTOCKS - minimal entry point
// The full feature-test program lives at examples/MessyTest/messytest.c.

#include "fuselage.h"

int main(void) {
    FuselageSetTitle("Fuselage");
    FuselageSetResolution(1280, 720);
    FuselageSetAspectRatio(16, 9);

    while (fuselage()) {
        if (CAKE_Keys[CAKE_KEY_ESCAPE]) { FuselageSignal(FUSELAGE_QUIT); }
    }

    return 0;
}