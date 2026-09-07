#include "roap.h"
#include "VPU_roap.h"     // VPUInputPage
#include "roap_mmio.h"
#include "CAKE/cake.h"
#include <string.h>

// roap_mmio.h defines the guest's view of input deliberately without including
// any CAKE header -- that's what keeps the guest ABI independent of the host's
// internals. The cost is two parallel definitions that have to agree, and
// nothing but these assertions to notice when they stop agreeing. They are
// cheap and they fail at compile time, which is the only time worth failing.
_Static_assert(ROAP_NKEYS     >= CAKE_KEY_TABLE_SIZE,     "MMIO key page too small for CAKE's key table");
_Static_assert(ROAP_NPADS     >= CAKE_CONTROLLER_MAX,     "MMIO pad array too small for CAKE's controller slots");
_Static_assert(ROAP_NMOUSEBTN >= CAKE_MOUSE_BUTTON_COUNT, "MMIO mouse buttons too few for CAKE's");

// roap_mmio.h says its pad masks mirror CAKE_BUTTON_*, which is what lets the
// bitmask be copied straight across instead of translated bit by bit below.
_Static_assert(ROAP_PAD_DPAD_UP    == CAKE_BUTTON_DPAD_UP,        "pad mask drift: DPAD_UP");
_Static_assert(ROAP_PAD_DPAD_DOWN  == CAKE_BUTTON_DPAD_DOWN,      "pad mask drift: DPAD_DOWN");
_Static_assert(ROAP_PAD_DPAD_LEFT  == CAKE_BUTTON_DPAD_LEFT,      "pad mask drift: DPAD_LEFT");
_Static_assert(ROAP_PAD_DPAD_RIGHT == CAKE_BUTTON_DPAD_RIGHT,     "pad mask drift: DPAD_RIGHT");
_Static_assert(ROAP_PAD_START      == CAKE_BUTTON_START,          "pad mask drift: START");
_Static_assert(ROAP_PAD_BACK       == CAKE_BUTTON_BACK,           "pad mask drift: BACK");
_Static_assert(ROAP_PAD_LTHUMB     == CAKE_BUTTON_LEFT_THUMB,     "pad mask drift: LTHUMB");
_Static_assert(ROAP_PAD_RTHUMB     == CAKE_BUTTON_RIGHT_THUMB,    "pad mask drift: RTHUMB");
_Static_assert(ROAP_PAD_LSHOULDER  == CAKE_BUTTON_LEFT_SHOULDER,  "pad mask drift: LSHOULDER");
_Static_assert(ROAP_PAD_RSHOULDER  == CAKE_BUTTON_RIGHT_SHOULDER, "pad mask drift: RSHOULDER");
_Static_assert(ROAP_PAD_A          == CAKE_BUTTON_A,              "pad mask drift: A");
_Static_assert(ROAP_PAD_B          == CAKE_BUTTON_B,              "pad mask drift: B");
_Static_assert(ROAP_PAD_X          == CAKE_BUTTON_X,              "pad mask drift: X");
_Static_assert(ROAP_PAD_Y          == CAKE_BUTTON_Y,              "pad mask drift: Y");

void RoapPumpInput(void) {
    RoapInput *in = (RoapInput *)VPUInputPage();

    // CAKE_Keys is bool[]; the page is uint8_t 0/1. Normalise rather than
    // memcpy: bool's representation is not the guest's business.
    for (int i = 0; i < CAKE_KEY_TABLE_SIZE; i++) {
        in->keys[i] = CAKE_Keys[i] ? 1u : 0u;
    }

    in->mouseX     = CAKE_MouseDeltaX;
    in->mouseY     = CAKE_MouseDeltaY;
    in->mouseWheel = CAKE_MouseWheel;

    // The page reserves more buttons than CAKE reports; clear the whole run so
    // the surplus reads as released rather than as whatever was there before.
    memset(in->mouseButtons, 0, sizeof in->mouseButtons);
    for (int b = 0; b < CAKE_MOUSE_BUTTON_COUNT; b++) {
        in->mouseButtons[b] = CAKE_MouseButtons[b] ? 1u : 0u;
    }

    for (int p = 0; p < ROAP_NPADS; p++) {
        // NULL for any slot that isn't connected -- zero the whole entry so a
        // pad unplugged mid-game reads as absent with neutral sticks, not as
        // its last live state frozen in place.
        const CAKE_ControllerState *s = CAKE_GetControllerState(p);
        if (!s) {
            memset(&in->pads[p], 0, sizeof in->pads[p]);
            continue;
        }
        in->pads[p].connected = 1;
        in->pads[p].lt        = s->leftTrigger;
        in->pads[p].rt        = s->rightTrigger;
        in->pads[p]._pad      = 0;
        in->pads[p].buttons   = s->buttons;   // masks asserted identical above
        in->pads[p].lx        = s->thumbLeftX;
        in->pads[p].ly        = s->thumbLeftY;
        in->pads[p].rx        = s->thumbRightX;
        in->pads[p].ry        = s->thumbRightY;
    }
}
