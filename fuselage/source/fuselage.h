/* Native orchestrator. SEE: (FUSELAGE.md, Lifecycle) */
#ifndef FUSELAGE_H
#define FUSELAGE_H
#include "BASE/fuselage_configuration.h"
#include "BASE/fuselage_runtime.h"
#include "GDMF/gdmf.h"
#include "GDMF/sprites/gdmf_sprites_help.h"
#include "GDMF/tiles/gdmf_tiles_help.h"
#include "CAKE/cake.h"
#include "CAKE/cake_help.h"
#include "DICE/dice.h"
#include "SHARP/sharp.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { FUSELAGE_EXIT_OK = 0, FUSELAGE_EXIT_FAILED = 1, FUSELAGE_EXIT_SAVE_FAILED = 5 };
/* Initializes once; false means shutdown completed. Use the exit code afterwards. */
bool fuselage(void);
int FuselageGetExitCode(void);
void FuselageGetCanvasResolution(int* width, int* height);
void FuselageGetResolution(int* width, int* height);
void FuselageGetAspectRatio(int* numerator, int* denominator);
void FuselageSetInputRequiresFocus(bool enabled);
bool FuselageGetInputRequiresFocus(void);
void FuselageWaitFrame(void);
/* Project callback; the supplied main registers it for a native application. */
void tick(void);
#ifdef __cplusplus
}
#endif
#endif
