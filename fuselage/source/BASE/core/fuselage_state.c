#include "fuselage_internal.h"
FuselageRootState fuselage_root = {.phase = FUSELAGE_COLD,
                                   .simulation_rate = 60,
                                   .input_requires_focus = true,
                                   .focus_policy_changed = true,
                                   .previously_focused = true,
                                   .banner = true};
int FuselageGetExitCode(void) {
    return fuselage_root.exit_code;
}
