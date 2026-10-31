/* Engine-owned entry point. SEE: (FUSELAGE_BUILD.md, Executable modes) */
#include "../../fuselage.h"
#include "../../SRAM/sram.h"
#include "../../MISL/host/misl_host.h"
#include <stdio.h>
#if defined(FUSELAGE_ROAP_LAUNCHER) && defined(FUSELAGE_EMBEDDED_ROAP)
#error Select one executable mode.
#endif
#if defined(FUSELAGE_ROAP_LAUNCHER)
#include "../../MISL/host/misl_launcher.h"
#else
#include "fuselage_project.h"
#include "fuselage_save.h"
#if defined(FUSELAGE_EMBEDDED_ROAP)
#include "../../MISL/host/runtime/misl_embedded.h"
#include "../../MISL/host/runtime/misl_host_image_internal.h"
static int fuselage_embedded_exit;
static void fuselage_tick_embedded(void) {
    FuselageTickEmbeddedGuest();
    MislGuestHaltInfo halt;
    if (FuselageGetGuestHaltInfo(&halt)) {
        if (halt.kind == MISL_GUEST_HALT_FAULT) {
            fuselage_embedded_exit = 3;
        } else if (halt.kind == MISL_GUEST_HALT_SRAM) {
            fuselage_embedded_exit = 4;
        }
    }
}
#endif
static void fuselage_apply_project_config(void) {
    FuselageSetTitle(fuselage_project.title);
    if (fuselage_project.icon_rgba && fuselage_project.icon_width > 0 && fuselage_project.icon_height > 0) {
        FuselageSetWindowIcon(fuselage_project.icon_width, fuselage_project.icon_height,
                              fuselage_project.icon_rgba);
    }
    if (fuselage_project.canvas_width > 0 && fuselage_project.canvas_height > 0) {
        FuselageSetCanvasResolution(fuselage_project.canvas_width, fuselage_project.canvas_height);
    }
    if (fuselage_project.window_width > 0 && fuselage_project.window_height > 0) {
        FuselageSetResolution(fuselage_project.window_width, fuselage_project.window_height);
    }
    if (fuselage_project.aspect_numerator > 0 && fuselage_project.aspect_denominator > 0) {
        FuselageSetAspectRatio(fuselage_project.aspect_numerator, fuselage_project.aspect_denominator);
    }
    if (fuselage_project.simulation_rate > 0) {
        FuselageSetSimRate(fuselage_project.simulation_rate);
    }
    if (fuselage_project.target_fps > 0) {
        FuselageSetTargetFPS(fuselage_project.target_fps);
    }
    FuselageSetCursorVisible(fuselage_project.cursor_visible != 0);
    FuselageSetBanner(fuselage_project.banner != 0);
    if (fuselage_project.audio_pool_mib > 0) {
        FuselageSetAudioPoolMB(fuselage_project.audio_pool_mib);
    }
}
#if !defined(FUSELAGE_EMBEDDED_ROAP)
static const char* fuselage_save_failure;
static void fuselage_tick_save_failure(void) {
    FuselageTickSaveFailure(fuselage_save_failure);
}
#endif
#endif
int main(int argc, char** argv) {
    int application_exit = 0;
#if defined(FUSELAGE_ROAP_LAUNCHER)
    FuselageSetTitle("MISLlauncher");
    FuselageSetBanner(false);
    int launch = FuselageInitLauncher(argc, argv);
    if (launch != FUSELAGE_LAUNCH_RUN) {
        int result = launch == FUSELAGE_LAUNCH_DONE ? 0 : 1;
        FuselageShutdownLauncher();
        return result;
    }
    FuselageApplyLauncherDisplayMode();
    FuselageSetTickCallback(FuselageTickLauncher);
#else
    fuselage_apply_project_config();
    char program_path[1024];
    if (!SRAM_GetProgramPath(program_path, sizeof program_path, argc > 0 && argv ? argv[0] : NULL)) {
        fprintf(stderr, "[Fuselage] Could not determine the executable path.\n");
        return 1;
    }
    FuselageSetGuestProgramPath(program_path);
#if defined(FUSELAGE_EMBEDDED_ROAP)
    MislHostImagePlan plan;
    MislHostImageResult preflight = fuselage_guest_plan_image(FuselageGetEmbeddedGuest(), 0, &plan);
    if (preflight != MISL_HOST_IMAGE_OK) {
        fprintf(stderr, "[Fuselage] %s\n", fuselage_guest_get_image_result_message(preflight));
        return 1;
    }
    /* The guest loader alone opens the save declared by its ROAP manifest. */
    FuselageSetTickCallback(fuselage_tick_embedded);
#else
    bool runnable =
        SRAM_Configure(fuselage_project.save_id, fuselage_project.save_version, fuselage_project.save_bytes);
    if (runnable && fuselage_project.save_bytes) {
        runnable = SRAM_Open(program_path);
    }
    if (!runnable) {
        fuselage_save_failure = SRAM_GetFailureReason();
        fprintf(stderr, "[SRAM] %s\n", fuselage_save_failure);
        application_exit = 4;
    }
    FuselageSetTickCallback(runnable ? tick : fuselage_tick_save_failure);
#endif
#endif
    while (fuselage()) {
    }
    int root_exit = FuselageGetExitCode();
#if defined(FUSELAGE_ROAP_LAUNCHER)
    application_exit = FuselageGetLauncherExitCode();
    FuselageShutdownLauncher();
#elif defined(FUSELAGE_EMBEDDED_ROAP)
    application_exit = fuselage_embedded_exit;
#endif
    return root_exit ? root_exit : application_exit;
}
