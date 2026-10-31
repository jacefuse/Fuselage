/* Render-ahead producer and event-driven device consumer.
 * SEE: (SHARP_WINDOWS.md, Stream flow and counters) */
#include "sharp_wasapi_private.h"
#include <stdlib.h>
#include <string.h>

void sharp_wasapi_render_worker(void* user) {
    SharpBackend* backend = user;
    SharpWasapiState* implementation = backend->impl;
    while (InterlockedCompareExchange(&implementation->running, 1, 1)) {
        if (SHARP_GetAudioRingWritableFrames(&implementation->ring) >= SHARP_WASAPI_RENDER_BLOCK) {
            backend->render(backend->user, implementation->render_block, SHARP_WASAPI_RENDER_BLOCK);
            SHARP_WriteAudioRing(&implementation->ring, implementation->render_block,
                                 SHARP_WASAPI_RENDER_BLOCK);
        } else {
            sharp_thread_sleep_milliseconds(1);
        }
    }
}

void sharp_wasapi_device_worker(void* user) {
    SharpBackend* backend = user;
    SharpWasapiState* implementation = backend->impl;
    HRESULT com_result = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(com_result)) {
        SHARP_SetBackendError(backend, "device-thread COM initialization failed (0x%08lx)",
                              (unsigned long)com_result);
        InterlockedExchange(&implementation->device_lost, 1);
        InterlockedExchange(&implementation->running, 0);
        return;
    }

    while (InterlockedCompareExchange(&implementation->running, 1, 1)) {
        DWORD wait_result = WaitForSingleObject(implementation->event, 200);
        if (!InterlockedCompareExchange(&implementation->running, 0, 0)) {
            break;
        }
        if (wait_result == WAIT_FAILED) {
            SHARP_SetBackendError(backend, "device event wait failed");
            InterlockedExchange(&implementation->device_lost, 1);
            break;
        }
        if (wait_result != WAIT_OBJECT_0) {
            continue;
        }

        UINT32 padding_frames = 0;
        HRESULT result = IAudioClient_GetCurrentPadding(implementation->client, &padding_frames);
        if (result == AUDCLNT_E_DEVICE_INVALIDATED) {
            InterlockedExchange(&implementation->device_lost, 1);
            break;
        }
        if (FAILED(result)) {
            continue;
        }

        if (padding_frames > implementation->buffer_frames) {
            SHARP_SetBackendError(backend, "invalid device padding");
            InterlockedExchange(&implementation->device_lost, 1);
            break;
        }
        UINT32 requested_frames = implementation->buffer_frames - padding_frames;
        if (requested_frames == 0) {
            continue;
        }

        BYTE* data = NULL;
        result = IAudioRenderClient_GetBuffer(implementation->render_client, requested_frames, &data);
        if (result == AUDCLNT_E_DEVICE_INVALIDATED) {
            InterlockedExchange(&implementation->device_lost, 1);
            break;
        }
        if (FAILED(result) || !data) {
            continue;
        }

        uint32_t read_frames =
            SHARP_ReadAudioRing(&implementation->ring, implementation->device_scratch, requested_frames);
        if (read_frames < requested_frames) {
            memset(implementation->device_scratch + (size_t)read_frames * 2, 0,
                   (size_t)(requested_frames - read_frames) * 2 * sizeof(float));
            InterlockedIncrement(&implementation->underruns);
        }

        sharp_wasapi_copy_frames(implementation, data, requested_frames);

        result = IAudioRenderClient_ReleaseBuffer(implementation->render_client, requested_frames, 0);
        if (FAILED(result)) {
            SHARP_SetBackendError(backend, "ReleaseBuffer failed (0x%08lx)", (unsigned long)result);
            InterlockedExchange(&implementation->device_lost, 1);
            break;
        }
        atomic_fetch_add(&backend->frames, requested_frames);
    }
    InterlockedExchange(&implementation->running, 0);
    CoUninitialize();
}
