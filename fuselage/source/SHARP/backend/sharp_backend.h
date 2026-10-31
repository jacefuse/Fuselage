// Stream lifecycle and output. SEE: (SHARP.md, Backend interface)
#ifndef SHARP_BACKEND_H
#define SHARP_BACKEND_H
#include <stdint.h>

typedef void (*SharpRenderFn)(void* user, float* stereo, uint32_t frames);

typedef enum {
    SHARP_BACKEND_WAV = 0, // Requires an application-supplied extension.
    SHARP_BACKEND_DEVICE,
    SHARP_BACKEND_KIND_COUNT
} SharpBackendKind;

typedef struct {
    uint32_t sample_rate;
    uint32_t lead_frames;
    const char* path;
} SharpBackendConfig;

typedef struct SharpBackend SharpBackend;

const char* SHARP_GetBackendName(SharpBackendKind kind);
int SHARP_IsBackendAvailable(SharpBackendKind kind); // Present, not guaranteed to open.
SharpBackend* SHARP_OpenBackend(SharpBackendKind kind, const SharpBackendConfig* config,
                               SharpRenderFn render, void* user);
uint32_t SHARP_GetBackendSampleRate(const SharpBackend* backend);
int SHARP_StartBackend(SharpBackend* backend);
int SHARP_WaitBackend(SharpBackend* backend, uint64_t frames); // Absolute backend-frame target.
void SHARP_StopBackend(SharpBackend* backend);
void SHARP_CloseBackend(SharpBackend* backend);
uint64_t SHARP_GetBackendFrameCount(const SharpBackend* backend);
const char* SHARP_GetBackendError(const SharpBackend* backend);
const char* SHARP_GetLastBackendError(void);
#endif
