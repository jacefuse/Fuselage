// Application-supplied backend contract. SEE: (SHARP.md, Backend extensions)
#ifndef SHARP_BACKEND_EXTENSION_H
#define SHARP_BACKEND_EXTENSION_H
#include "sharp_backend.h"
#include <stdatomic.h>

typedef struct {
    const char* name;
    int (*open)(SharpBackend* backend, const SharpBackendConfig* config);
    int (*start)(SharpBackend* backend);
    int (*wait)(SharpBackend* backend, uint64_t frames);
    void (*stop)(SharpBackend* backend);
    void (*close)(SharpBackend* backend);
} SharpBackendVTable;

struct SharpBackend {
    const SharpBackendVTable* vt;
    SharpRenderFn render;
    void* user;
    uint32_t rate;
    uint32_t channels;
    _Atomic uint64_t frames;
    char error[256];
    void* impl;
};

// Register before use; the table is borrowed. NULL unregisters an optional kind.
int SHARP_RegisterBackend(SharpBackendKind kind, const SharpBackendVTable* vtable);
void SHARP_SetBackendError(SharpBackend* backend, const char* format, ...);
#endif
