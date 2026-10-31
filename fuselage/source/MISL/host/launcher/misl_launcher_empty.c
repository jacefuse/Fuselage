/* Missing-default-program screen. SEE: (MISL_LAUNCHER.md, No program supplied) */
#include "misl_launcher_internal.h"
#include "../../../GDMF/textlayer/gdmf_textlayer.h"
#include "../../../CAKE/cake.h"
#include <string.h>

static void fuselage_launcher_print_centered(const char* text, Color color, int row) {
    size_t length = strlen(text);
    int column = length < TEXT_LAYER_WIDTH ? (TEXT_LAYER_WIDTH - (int)length) / 2 : 0;
    tlPrintCP(text, color, column, row);
}

void fuselage_launcher_tick_no_program(void) {
    if (!fuselage_launcher.started) {
        fuselage_launcher.started = true;
        tlCLS();
        tlActivate();
        fuselage_launcher_print_centered(FUSELAGE_VERSION, WHITE, 1);
        int middle = TEXT_LAYER_HEIGHT / 2;
        fuselage_launcher_print_centered("No ROAP was declared and \"GAME.ROAP\" is not present.", RED, middle);
        fuselage_launcher_print_centered("This launcher executes a Fuselage ROAP.", WHITE, middle + 2);
        fuselage_launcher_print_centered("Press any key to quit.", YELLOW, middle + 4);
    }
    for (unsigned key = 0; key < CAKE_KEY_TABLE_SIZE; ++key) {
        if (CAKE_Keys[key] || CAKE_KeysPressed[key]) {
            FuselageSignal(FUSELAGE_QUIT);
            break;
        }
    }
}
