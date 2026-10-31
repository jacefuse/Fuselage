/* WASAPI lifecycle. SEE: (SHARP_WINDOWS.md, WASAPI output) */
#include "sharp_wasapi_private.h"
#include <stdlib.h>
#include <string.h>

static const GUID SHARP_CLSID_MMDeviceEnumerator = {
    0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};
static const GUID SHARP_IID_IMMDeviceEnumerator = {
    0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};
static const GUID SHARP_IID_IAudioClient = {
    0x1CB9AD4C, 0xDBFA, 0x4C32, {0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2}};
static const GUID SHARP_IID_IAudioRenderClient = {
    0xF294ACFC, 0x3146, 0x4483, {0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2}};

static void sharp_wasapi_stop(SharpBackend* backend);

static void sharp_wasapi_release_resources(SharpWasapiState* implementation) {
    if (!implementation) {
        return;
    }
    if (implementation->render_client) {
        IAudioRenderClient_Release(implementation->render_client);
    }
    if (implementation->client) {
        IAudioClient_Release(implementation->client);
    }
    if (implementation->device) {
        IMMDevice_Release(implementation->device);
    }
    if (implementation->enumerator) {
        IMMDeviceEnumerator_Release(implementation->enumerator);
    }
    if (implementation->mix_format) {
        CoTaskMemFree(implementation->mix_format);
    }
    if (implementation->event) {
        CloseHandle(implementation->event);
    }
    SHARP_FreeAudioRing(&implementation->ring);
    free(implementation->device_scratch);
    if (implementation->com_initialized) {
        CoUninitialize();
    }
}

static int sharp_wasapi_open(SharpBackend* backend, const SharpBackendConfig* config) {
    HRESULT result = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (result != S_OK && result != S_FALSE && result != RPC_E_CHANGED_MODE) {
        SHARP_SetBackendError(backend, "CoInitializeEx failed (0x%08lx)", (unsigned long)result);
        return 0;
    }

    SharpWasapiState* implementation = calloc(1, sizeof *implementation);
    if (!implementation) {
        if (SUCCEEDED(result)) {
            CoUninitialize();
        }
        SHARP_SetBackendError(backend, "out of memory");
        return 0;
    }
    implementation->com_initialized = SUCCEEDED(result);

    result = CoCreateInstance(&SHARP_CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                              &SHARP_IID_IMMDeviceEnumerator, (void**)&implementation->enumerator);
    if (FAILED(result)) {
        SHARP_SetBackendError(backend, "no device enumerator (0x%08lx)", (unsigned long)result);
        goto fail;
    }

    result = IMMDeviceEnumerator_GetDefaultAudioEndpoint(implementation->enumerator, eRender, eConsole,
                                                         &implementation->device);
    if (FAILED(result)) {
        SHARP_SetBackendError(backend, "no default output device (0x%08lx)", (unsigned long)result);
        goto fail;
    }

    result = IMMDevice_Activate(implementation->device, &SHARP_IID_IAudioClient, CLSCTX_ALL, NULL,
                                (void**)&implementation->client);
    if (FAILED(result)) {
        SHARP_SetBackendError(backend, "cannot activate audio client (0x%08lx)", (unsigned long)result);
        goto fail;
    }

    result = IAudioClient_GetMixFormat(implementation->client, &implementation->mix_format);
    if (FAILED(result)) {
        SHARP_SetBackendError(backend, "GetMixFormat failed (0x%08lx)", (unsigned long)result);
        goto fail;
    }

    implementation->device_channels = implementation->mix_format->nChannels;
    backend->rate = implementation->mix_format->nSamplesPerSec;

    if (!sharp_wasapi_select_format(implementation->mix_format, &implementation->format)) {
        SHARP_SetBackendError(backend, "unsupported or malformed shared-mode format");
        goto fail;
    }

#ifndef SHARP_WASAPI_DEVICE_MS
#define SHARP_WASAPI_DEVICE_MS 0
#endif
    REFERENCE_TIME duration = (REFERENCE_TIME)SHARP_WASAPI_DEVICE_MS * 10000;
    result = IAudioClient_Initialize(implementation->client, AUDCLNT_SHAREMODE_SHARED,
                                     AUDCLNT_STREAMFLAGS_EVENTCALLBACK, duration, 0,
                                     implementation->mix_format, NULL);
    if (FAILED(result)) {
        SHARP_SetBackendError(backend, "IAudioClient::Initialize failed (0x%08lx)", (unsigned long)result);
        goto fail;
    }

    implementation->event = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!implementation->event) {
        SHARP_SetBackendError(backend, "CreateEvent failed");
        goto fail;
    }
    result = IAudioClient_SetEventHandle(implementation->client, implementation->event);
    if (FAILED(result)) {
        SHARP_SetBackendError(backend, "SetEventHandle failed (0x%08lx)", (unsigned long)result);
        goto fail;
    }

    result = IAudioClient_GetBufferSize(implementation->client, &implementation->buffer_frames);
    if (FAILED(result)) {
        SHARP_SetBackendError(backend, "GetBufferSize failed (0x%08lx)", (unsigned long)result);
        goto fail;
    }

    result = IAudioClient_GetService(implementation->client, &SHARP_IID_IAudioRenderClient,
                                     (void**)&implementation->render_client);
    if (FAILED(result)) {
        SHARP_SetBackendError(backend, "GetService(IAudioRenderClient) failed (0x%08lx)",
                              (unsigned long)result);
        goto fail;
    }

    uint32_t lead_frames = config->lead_frames ? config->lead_frames : backend->rate / 50;
    if (!implementation->buffer_frames || implementation->buffer_frames > UINT32_MAX / 2u ||
        (uint64_t)implementation->buffer_frames * 2 * sizeof(float) > SIZE_MAX) {
        SHARP_SetBackendError(backend, "invalid device buffer size");
        goto fail;
    }
    if (lead_frames < implementation->buffer_frames * 2) {
        lead_frames = implementation->buffer_frames * 2;
    }
    if (lead_frames < SHARP_WASAPI_RENDER_BLOCK) {
        lead_frames = SHARP_WASAPI_RENDER_BLOCK;
    }
    if (!SHARP_InitAudioRing(&implementation->ring, lead_frames, 2)) {
        SHARP_SetBackendError(backend, "ring alloc failed");
        goto fail;
    }

    implementation->device_scratch = calloc((size_t)implementation->buffer_frames * 2, sizeof(float));
    if (!implementation->device_scratch) {
        SHARP_SetBackendError(backend, "scratch alloc failed");
        goto fail;
    }

    backend->impl = implementation;
    return 1;

fail:
    sharp_wasapi_release_resources(implementation);
    free(implementation);
    return 0;
}

static int sharp_wasapi_start(SharpBackend* backend) {
    SharpWasapiState* implementation = backend->impl;
    if (InterlockedCompareExchange(&implementation->running, 0, 0)) {
        return 1;
    }
    if (InterlockedCompareExchange(&implementation->device_lost, 0, 0)) {
        SHARP_SetBackendError(backend, "reopen required after device failure");
        return 0;
    }

    while (SHARP_GetAudioRingWritableFrames(&implementation->ring) >= SHARP_WASAPI_RENDER_BLOCK) {
        backend->render(backend->user, implementation->render_block, SHARP_WASAPI_RENDER_BLOCK);
        SHARP_WriteAudioRing(&implementation->ring, implementation->render_block, SHARP_WASAPI_RENDER_BLOCK);
    }

    InterlockedExchange(&implementation->running, 1);
    HRESULT result = IAudioClient_Start(implementation->client);
    if (FAILED(result)) {
        InterlockedExchange(&implementation->running, 0);
        SHARP_SetBackendError(backend, "IAudioClient::Start failed (0x%08lx)", (unsigned long)result);
        return 0;
    }
    implementation->client_started = 1;
    implementation->device_thread = sharp_thread_start(sharp_wasapi_device_worker, backend);
    implementation->render_thread = sharp_thread_start(sharp_wasapi_render_worker, backend);
    if (!implementation->device_thread || !implementation->render_thread) {
        SHARP_SetBackendError(backend, "could not start threads");
        sharp_wasapi_stop(backend);
        return 0;
    }
    return 1;
}

static int sharp_wasapi_wait(SharpBackend* backend, uint64_t frames) {
    SharpWasapiState* implementation = backend->impl;
    while (atomic_load(&backend->frames) < frames) {
        if (InterlockedCompareExchange(&implementation->device_lost, 0, 0)) {
            SHARP_SetBackendError(backend, "audio device was lost (unplugged or changed)");
            return 0;
        }
        if (!InterlockedCompareExchange(&implementation->running, 0, 0)) {
            SHARP_SetBackendError(backend, "audio backend is stopped");
            return 0;
        }
        sharp_thread_sleep_milliseconds(5);
    }
    return 1;
}

static void sharp_wasapi_stop(SharpBackend* backend) {
    SharpWasapiState* implementation = backend->impl;
    if (!implementation) {
        return;
    }
    InterlockedExchange(&implementation->running, 0);
    if (implementation->event) {
        SetEvent(implementation->event);
    }
    sharp_thread_join(implementation->render_thread);
    implementation->render_thread = NULL;
    sharp_thread_join(implementation->device_thread);
    implementation->device_thread = NULL;
    if (implementation->client_started) {
        IAudioClient_Stop(implementation->client);
        implementation->client_started = 0;
    }
}

static void sharp_wasapi_close(SharpBackend* backend) {
    SharpWasapiState* implementation = backend->impl;
    if (!implementation) {
        return;
    }
    sharp_wasapi_stop(backend);
    sharp_wasapi_release_resources(implementation);
    free(implementation);
    backend->impl = NULL;
}

long sharp_wasapi_get_underrun_count(const SharpBackend* backend) {
    SharpWasapiState* implementation = backend ? backend->impl : NULL;
    return implementation ? InterlockedCompareExchange(&implementation->underruns, 0, 0) : 0;
}

const SharpBackendVTable sharp_device_vtable = {"WASAPI (shared)", sharp_wasapi_open, sharp_wasapi_start,
                                                sharp_wasapi_wait, sharp_wasapi_stop, sharp_wasapi_close};
