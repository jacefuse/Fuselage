#pragma once
#include "misl_guest_call_internal.h"

// System calls for guest scheduling and presentation.
// SEE: (MISL_GUEST.md, System and presentation)

static inline void FuselageSignal(int signal) {
    MISL_GUEST_CALL(MISL_SYS_SIGNAL, signal, 0, 0, 0, 0, 0, 0);
}

static inline Color GDMF_GetClearColor(void) {
    return misl_guest_unpack_color(MISL_GUEST_CALL(MISL_SYS_GET_CLEARCOLOR, 0, 0, 0, 0, 0, 0, 0));
}

static inline void GDMF_SetClearColor(Color color) {
    MISL_GUEST_CALL(MISL_SYS_SET_CLEARCOLOR, misl_guest_pack_color(color), 0, 0, 0, 0, 0, 0);
}

static inline void FuselageWaitVBL(void) {
    MISL_GUEST_CALL(MISL_SYS_WAITVBL, 0, 0, 0, 0, 0, 0, 0);
}

static inline void MISL_Yield(void) {
    MISL_GUEST_CALL(MISL_SYS_YIELD, 0, 0, 0, 0, 0, 0, 0);
}

static inline void MISL_Sleep(int ticks) {
    MISL_GUEST_CALL(MISL_SYS_SLEEP, ticks, 0, 0, 0, 0, 0, 0);
}

static inline float FuselageGetCurrentFPS(void) {
    return misl_guest_bits_float(MISL_GUEST_CALL(MISL_SYS_GET_FPS, 0, 0, 0, 0, 0, 0, 0));
}

static inline uint64_t FuselageElapsedMicroseconds(void) {
    return MISL_GUEST_CALL64(MISL_SYS_ELAPSED_US, 0, 0, 0, 0, 0, 0, 0);
}

static inline bool FuselageCaptureFrame(const char* name) {
    return MISL_GUEST_CALL(MISL_SYS_CAPTURE, misl_guest_address(name), 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool FuselageCapturePending(void) {
    return MISL_GUEST_CALL(MISL_SYS_CAPTURE_PENDING, 0, 0, 0, 0, 0, 0, 0) != 0;
}

static inline void FuselageSetMouseCapture(bool captured) {
    MISL_GUEST_CALL(MISL_SYS_SET_MOUSECAPTURE, captured, 0, 0, 0, 0, 0, 0);
}

static inline bool FuselageGetMouseCapture(void) {
    return MISL_GUEST_CALL(MISL_SYS_GET_MOUSECAPTURE, 0, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool FuselageToggleMouseCapture(void) {
    return MISL_GUEST_CALL(MISL_SYS_TOGGLE_MOUSECAPTURE, 0, 0, 0, 0, 0, 0, 0) != 0;
}

static inline void FuselageSetCursorVisible(bool visible) {
    MISL_GUEST_CALL(MISL_SYS_SET_CURSORVISIBLE, visible, 0, 0, 0, 0, 0, 0);
}

static inline bool FuselageGetCursorVisible(void) {
    return MISL_GUEST_CALL(MISL_SYS_GET_CURSORVISIBLE, 0, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool FuselageIsFocused(void) {
    return MISL_GUEST_CALL(MISL_SYS_IS_FOCUSED, 0, 0, 0, 0, 0, 0, 0) != 0;
}

static inline void FuselageSetVSync(bool enabled) {
    MISL_GUEST_CALL(MISL_SYS_SET_VSYNC, enabled, 0, 0, 0, 0, 0, 0);
}

static inline bool FuselageGetVSync(void) {
    return MISL_GUEST_CALL(MISL_SYS_GET_VSYNC, 0, 0, 0, 0, 0, 0, 0) != 0;
}

static inline void FuselageSetTargetFPS(double frames_per_second) {
    MISL_GUEST_CALL(MISL_SYS_SET_TARGETFPS, misl_guest_double_low(frames_per_second),
                    misl_guest_double_high(frames_per_second), 0, 0, 0, 0, 0);
}

static inline double FuselageGetTargetFPS(void) {
    return misl_guest_bits_double(MISL_GUEST_CALL64(MISL_SYS_GET_TARGETFPS, 0, 0, 0, 0, 0, 0, 0));
}

static inline double FuselageGetSimRate(void) {
    return misl_guest_bits_double(MISL_GUEST_CALL64(MISL_SYS_GET_SIMRATE, 0, 0, 0, 0, 0, 0, 0));
}

static inline int FuselageGetCanvasWidth(void) {
    return (int)MISL_GUEST_CALL(MISL_SYS_GET_CANVAS_W, 0, 0, 0, 0, 0, 0, 0);
}

static inline int FuselageGetCanvasHeight(void) {
    return (int)MISL_GUEST_CALL(MISL_SYS_GET_CANVAS_H, 0, 0, 0, 0, 0, 0, 0);
}
