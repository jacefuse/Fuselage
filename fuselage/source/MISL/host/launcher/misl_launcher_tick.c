/* Run policy and timing reports. SEE: (MISL_LAUNCHER.md, Timing reports) */
#include "misl_launcher_internal.h"
#include "../../../DICE/dice.h"
#include "../../../BASE/pacing/fuselage_pacing.h"
#include <math.h>

static void fuselage_launcher_quote(const char* text) {
    fputc('"', stderr);
    for (const unsigned char* byte = (const unsigned char*)(text ? text : ""); *byte; byte++) {
        if (*byte == '"' || *byte == '\\') {
            fputc('\\', stderr);
            fputc(*byte, stderr);
        } else if (*byte == '\n') {
            fputs("\\n", stderr);
        } else if (*byte == '\r') {
            fputs("\\r", stderr);
        } else if (*byte == '\t') {
            fputs("\\t", stderr);
        } else if (*byte < 32 || *byte == 127) {
            fprintf(stderr, "\\x%02X", *byte);
        } else {
            fputc(*byte, stderr);
        }
    }
    fputc('"', stderr);
}
static void fuselage_launcher_report_duration(double elapsed) {
    if (elapsed >= 1) {
        double fps = FuselageGetCurrentFPS();
        if (isfinite(fps) && fps >= 0) {
            if (!fuselage_launcher.sample_count) {
                fuselage_launcher.fps_min = fuselage_launcher.fps_max = fps;
                fuselage_launcher.sample_steps = FuselageGetGuestStepCount();
                fuselage_launcher.sample_start = elapsed;
            }
            if (fps < fuselage_launcher.fps_min) {
                fuselage_launcher.fps_min = fps;
            }
            if (fps > fuselage_launcher.fps_max) {
                fuselage_launcher.fps_max = fps;
            }
            fuselage_launcher.fps_sum += fps;
            fuselage_launcher.sample_count++;
        }
    }
    if (elapsed < fuselage_launcher.exit_after) {
        return;
    }
    fuselage_launcher_ensure_console();
    fprintf(stderr, "[ran] %.1fs", elapsed);
    if (fuselage_launcher.sample_count) {
        double window = elapsed - fuselage_launcher.sample_start;
        double attempts =
            window > 0 ? (double)(FuselageGetGuestStepCount() - fuselage_launcher.sample_steps) / window / 1e6
                       : 0;
        double ticks = window > 0 ? (double)(fuselage_launcher.sample_count - 1) / window : 0;
        double refresh = 0, period = 0;
        int ratio = 0;
        bool locked = fuselage_pacing_status(&refresh, &ratio, &period);
        fprintf(stderr,
                "  render fps avg %.1f min %.1f max %.1f   sim %.1f ticks/s   guest %.1f M attempts/s   "
                "pacing %s %.2f Hz",
                fuselage_launcher.fps_sum / (double)fuselage_launcher.sample_count, fuselage_launcher.fps_min,
                fuselage_launcher.fps_max, ticks, attempts, locked ? "locked to" : "free, display", refresh);
        if (locked && ratio > 1) {
            fprintf(stderr, " (%d ticks/frame)", ratio);
        } else if (locked && ratio < -1) {
            fprintf(stderr, " (%d frames/tick)", -ratio);
        }
    }
    fputc('\n', stderr);
    fflush(stderr);
    fuselage_launcher.exit_code = FUSELAGE_LAUNCH_EXIT_OK;
    fuselage_launcher.verdict_given = true;
    FuselageSignal(FUSELAGE_QUIT);
}
void FuselageTickLauncher(void) {
    if (fuselage_launcher.no_program) {
        fuselage_launcher_tick_no_program();
        return;
    }
    if (!fuselage_launcher.run_ready || fuselage_launcher.verdict_given) {
        return;
    }
    double now = DICE_GetElapsedSeconds();
    if (isfinite(now) && (!fuselage_launcher.started || now < fuselage_launcher.start_time)) {
        fuselage_launcher.start_time = now;
        fuselage_launcher.started = true;
        fuselage_launcher.sample_count = 0;
        fuselage_launcher.fps_sum = 0;
    }
    FuselageRunGuest(&fuselage_launcher.image);
    MislGuestHaltInfo halt;
    bool halted = FuselageGetGuestHaltInfo(&halt) != 0;
    if (halted && !fuselage_launcher.logged) {
        fuselage_launcher.logged = true;
        fuselage_launcher_log_halt(&halt);
    }
    FuselageHandleGuestHalt();
    if (halted && (fuselage_launcher.exit_on_halt || fuselage_launcher.exit_after > 0)) {
        const char* kind = halt.kind == MISL_GUEST_HALT_FINISHED ? "FINISHED"
                           : halt.kind == MISL_GUEST_HALT_SRAM   ? "SRAM"
                                                                 : "FAULT";
        fuselage_launcher.exit_code = halt.kind == MISL_GUEST_HALT_FINISHED ? FUSELAGE_LAUNCH_EXIT_OK
                                      : halt.kind == MISL_GUEST_HALT_SRAM   ? FUSELAGE_LAUNCH_EXIT_SRAM
                                                                            : FUSELAGE_LAUNCH_EXIT_FAULT;
        fuselage_launcher_ensure_console();
        fprintf(stderr, "[halt] %s ", kind);
        fuselage_launcher_quote(halt.reason);
        fputc('\n', stderr);
        fflush(stderr);
        fuselage_launcher.verdict_given = true;
        FuselageSignal(FUSELAGE_QUIT);
        return;
    }
    now = DICE_GetElapsedSeconds();
    if (fuselage_launcher.exit_after > 0 && fuselage_launcher.started && isfinite(now) &&
        now >= fuselage_launcher.start_time) {
        fuselage_launcher_report_duration(now - fuselage_launcher.start_time);
    }
}
