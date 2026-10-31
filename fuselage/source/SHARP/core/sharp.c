/* SHARP lifecycle and backend ownership. SEE: (SHARP.md, Lifecycle) */
#include "sharp_facade_private.h"
#include <stdio.h>
#include <stdlib.h>

SharpFacadeState sharp_facade_state;

bool SHARP_Init(void) {
    if (sharp_facade_state.initialized) {
        return sharp_facade_state.available;
    }
    sharp_facade_state.initialized = true;
    sharp_facade_state.chip_count = 0;

    int pool_mib =
        (sharp_facade_state.pool_mib > 0) ? sharp_facade_state.pool_mib : SHARP_FACADE_POOL_MIB_DEFAULT;
    if (pool_mib < SHARP_FACADE_POOL_MIB_MIN) {
        pool_mib = SHARP_FACADE_POOL_MIB_MIN;
    }
    if ((uint32_t)pool_mib > UINT32_MAX / (1024u * 1024u)) {
        printf("[SHARP] pool size exceeds the 32-bit byte range; audio disabled\n");
        return false;
    }
    sharp_facade_state.pool_size = (uint32_t)pool_mib * 1024u * 1024u;
    sharp_facade_state.pool = calloc(1, sharp_facade_state.pool_size);
    if (!sharp_facade_state.pool) {
        printf("[SHARP] pool allocation of %d MiB failed; audio disabled\n", pool_mib);
        sharp_facade_state.pool_size = 0;
        return false;
    }

    if (!SHARP_InitSequence(&sharp_facade_state.sequence, 8192)) {
        printf("[SHARP] event ring allocation failed; audio disabled\n");
        free(sharp_facade_state.pool);
        sharp_facade_state.pool = NULL;
        sharp_facade_state.pool_size = 0;
        return false;
    }

    InitLori(&sharp_facade_state.mixer, 0.9f);

    SharpBackendConfig config = {48000, 0, NULL};
    sharp_facade_state.backend = SHARP_OpenBackend(SHARP_BACKEND_DEVICE, &config, sharp_facade_render, NULL);
    if (!sharp_facade_state.backend) {
        printf("[SHARP] no audio device (%s); running silent\n", SHARP_GetLastBackendError());
        return false;
    }

    if (!SHARP_StartBackend(sharp_facade_state.backend)) {
        printf("[SHARP] backend would not start: %s; running silent\n",
               SHARP_GetBackendError(sharp_facade_state.backend));
        SHARP_CloseBackend(sharp_facade_state.backend);
        sharp_facade_state.backend = NULL;
        return false;
    }

    sharp_facade_state.available = true;
    printf("[SHARP] %s at %u Hz, %d MiB pool\n", SHARP_GetBackendName(SHARP_BACKEND_DEVICE),
           SHARP_GetBackendSampleRate(sharp_facade_state.backend), pool_mib);
    return true;
}

void SHARP_Shutdown(void) {
    if (!sharp_facade_state.initialized) {
        return;
    }

    /* Stop callbacks before releasing the objects they use. */
    if (sharp_facade_state.backend) {
        SHARP_StopBackend(sharp_facade_state.backend);
        SHARP_CloseBackend(sharp_facade_state.backend);
        sharp_facade_state.backend = NULL;
    }
    SHARP_FreeSequence(&sharp_facade_state.sequence);
    free(sharp_facade_state.pool);
    sharp_facade_state.pool = NULL;
    sharp_facade_state.pool_size = 0;
    sharp_facade_state.chip_count = 0;
    sharp_facade_state.available = false;
    sharp_facade_state.initialized = false;
}

bool SHARP_IsAvailable(void) {
    return sharp_facade_state.available;
}

uint32_t SHARP_GetSampleRate(void) {
    return sharp_facade_state.backend ? SHARP_GetBackendSampleRate(sharp_facade_state.backend) : 0;
}

uint64_t SHARP_GetRenderPosition(void) {
    return sharp_facade_state.available ? SHARP_GetSequencePosition(&sharp_facade_state.sequence) : 0;
}
