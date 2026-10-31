#include "fuselage.h"
#include <stdlib.h>

void tick(void) {
    static bool started = false;
    if (!started) {
        started = true;
        tlActivate();
    }
    if (CAKE_IsKeyPressed(CAKE_KEY_ESCAPE)) {
        FuselageSignal(FUSELAGE_QUIT);
    }

    // Keep the starter character at the cursor without advancing through the banner.
    int cursor_position = tlGetCursor();
    int character = rand() % (128 - 32) + 32;
    if (character == 127) {
        character = 255;
    }
    tlSetColor(GetColorsColorFromChar((unsigned char)(rand() % 256)));
    tlPrintChar((unsigned char)character);
    if (cursor_position >= 0 && cursor_position < TEXT_LAYER_WIDTH * TEXT_LAYER_HEIGHT) {
        tlSetCursor(cursor_position % TEXT_LAYER_WIDTH, cursor_position / TEXT_LAYER_WIDTH);
    }
    tlSetColor(GRAY);
}
