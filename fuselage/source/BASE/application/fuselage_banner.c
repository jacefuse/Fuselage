#include "../core/fuselage_internal.h"
#include "../../MISL/host/misl_host.h"
void fuselage_print_banner(void) {
    tlPrintFormattedC(WHITE, "[Fuselage] Version %s\n\n", FUSELAGE_VERSION);
    tlPrintFormattedC(GRAY, "[Text Layer] Version %s\n", GDMF_TEXTLAYER_VERSION);
    tlPrintFormattedC(CYAN, "[GDMF] Version %s\n", GDMF_VERSION);
    tlPrintFormattedC(BLUE, "[Sprites] Version %s\n", GDMF_SPRITES_VERSION);
    tlPrintFormattedC(GREEN, "[Tiles] Version %s\n", GDMF_TILES_VERSION);
    tlPrintFormattedC(YELLOW, "[Pixies] Version %s\n", GDMF_PIXIES_VERSION);
    tlPrintFormattedC(ORANGE, "[DICE] Version %s\n", DICE_VERSION);
    tlPrintFormattedC(RED, "[CAKE] Version %s\n", CAKE_VERSION);
    tlPrintFormattedC(MAGENTA, "[Colors] Version %s\n", GDMF_COLORS_VERSION);
    tlPrintFormattedC(WHITE, "[MISL host] Version %s\n", FUSELAGE_GUEST_VERSION);
    tlPrintFormattedC(PINK, "[SHARP] Version %s\n", SHARP_VERSION);
    const char* name = NULL;
    const char* type = NULL;
    int score = 0;
    if (GDMF_GetDeviceInfo(&name, &type, &score)) {
        tlPrintFormattedC(LIGHTGRAY, "[Vulkan] Device: %s (%s, score %d)\n", name ? name : "unknown",
                          type ? type : "unknown", score);
    }
    tlPrintFormattedC(WHITE, "\nREADY!\n");
}
