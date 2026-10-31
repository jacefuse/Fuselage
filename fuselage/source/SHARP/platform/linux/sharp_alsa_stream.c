/* Render producer and blocking PCM consumer. SEE: (SHARP_LINUX.md, Stream flow and counters) */
#include "sharp_alsa_private.h"
#include <string.h>
#include <errno.h>

void sharp_alsa_render_worker(void* user) {
    SharpBackend* backend = user;
    SharpAlsaState* implementation = backend->impl;
    while (atomic_load_explicit(&implementation->running, memory_order_relaxed)) {
        if (SHARP_GetAudioRingWritableFrames(&implementation->ring) >= SHARP_ALSA_RENDER_BLOCK) {
            backend->render(backend->user, implementation->render_block, SHARP_ALSA_RENDER_BLOCK);
            SHARP_WriteAudioRing(&implementation->ring, implementation->render_block,
                                 SHARP_ALSA_RENDER_BLOCK);
        } else {
            sharp_thread_sleep_milliseconds(1);
        }
    }
}

void sharp_alsa_device_worker(void* user) {
    SharpBackend* backend = user;
    SharpAlsaState* implementation = backend->impl;
    const uint32_t frames = (uint32_t)implementation->period_frames;
    size_t sample_bytes = implementation->format == SHARP_ALSA_F32 ? sizeof(float) : sizeof(int16_t);
    size_t frame_bytes = (size_t)implementation->device_channels * sample_bytes;
    while (atomic_load_explicit(&implementation->running, memory_order_relaxed)) {
        uint32_t read_frames =
            SHARP_ReadAudioRing(&implementation->ring, implementation->device_scratch, frames);
        if (read_frames < frames) {
            memset(implementation->device_scratch + (size_t)read_frames * 2, 0,
                   (size_t)(frames - read_frames) * 2 * sizeof(float));
            atomic_fetch_add_explicit(&implementation->underruns, 1, memory_order_relaxed);
        }
        sharp_alsa_copy_frames(implementation, implementation->device_buffer, frames);
        uint32_t written_frames = 0;
        while (written_frames < frames &&
               atomic_load_explicit(&implementation->running, memory_order_relaxed)) {
            uint32_t remaining_frames = frames - written_frames;
            const uint8_t* pending =
                (const uint8_t*)implementation->device_buffer + (size_t)written_frames * frame_bytes;
            snd_pcm_sframes_t result = snd_pcm_writei(implementation->pcm, pending, remaining_frames);
            if (result > 0) {
                if ((uint64_t)result > remaining_frames) {
                    atomic_store(&implementation->device_lost, 1);
                    break;
                }
                written_frames += (uint32_t)result;
                atomic_fetch_add(&backend->frames, (uint64_t)result);
                continue;
            }
            if (!atomic_load_explicit(&implementation->running, memory_order_relaxed)) {
                break;
            }
            if (result == 0 || result == -EAGAIN) {
                sharp_thread_sleep_milliseconds(1);
                continue;
            }
            if (result == -ENODEV || result == -ENOTTY || result == -EBADFD) {
                atomic_store(&implementation->device_lost, 1);
                break;
            }
            if (result == -EPIPE || result == -ESTRPIPE) {
                atomic_fetch_add_explicit(&implementation->underruns, 1, memory_order_relaxed);
            }
            if (snd_pcm_recover(implementation->pcm, (int)result, 1) < 0) {
                atomic_store(&implementation->device_lost, 1);
                break;
            }
            /* Retry the remaining converted period after recovery. */
        }
        if (atomic_load(&implementation->device_lost)) {
            break;
        }
    }
    atomic_store(&implementation->running, 0);
}
