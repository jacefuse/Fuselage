/* Default-output AudioUnit lifecycle. SEE: (SHARP_MACOS.md, CoreAudio output) */
#include "sharp_coreaudio_private.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if defined(MAC_OS_VERSION_12_0)
#define SHARP_ELEM_MAIN kAudioObjectPropertyElementMain
#else
#define SHARP_ELEM_MAIN kAudioObjectPropertyElementMaster
#endif

static UInt32 sharp_coreaudio_get_device_buffer_frames(void) {
    AudioObjectPropertyAddress address = {kAudioHardwarePropertyDefaultOutputDevice,
                                          kAudioObjectPropertyScopeGlobal, SHARP_ELEM_MAIN};
    AudioDeviceID device_id = kAudioObjectUnknown;
    UInt32 property_size = sizeof device_id;
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, NULL, &property_size, &device_id) !=
            noErr ||
        device_id == kAudioObjectUnknown) {
        return 0;
    }

    address.mSelector = kAudioDevicePropertyBufferFrameSize;
    address.mScope = kAudioDevicePropertyScopeOutput;
    UInt32 buffer_frames = 0;
    property_size = sizeof buffer_frames;
    if (AudioObjectGetPropertyData(device_id, &address, 0, NULL, &property_size, &buffer_frames) != noErr) {
        return 0;
    }
    return buffer_frames;
}

static void sharp_coreaudio_release_resources(SharpCoreAudioState* implementation) {
    if (!implementation) {
        return;
    }
    if (implementation->unit_open) {
        if (implementation->unit_started) {
            AudioOutputUnitStop(implementation->unit);
        }
        if (implementation->unit_initialized) {
            AudioUnitUninitialize(implementation->unit);
        }
        AudioComponentInstanceDispose(implementation->unit);
        implementation->unit_started = implementation->unit_initialized = implementation->unit_open = 0;
    }
    SHARP_FreeAudioRing(&implementation->ring);
    free(implementation->device_scratch);
    implementation->device_scratch = NULL;
}

static int sharp_coreaudio_open(SharpBackend* backend, const SharpBackendConfig* config) {
    SharpCoreAudioState* implementation = calloc(1, sizeof *implementation);
    if (!implementation) {
        SHARP_SetBackendError(backend, "out of memory");
        return 0;
    }

    AudioComponentDescription description = {
        .componentType = kAudioUnitType_Output,
        .componentSubType = kAudioUnitSubType_DefaultOutput,
        .componentManufacturer = kAudioUnitManufacturer_Apple,
        .componentFlags = 0,
        .componentFlagsMask = 0,
    };
    AudioComponent component = AudioComponentFindNext(NULL, &description);
    if (!component) {
        SHARP_SetBackendError(backend, "no default output audio unit");
        goto fail;
    }

    OSStatus status = AudioComponentInstanceNew(component, &implementation->unit);
    if (status != noErr) {
        SHARP_SetBackendError(backend, "AudioComponentInstanceNew failed (%d)", (int)status);
        goto fail;
    }
    implementation->unit_open = 1;

    AudioStreamBasicDescription device_format = {0};
    UInt32 property_size = sizeof device_format;
    status = AudioUnitGetProperty(implementation->unit, kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Output, 0, &device_format, &property_size);
    if (status != noErr || property_size < sizeof device_format) {
        SHARP_SetBackendError(backend, "cannot read device stream format (%d)", (int)status);
        goto fail;
    }
    if (device_format.mSampleRate <= 0) {
        device_format.mSampleRate = config->sample_rate ? (Float64)config->sample_rate : 48000.0;
    }
    if (device_format.mChannelsPerFrame < 1) {
        device_format.mChannelsPerFrame = 2;
    }

    double rounded_rate = floor(device_format.mSampleRate + 0.5);
    if (!isfinite(rounded_rate) || rounded_rate < 1 || rounded_rate > UINT32_MAX ||
        device_format.mChannelsPerFrame > UINT32_MAX / sizeof(float)) {
        SHARP_SetBackendError(backend, "invalid device rate or channel count");
        goto fail;
    }
    implementation->device_channels = device_format.mChannelsPerFrame;
    backend->rate = (uint32_t)rounded_rate;

    AudioStreamBasicDescription client_format;
    memset(&client_format, 0, sizeof client_format);
    client_format.mSampleRate = device_format.mSampleRate;
    client_format.mFormatID = kAudioFormatLinearPCM;
    client_format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    client_format.mChannelsPerFrame = implementation->device_channels;
    client_format.mBitsPerChannel = 32;
    client_format.mFramesPerPacket = 1;
    client_format.mBytesPerFrame = 4 * implementation->device_channels;
    client_format.mBytesPerPacket = client_format.mBytesPerFrame;
    status = AudioUnitSetProperty(implementation->unit, kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input, 0, &client_format, sizeof client_format);
    if (status != noErr) {
        SHARP_SetBackendError(backend, "unsupported client format: %u ch f32 @ %u Hz (%d)",
                              (unsigned)implementation->device_channels, backend->rate, (int)status);
        goto fail;
    }

    AURenderCallbackStruct callback = {sharp_coreaudio_device_callback, backend};
    status = AudioUnitSetProperty(implementation->unit, kAudioUnitProperty_SetRenderCallback,
                                  kAudioUnitScope_Input, 0, &callback, sizeof callback);
    if (status != noErr) {
        SHARP_SetBackendError(backend, "SetRenderCallback failed (%d)", (int)status);
        goto fail;
    }

    status = AudioUnitInitialize(implementation->unit);
    if (status != noErr) {
        SHARP_SetBackendError(backend, "AudioUnitInitialize failed (%d)", (int)status);
        goto fail;
    }
    implementation->unit_initialized = 1;

    UInt32 max_slice_frames = 0;
    property_size = sizeof max_slice_frames;
    if (AudioUnitGetProperty(implementation->unit, kAudioUnitProperty_MaximumFramesPerSlice,
                             kAudioUnitScope_Global, 0, &max_slice_frames, &property_size) != noErr) {
        max_slice_frames = 0;
    }

    UInt32 device_buffer_frames = sharp_coreaudio_get_device_buffer_frames();
    if (device_buffer_frames == 0) {
        device_buffer_frames = 512;
    }

    implementation->slice_capacity =
        max_slice_frames > device_buffer_frames ? max_slice_frames : device_buffer_frames;
    if (implementation->slice_capacity < 4096) {
        implementation->slice_capacity = 4096;
    }

    uint32_t lead_frames = config->lead_frames ? config->lead_frames : backend->rate / 50;
    if (device_buffer_frames > UINT32_MAX / 2u ||
        (uint64_t)implementation->slice_capacity * 2 * sizeof(float) > SIZE_MAX ||
        (uint64_t)implementation->slice_capacity * implementation->device_channels * sizeof(float) >
            UINT32_MAX) {
        SHARP_SetBackendError(backend, "invalid device slice or buffer size");
        goto fail;
    }
    if (lead_frames < SHARP_COREAUDIO_RENDER_BLOCK) {
        lead_frames = SHARP_COREAUDIO_RENDER_BLOCK;
    }
    if (lead_frames < device_buffer_frames * 2) {
        lead_frames = device_buffer_frames * 2;
    }
    if (!SHARP_InitAudioRing(&implementation->ring, lead_frames, 2)) {
        SHARP_SetBackendError(backend, "ring alloc failed");
        goto fail;
    }

    implementation->device_scratch = calloc((size_t)implementation->slice_capacity * 2, sizeof(float));
    if (!implementation->device_scratch) {
        SHARP_SetBackendError(backend, "scratch alloc failed");
        goto fail;
    }

    backend->impl = implementation;
    return 1;

fail:
    sharp_coreaudio_release_resources(implementation);
    free(implementation);
    return 0;
}

static int sharp_coreaudio_start(SharpBackend* backend) {
    SharpCoreAudioState* implementation = backend->impl;
    if (atomic_load(&implementation->running)) {
        return 1;
    }
    if (atomic_load(&implementation->device_lost)) {
        SHARP_SetBackendError(backend, "reopen required after device failure");
        return 0;
    }

    while (SHARP_GetAudioRingWritableFrames(&implementation->ring) >= SHARP_COREAUDIO_RENDER_BLOCK) {
        backend->render(backend->user, implementation->render_block, SHARP_COREAUDIO_RENDER_BLOCK);
        SHARP_WriteAudioRing(&implementation->ring, implementation->render_block,
                             SHARP_COREAUDIO_RENDER_BLOCK);
    }

    atomic_store(&implementation->running, 1);
    implementation->render_thread = sharp_thread_start(sharp_coreaudio_render_worker, backend);
    if (!implementation->render_thread) {
        atomic_store(&implementation->running, 0);
        SHARP_SetBackendError(backend, "could not start render thread");
        return 0;
    }

    OSStatus status = AudioOutputUnitStart(implementation->unit);
    if (status != noErr) {
        atomic_store(&implementation->running, 0);
        sharp_thread_join(implementation->render_thread);
        implementation->render_thread = NULL;
        SHARP_SetBackendError(backend, "AudioOutputUnitStart failed (%d)", (int)status);
        return 0;
    }
    implementation->unit_started = 1;
    return 1;
}

static int sharp_coreaudio_wait(SharpBackend* backend, uint64_t frames) {
    SharpCoreAudioState* implementation = backend->impl;
    uint64_t last_position = atomic_load(&backend->frames);
    uint32_t idle_milliseconds = 0;

    if (atomic_load(&implementation->device_lost)) {
        SHARP_SetBackendError(backend, "audio device stopped delivering (unplugged or changed)");
        return 0;
    }

    while (atomic_load(&backend->frames) < frames) {
        if (!atomic_load(&implementation->running)) {
            SHARP_SetBackendError(backend, "audio backend is stopped");
            return 0;
        }
        sharp_thread_sleep_milliseconds(5);
        uint64_t current_position = atomic_load(&backend->frames);
        if (current_position != last_position) {
            last_position = current_position;
            idle_milliseconds = 0;
            continue;
        }
        idle_milliseconds += 5;
        if (idle_milliseconds >= SHARP_COREAUDIO_STALL_MILLISECONDS) {
            atomic_store(&implementation->device_lost, 1);
            atomic_store(&implementation->running, 0);
            SHARP_SetBackendError(backend, "audio device stopped delivering (unplugged or changed)");
            return 0;
        }
    }
    return 1;
}

static void sharp_coreaudio_stop(SharpBackend* backend) {
    SharpCoreAudioState* implementation = backend->impl;
    if (!implementation) {
        return;
    }

    if (implementation->unit_started) {
        AudioOutputUnitStop(implementation->unit);
        implementation->unit_started = 0;
    }
    atomic_store(&implementation->running, 0);
    if (implementation->render_thread) {
        sharp_thread_join(implementation->render_thread);
        implementation->render_thread = NULL;
    }
}

static void sharp_coreaudio_close(SharpBackend* backend) {
    SharpCoreAudioState* implementation = backend->impl;
    if (!implementation) {
        return;
    }
    sharp_coreaudio_stop(backend);
    sharp_coreaudio_release_resources(implementation);
    free(implementation);
    backend->impl = NULL;
}

long sharp_coreaudio_get_underrun_count(const SharpBackend* backend) {
    const SharpCoreAudioState* implementation = backend ? (const SharpCoreAudioState*)backend->impl : NULL;
    return implementation ? atomic_load(&implementation->underruns) : 0;
}

const SharpBackendVTable sharp_device_vtable = {"CoreAudio (default output)", sharp_coreaudio_open,
                                                sharp_coreaudio_start,        sharp_coreaudio_wait,
                                                sharp_coreaudio_stop,         sharp_coreaudio_close};
