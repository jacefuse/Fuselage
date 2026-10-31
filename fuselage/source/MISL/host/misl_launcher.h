/* Native disk launcher. SEE: (MISL_LAUNCHER.md, Lifecycle) */
#ifndef MISL_LAUNCHER_H
#define MISL_LAUNCHER_H
enum { FUSELAGE_LAUNCH_FAILED = 0, FUSELAGE_LAUNCH_RUN = 1, FUSELAGE_LAUNCH_DONE = 2 };
enum {
    FUSELAGE_LAUNCH_EXIT_OK = 0,
    FUSELAGE_LAUNCH_EXIT_FAILED = 1,
    FUSELAGE_LAUNCH_EXIT_FAULT = 3,
    FUSELAGE_LAUNCH_EXIT_SRAM = 4
};
#ifdef __cplusplus
extern "C" {
#endif
/* Call before engine startup or after tick callbacks have stopped. */
int FuselageInitLauncher(int argc, char** argv);
void FuselageApplyLauncherDisplayMode(void);
void FuselageTickLauncher(void);
/* Detaches guest mappings before releasing owned file bytes. Does not save. */
void FuselageShutdownLauncher(void);
int FuselageGetLauncherExitCode(void);
#ifdef __cplusplus
}
#endif
#endif
