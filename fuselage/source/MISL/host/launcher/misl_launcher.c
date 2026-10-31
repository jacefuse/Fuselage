/* Launcher lifecycle. SEE: (MISL_LAUNCHER.md, Lifecycle) */
#include "misl_launcher_internal.h"
#include "../runtime/misl_host_state_internal.h"
#include "../runtime/misl_host_save_internal.h"
#include "../../../SRAM/sram.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
FuselageLauncherState fuselage_launcher;
int FuselageGetLauncherExitCode(void) {
    return fuselage_launcher.exit_code;
}
void FuselageShutdownLauncher(void) {
    if (fuselage_guest_session.image == &fuselage_launcher.image) {
        fuselage_guest_release_memory();
        fuselage_guest_close_save();
    }
    free(fuselage_launcher.bytes);
    fuselage_launcher.bytes = NULL;
    fuselage_launcher.image = (MislGuestImage){0};
    fuselage_launcher.run_ready = false;
}
int FuselageInitLauncher(int argc, char** argv) {
    FuselageShutdownLauncher();
    fuselage_launcher =
        (FuselageLauncherState){.display_mode = FUSELAGE_WINDOWED, .exit_code = FUSELAGE_LAUNCH_EXIT_FAILED};
    const char* path;
    bool info_only;
    int result = fuselage_launcher_parse(argc, argv, &path, &info_only);
    if (result != FUSELAGE_LAUNCH_RUN) {
        if (result == FUSELAGE_LAUNCH_DONE) {
            fuselage_launcher.exit_code = FUSELAGE_LAUNCH_EXIT_OK;
        }
        return result;
    }
    char default_path[1024];
    bool using_default = path == NULL;
    if (!path) {
        if (!fuselage_launcher_default_path(default_path, sizeof default_path,
                                            argc > 0 && argv ? argv[0] : NULL)) {
            fprintf(stderr, "MISLlauncher: cannot locate GAME.ROAP beside the executable.\n");
            return FUSELAGE_LAUNCH_FAILED;
        }
        path = default_path;
    }
    if (!fuselage_launcher_load(path)) {
        if (using_default && !info_only && fuselage_launcher.load_error == ENOENT) {
            fuselage_launcher.no_program = true;
            FuselageSetTitle("MISLlauncher");
            FuselageSetBanner(false);
            return FUSELAGE_LAUNCH_RUN;
        }
        return FUSELAGE_LAUNCH_FAILED;
    }
    fuselage_launcher_decode_manifest();
    if (info_only) {
        fuselage_launcher_print_info();
        fuselage_launcher.exit_code = FUSELAGE_LAUNCH_EXIT_OK;
        return FUSELAGE_LAUNCH_DONE;
    }
    if (!SRAM_SetSaveDirectory(fuselage_launcher.save_directory)) {
        fprintf(stderr, "MISLlauncher: %s\n", SRAM_GetFailureReason());
        FuselageShutdownLauncher();
        return FUSELAGE_LAUNCH_FAILED;
    }
    FuselageSetGuestProgramPath(fuselage_launcher.path);
    FuselageSetGuestCaptureDirectory(fuselage_launcher.capture_directory);
    FuselageSetGuestMemoryCapMB(fuselage_launcher.memory_cap_mib);
    FuselageSetTitle("MISLlauncher");
    FuselageSetBanner(false);
    fuselage_launcher_apply_manifest();
    fuselage_launcher.exit_code = FUSELAGE_LAUNCH_EXIT_OK;
    fuselage_launcher.run_ready = true;
    return FUSELAGE_LAUNCH_RUN;
}
void FuselageApplyLauncherDisplayMode(void) {
    if (fuselage_launcher.run_ready && fuselage_launcher.display_chosen) {
        FuselageSignal(fuselage_launcher.display_mode);
    }
}
