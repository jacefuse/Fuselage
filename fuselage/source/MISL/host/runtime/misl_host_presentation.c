/* Host-controlled halt view. SEE: (MISL_HOST_RUNTIME.md, Halt presentation) */
#include "../misl_host.h"
#include "../../../BASE/fuselage_runtime.h"
#include "../../../BASE/application/fuselage_save.h"
#include "../../../CAKE/cake.h"
#include "../../../CAKE/cake_help.h"
#include "../../../GDMF/gdmf_colors.h"
#include "../../../GDMF/textlayer/gdmf_textlayer.h"
#include <string.h>
static FuselageGuestHaltAction fuselage_guest_halt_action = FUSELAGE_GUEST_HALT_ESCAPE;
void FuselageSetGuestHaltAction(FuselageGuestHaltAction action) {
    fuselage_guest_halt_action = action;
}
FuselageGuestHaltAction FuselageGetGuestHaltAction(void) {
    return fuselage_guest_halt_action;
}
void FuselageHandleGuestHalt(void) {
    MislGuestHaltInfo halt;
    if (fuselage_guest_halt_action == FUSELAGE_GUEST_HALT_IGNORE || !FuselageGetGuestHaltInfo(&halt)) {
        return;
    }
    if (halt.kind == MISL_GUEST_HALT_SRAM) {
        FuselageTickSaveFailure(halt.reason);
        return;
    }
    if (CAKE_IsKeyPressed(CAKE_KEY_ESCAPE)) {
        FuselageSignal(FUSELAGE_QUIT);
        return;
    }
    const Color white = {255, 255, 255, 255}, red = {255, 80, 80, 255}, gray = {170, 170, 170, 255};
    bool finished = halt.kind == MISL_GUEST_HALT_FINISHED;
    tlActivate();
    char blank[TEXT_LAYER_WIDTH + 1];
    memset(blank, ' ', TEXT_LAYER_WIDTH);
    blank[TEXT_LAYER_WIDTH] = '\0';
    for (int row = 20; row <= 24; row++) {
        tlPrintCP(blank, gray, 0, row);
    }
    tlPrintCP(finished ? "The program has finished." : "The program stopped.", finished ? white : red, 2, 20);
    char displayed_reason[TEXT_LAYER_WIDTH - 1];
    const char* reason = halt.reason ? halt.reason : "No reason was recorded.";
    size_t copied = 0;
    while (copied + 1 < sizeof displayed_reason && reason[copied]) {
        displayed_reason[copied] = reason[copied];
        copied++;
    }
    displayed_reason[copied] = '\0';
    tlPrintCP(displayed_reason, gray, 2, 21);
    if (!finished) {
        tlSetCursor(2, 22);
        tlPrintFormattedC(gray, "pc 0x%08X   opcode 0x%08X   sp 0x%08X   ra 0x%08X", halt.program_counter,
                          halt.instruction, halt.stack_pointer, halt.return_address);
    }
    tlPrintCP("Press ESC to exit.", white, 2, 24);
}
