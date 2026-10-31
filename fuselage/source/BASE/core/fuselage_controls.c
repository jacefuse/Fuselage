/* Native controls and pre-init settings. SEE: (FUSELAGE.md, Configuration) */
#include "fuselage_internal.h"
#include <math.h>
static bool fuselage_before_start(void) {
    return atomic_load(&fuselage_root.phase) == FUSELAGE_COLD;
}
void FuselageSetTitle(const char* title) {
    if (fuselage_before_start()) {
        GDMF_SetTitle(title);
    }
}
void FuselageSetResolution(int width, int height) {
    if (fuselage_before_start()) {
        GDMF_SetResolution(width, height);
    }
}
void FuselageSetCanvasResolution(int width, int height) {
    if (fuselage_before_start()) {
        GDMF_SetCanvasResolution(width, height);
    }
}
void FuselageSetAspectRatio(int numerator, int denominator) {
    if (fuselage_before_start()) {
        GDMF_SetAspectRatio(numerator, denominator);
    }
}
void FuselageSetWindowIcon(int width, int height, const unsigned char* rgba) {
    if (fuselage_before_start()) {
        GDMF_SetWindowIcon(width, height, rgba);
    }
}
void FuselageSetSimRate(double rate) {
    if (fuselage_before_start() && isfinite(rate) && rate > 0) {
        fuselage_root.simulation_rate = rate;
    }
}
double FuselageGetSimRate(void) {
    return fuselage_root.simulation_rate;
}
void FuselageSetBanner(bool enabled) {
    if (fuselage_before_start()) {
        fuselage_root.banner = enabled;
    }
}
void FuselageSetAudioPoolMB(int pool_mib) {
    if (fuselage_before_start()) {
        SHARP_SetPoolMB(pool_mib);
    }
}
void FuselageGetCanvasResolution(int* width, int* height) {
    if (width) {
        *width = GDMF_GetCanvasWidth();
    }
    if (height) {
        *height = GDMF_GetCanvasHeight();
    }
}
void FuselageGetResolution(int* width, int* height) {
    if (width) {
        *width = GDMF_GetWidth();
    }
    if (height) {
        *height = GDMF_GetHeight();
    }
}
void FuselageGetAspectRatio(int* numerator, int* denominator) {
    if (numerator) {
        *numerator = GDMF_GetAspectRatioNum();
    }
    if (denominator) {
        *denominator = GDMF_GetAspectRatioDen();
    }
}
void FuselageSetMouseCapture(bool capture) {
    GDMF_SetMouseCapture(capture);
}
bool FuselageGetMouseCapture(void) {
    return GDMF_GetMouseCapture();
}
bool FuselageToggleMouseCapture(void) {
    return GDMF_ToggleMouseCapture();
}
void FuselageSetCursorVisible(bool visible) {
    GDMF_SetCursorVisible(visible);
}
bool FuselageGetCursorVisible(void) {
    return GDMF_GetCursorVisible();
}
void FuselageSetVSync(bool enabled) {
    GDMF_SetVSync(enabled);
}
bool FuselageGetVSync(void) {
    return GDMF_GetVSync();
}
float FuselageGetCurrentFPS(void) {
    return GDMF_GetCurrentFPS();
}
void FuselageSetInputRequiresFocus(bool enabled) {
    atomic_store(&fuselage_root.input_requires_focus, enabled);
    atomic_store(&fuselage_root.focus_policy_changed, true);
}
bool FuselageGetInputRequiresFocus(void) {
    return atomic_load(&fuselage_root.input_requires_focus);
}
void FuselageSetTickCallback(FuselageTickCallback callback) {
    atomic_store(&fuselage_root.tick_callback, callback);
}
void FuselageSetTargetFPS(double fps) {
    if (isfinite(fps) && fps >= 0) {
        atomic_store(&fuselage_root.target_fps, fps);
    }
}
double FuselageGetTargetFPS(void) {
    return atomic_load(&fuselage_root.target_fps);
}
void FuselageWaitFrame(void) {
    if (atomic_load(&fuselage_root.phase) == FUSELAGE_RUNNING && fuselage_root.frame_event_ready) {
        fuselage_event_wait(&fuselage_root.frame_event);
    }
}
