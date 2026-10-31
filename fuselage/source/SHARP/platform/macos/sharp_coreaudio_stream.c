/* Render-ahead producer and host callback. SEE: (SHARP_MACOS.md, Callback buffers) */
#include "sharp_coreaudio_private.h"
#include <string.h>

void sharp_coreaudio_render_worker(void* user) {
    SharpBackend* backend = user;
    SharpCoreAudioState* implementation = backend->impl;
    while (atomic_load_explicit(&implementation->running, memory_order_relaxed)) {
        if (SHARP_GetAudioRingWritableFrames(&implementation->ring) >= SHARP_COREAUDIO_RENDER_BLOCK) {
            backend->render(backend->user, implementation->render_block, SHARP_COREAUDIO_RENDER_BLOCK);
            SHARP_WriteAudioRing(&implementation->ring, implementation->render_block,
                                 SHARP_COREAUDIO_RENDER_BLOCK);
        } else {
            sharp_thread_sleep_milliseconds(1);
        }
    }
}

OSStatus sharp_coreaudio_device_callback(void* context, AudioUnitRenderActionFlags* flags,
                                         const AudioTimeStamp* timestamp, UInt32 bus, UInt32 frames,
                                         AudioBufferList* buffers) {
    SharpBackend* backend = context;
    SharpCoreAudioState* implementation = backend->impl;
    (void)timestamp;
    (void)bus;
    if (!frames) {
        return noErr;
    }

    if (!implementation || !implementation->device_scratch || !buffers) {
        return kAudio_ParamError;
    }

    const UInt32 channels = implementation->device_channels;
    uint64_t required_bytes = (uint64_t)frames * channels * sizeof(float);
    if (buffers->mNumberBuffers != 1 || !buffers->mBuffers[0].mData ||
        buffers->mBuffers[0].mNumberChannels != channels || !channels || required_bytes > SIZE_MAX ||
        required_bytes > buffers->mBuffers[0].mDataByteSize) {
        /* Silence only storage the host actually supplied. */
        for (UInt32 buffer_index = 0; buffer_index < buffers->mNumberBuffers; ++buffer_index) {
            if (buffers->mBuffers[buffer_index].mData) {
                memset(buffers->mBuffers[buffer_index].mData, 0,
                       buffers->mBuffers[buffer_index].mDataByteSize);
            }
        }
        atomic_fetch_add_explicit(&implementation->underruns, 1, memory_order_relaxed);
        return kAudio_ParamError;
    }
    float* output = buffers->mBuffers[0].mData;

    if (frames > implementation->slice_capacity) {
        memset(output, 0, (size_t)frames * channels * sizeof(float));
        atomic_fetch_add_explicit(&implementation->underruns, 1, memory_order_relaxed);
        atomic_fetch_add(&backend->frames, frames);
        if (flags) {
            *flags |= kAudioUnitRenderAction_OutputIsSilence;
        }
        return noErr;
    }

    uint32_t read_frames = SHARP_ReadAudioRing(&implementation->ring, implementation->device_scratch, frames);
    if (read_frames < frames) {
        memset(implementation->device_scratch + (size_t)read_frames * 2, 0,
               (size_t)(frames - read_frames) * 2 * sizeof(float));
        atomic_fetch_add_explicit(&implementation->underruns, 1, memory_order_relaxed);
    }

    for (UInt32 frame_index = 0; frame_index < frames; frame_index++) {
        output[(size_t)frame_index * channels + 0] =
            implementation->device_scratch[(size_t)frame_index * 2 + 0];
        if (channels > 1) {
            output[(size_t)frame_index * channels + 1] =
                implementation->device_scratch[(size_t)frame_index * 2 + 1];
        }
        for (UInt32 channel_index = 2; channel_index < channels; channel_index++) {
            output[(size_t)frame_index * channels + channel_index] = 0.0f;
        }
    }

    atomic_fetch_add(&backend->frames, frames);
    return noErr;
}
