/* Save-load refusal view. SEE: (MISL_HOST_RUNTIME.md, Halt presentation) */
#include "fuselage_save.h"
#include "../fuselage_runtime.h"
#include "../../CAKE/cake.h"
#include "../../CAKE/cake_help.h"
#include "../../GDMF/gdmf_colors.h"
#include "../../GDMF/textlayer/gdmf_textlayer.h"
#include <string.h>

static void fuselage_print_centered(const char* text, Color color, int columns, int row) {
    int column = (columns - (int)strlen(text)) / 2;
    tlPrintCP(text, color, column > 0 ? column : 0, row > 0 ? row : 0);
}
void FuselageTickSaveFailure(const char* reason) {
    if (CAKE_IsKeyPressed(CAKE_KEY_ESCAPE)) {
        FuselageSignal(FUSELAGE_QUIT);
        return;
    }
    int columns = TEXT_LAYER_WIDTH, rows = TEXT_LAYER_HEIGHT;
    int row = rows / 2 - 4;
    const Color white = {255, 255, 255, 255}, red = {255, 80, 80, 255}, yellow = {255, 220, 80, 255};
    tlActivate();
    tlCLS();
    fuselage_print_centered("[SRAM FAILURE]", white, columns, row);
    fuselage_print_centered("The save file failed to load.", red, columns, row + 2);
    fuselage_print_centered(reason && *reason ? reason : "No reason was recorded.", yellow, columns, row + 4);
    fuselage_print_centered("The program has not run. Check the save files before retrying.", red, columns,
                            row + 6);
    fuselage_print_centered("Press [ESC] to exit.", white, columns, row + 8);
}
