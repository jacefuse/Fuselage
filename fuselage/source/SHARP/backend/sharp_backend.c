/* Backend selection and shared handle lifecycle. SEE: (SHARP.md, Backend interface) */
#include "sharp_backend_private.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static char sharp_backend_last_error_text[256];
static const SharpBackendVTable* sharp_backend_registered[SHARP_BACKEND_KIND_COUNT];

void SHARP_SetBackendError(SharpBackend* backend, const char* format, ...) {
    char message[256] = {0};
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(message, sizeof message, format ? format : "", arguments);
    va_end(arguments);
    if (backend) {
        snprintf(backend->error, sizeof backend->error, "%s", message);
    }
    snprintf(sharp_backend_last_error_text, sizeof sharp_backend_last_error_text, "%s", message);
}

int SHARP_RegisterBackend(SharpBackendKind kind, const SharpBackendVTable* vtable) {
    if (kind < 0 || kind >= SHARP_BACKEND_KIND_COUNT || kind == SHARP_BACKEND_DEVICE) {
        return 0;
    }
    sharp_backend_registered[kind] = vtable;
    return 1;
}

static const SharpBackendVTable* sharp_backend_get_vtable(SharpBackendKind kind) {
    switch (kind) {
    case SHARP_BACKEND_WAV:
        return sharp_backend_registered[SHARP_BACKEND_WAV];
    case SHARP_BACKEND_DEVICE:
        return &sharp_device_vtable;
    default:
        return NULL;
    }
}

const char* SHARP_GetBackendName(SharpBackendKind kind) {
    const SharpBackendVTable* vtable = sharp_backend_get_vtable(kind);
    return vtable && vtable->name ? vtable->name : "(unknown)";
}

int SHARP_IsBackendAvailable(SharpBackendKind kind) {
    const SharpBackendVTable* vtable = sharp_backend_get_vtable(kind);
    return vtable && vtable->open ? 1 : 0;
}

SharpBackend* SHARP_OpenBackend(SharpBackendKind kind, const SharpBackendConfig* config, SharpRenderFn render,
                                void* user) {
    const SharpBackendVTable* vtable = sharp_backend_get_vtable(kind);
    if (!vtable) {
        SHARP_SetBackendError(NULL, "no such backend");
        return NULL;
    }
    if (!vtable->open) {
        SHARP_SetBackendError(NULL, "backend has no open callback");
        return NULL;
    }
    if (!config || !render) {
        SHARP_SetBackendError(NULL, "backend requires configuration and render callback");
        return NULL;
    }

    SharpBackend* backend = calloc(1, sizeof *backend);
    if (!backend) {
        SHARP_SetBackendError(NULL, "out of memory");
        return NULL;
    }
    backend->vt = vtable;
    backend->render = render;
    backend->user = user;
    backend->rate = config->sample_rate;
    backend->channels = 2;
    atomic_store(&backend->frames, 0);

    /* Failed open must release its own partial resources; close is not called. */
    if (!vtable->open(backend, config)) {
        snprintf(sharp_backend_last_error_text, sizeof sharp_backend_last_error_text, "%s", backend->error);
        free(backend);
        return NULL;
    }
    return backend;
}

uint32_t SHARP_GetBackendSampleRate(const SharpBackend* backend) {
    return backend ? backend->rate : 0;
}

const char* SHARP_GetBackendError(const SharpBackend* backend) {
    return backend ? backend->error : sharp_backend_last_error_text;
}

const char* SHARP_GetLastBackendError(void) {
    return sharp_backend_last_error_text;
}

uint64_t SHARP_GetBackendFrameCount(const SharpBackend* backend) {
    return backend ? atomic_load(&backend->frames) : 0;
}

int SHARP_StartBackend(SharpBackend* backend) {
    return backend && backend->vt->start ? backend->vt->start(backend) : 0;
}

int SHARP_WaitBackend(SharpBackend* backend, uint64_t frames) {
    return backend && backend->vt->wait ? backend->vt->wait(backend, frames) : 0;
}

void SHARP_StopBackend(SharpBackend* backend) {
    if (backend && backend->vt->stop) {
        backend->vt->stop(backend);
    }
}

void SHARP_CloseBackend(SharpBackend* backend) {
    if (!backend) {
        return;
    }
    /* The backend close callback owns stopping/joining any remaining users. */
    if (backend->vt->close) {
        backend->vt->close(backend);
    }
    free(backend);
}
