/* Native runtime controls. SEE: (MISL_HOST.md, System services) */
#ifndef FUSELAGE_RUNTIME_H
#define FUSELAGE_RUNTIME_H
#include <stdbool.h>

typedef enum {
    FUSELAGE_QUIT,
    FUSELAGE_WINDOWED,
    FUSELAGE_BORDERLESS,
    FUSELAGE_FULLSCREEN_EXCLUSIVE,
    FUSELAGE_APPLY_TARGET_FPS
} FuselageSignalType;

#ifdef __cplusplus
extern "C" {
#endif
/* Request a transition at the host's safe point. */
void FuselageSignal(FuselageSignalType signal);
void FuselageSetMouseCapture(bool capture);
bool FuselageGetMouseCapture(void);
bool FuselageToggleMouseCapture(void);
void FuselageSetCursorVisible(bool visible);
bool FuselageGetCursorVisible(void);
void FuselageSetVSync(bool enabled);
bool FuselageGetVSync(void);
/* Records the cap; after startup, request FUSELAGE_APPLY_TARGET_FPS to apply it. */
void FuselageSetTargetFPS(double fps);
double FuselageGetTargetFPS(void);
double FuselageGetSimRate(void);
float FuselageGetCurrentFPS(void);
#ifdef __cplusplus
}
#endif
#endif
