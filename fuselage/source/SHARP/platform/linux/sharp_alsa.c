/* ALSA negotiation and lifecycle. SEE: (SHARP_LINUX.md, ALSA output) */
#include "sharp_alsa_private.h"
#include <stdlib.h>
#include <string.h>
#ifndef SHARP_ALSA_DEVICE_MS
#define SHARP_ALSA_DEVICE_MS 20
#endif
#if SHARP_ALSA_DEVICE_MS < 0
#error SHARP_ALSA_DEVICE_MS must be nonnegative
#endif
#define SHARP_ALSA_PERIODS 4
static void sharp_alsa_stop(SharpBackend* backend);

static void sharp_alsa_release_resources(SharpAlsaState* implementation) {
    if (!implementation) {
        return;
    }
    if (implementation->pcm) {
        snd_pcm_close(implementation->pcm);
    }
    SHARP_FreeAudioRing(&implementation->ring);
    free(implementation->device_scratch);
    free(implementation->device_buffer);
}

static int sharp_alsa_open(SharpBackend* backend, const SharpBackendConfig* config) {
    SharpAlsaState* implementation = calloc(1, sizeof *implementation);
    if (!implementation) {
        SHARP_SetBackendError(backend, "out of memory");
        return 0;
    }

    int result = snd_pcm_open(&implementation->pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (result < 0) {
        SHARP_SetBackendError(backend, "no audio device: %s", snd_strerror(result));
        goto fail;
    }

    snd_pcm_hw_params_t* hardware;
    snd_pcm_hw_params_alloca(&hardware);
    if ((result = snd_pcm_hw_params_any(implementation->pcm, hardware)) < 0) {
        SHARP_SetBackendError(backend, "no playback configurations: %s", snd_strerror(result));
        goto fail;
    }
    if ((result = snd_pcm_hw_params_set_access(implementation->pcm, hardware,
                                               SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        SHARP_SetBackendError(backend, "interleaved access unavailable: %s", snd_strerror(result));
        goto fail;
    }

    if (snd_pcm_hw_params_set_format(implementation->pcm, hardware, SND_PCM_FORMAT_FLOAT) >= 0) {
        implementation->format = SHARP_ALSA_F32;
    } else if ((result = snd_pcm_hw_params_set_format(implementation->pcm, hardware, SND_PCM_FORMAT_S16)) >=
               0) {
        implementation->format = SHARP_ALSA_S16;
    } else {
        SHARP_SetBackendError(backend, "device supports neither float nor s16: %s", snd_strerror(result));
        goto fail;
    }

    unsigned channels = 2;
    if ((result = snd_pcm_hw_params_set_channels_near(implementation->pcm, hardware, &channels)) < 0) {
        SHARP_SetBackendError(backend, "cannot set channels: %s", snd_strerror(result));
        goto fail;
    }
    if (!channels) {
        SHARP_SetBackendError(backend, "invalid negotiated channel count");
        goto fail;
    }
    implementation->device_channels = channels;

    unsigned rate = config->sample_rate ? config->sample_rate : 48000;
    if ((result = snd_pcm_hw_params_set_rate_near(implementation->pcm, hardware, &rate, 0)) < 0) {
        SHARP_SetBackendError(backend, "cannot set sample rate: %s", snd_strerror(result));
        goto fail;
    }
    if (!rate) {
        SHARP_SetBackendError(backend, "invalid negotiated sample rate");
        goto fail;
    }
    backend->rate = rate;

    snd_pcm_uframes_t buffer_frames = (snd_pcm_uframes_t)((uint64_t)rate * SHARP_ALSA_DEVICE_MS / 1000);
    if (buffer_frames < 256) {
        buffer_frames = 256;
    }
    if ((result = snd_pcm_hw_params_set_buffer_size_near(implementation->pcm, hardware, &buffer_frames)) <
        0) {
        SHARP_SetBackendError(backend, "cannot set buffer size: %s", snd_strerror(result));
        goto fail;
    }
    snd_pcm_uframes_t period_frames = buffer_frames / SHARP_ALSA_PERIODS;
    if (period_frames < 64) {
        period_frames = 64;
    }
    if ((result = snd_pcm_hw_params_set_period_size_near(implementation->pcm, hardware, &period_frames, 0)) <
        0) {
        SHARP_SetBackendError(backend, "cannot set period size: %s", snd_strerror(result));
        goto fail;
    }
    if ((result = snd_pcm_hw_params(implementation->pcm, hardware)) < 0) {
        SHARP_SetBackendError(backend, "cannot apply hw params: %s", snd_strerror(result));
        goto fail;
    }

    if (snd_pcm_hw_params_get_buffer_size(hardware, &implementation->buffer_frames) < 0 ||
        snd_pcm_hw_params_get_period_size(hardware, &implementation->period_frames, 0) < 0) {
        SHARP_SetBackendError(backend, "cannot read negotiated buffer sizes");
        goto fail;
    }
    size_t sample_bytes = implementation->format == SHARP_ALSA_F32 ? sizeof(float) : sizeof(int16_t);
    if (!implementation->buffer_frames || !implementation->period_frames ||
        implementation->buffer_frames > UINT32_MAX / 2u ||
        implementation->period_frames > implementation->buffer_frames ||
        implementation->period_frames > UINT32_MAX ||
        (uint64_t)implementation->period_frames * 2 > SIZE_MAX / sizeof(float) ||
        (uint64_t)implementation->period_frames * implementation->device_channels > SIZE_MAX / sample_bytes) {
        SHARP_SetBackendError(backend, "invalid negotiated buffer dimensions");
        goto fail;
    }

    snd_pcm_sw_params_t* software;
    snd_pcm_sw_params_alloca(&software);
    if ((result = snd_pcm_sw_params_current(implementation->pcm, software)) < 0) {
        SHARP_SetBackendError(backend, "cannot read sw params: %s", snd_strerror(result));
        goto fail;
    }

    if (snd_pcm_sw_params_set_start_threshold(implementation->pcm, software, implementation->buffer_frames) <
            0 ||
        snd_pcm_sw_params_set_avail_min(implementation->pcm, software, implementation->period_frames) < 0) {
        SHARP_SetBackendError(backend, "cannot set software thresholds");
        goto fail;
    }
    if ((result = snd_pcm_sw_params(implementation->pcm, software)) < 0) {
        SHARP_SetBackendError(backend, "cannot apply sw params: %s", snd_strerror(result));
        goto fail;
    }

    uint32_t lead_frames = config->lead_frames ? config->lead_frames : backend->rate / 50;
    if (lead_frames < SHARP_ALSA_RENDER_BLOCK) {
        lead_frames = SHARP_ALSA_RENDER_BLOCK;
    }
    if (lead_frames < (uint32_t)implementation->buffer_frames * 2) {
        lead_frames = (uint32_t)implementation->buffer_frames * 2;
    }
    if (!SHARP_InitAudioRing(&implementation->ring, lead_frames, 2)) {
        SHARP_SetBackendError(backend, "ring alloc failed");
        goto fail;
    }

    implementation->device_scratch = calloc((size_t)implementation->period_frames * 2, sizeof(float));
    implementation->device_buffer =
        calloc((size_t)implementation->period_frames * implementation->device_channels,
               implementation->format == SHARP_ALSA_F32 ? sizeof(float) : sizeof(int16_t));
    if (!implementation->device_scratch || !implementation->device_buffer) {
        SHARP_SetBackendError(backend, "scratch alloc failed");
        goto fail;
    }

    backend->impl = implementation;
    return 1;

fail:
    sharp_alsa_release_resources(implementation);
    free(implementation);
    return 0;
}

static int sharp_alsa_start(SharpBackend* backend) {
    SharpAlsaState* implementation = backend->impl;
    if (atomic_load(&implementation->running)) {
        return 1;
    }
    if (atomic_load(&implementation->device_lost)) {
        SHARP_SetBackendError(backend, "reopen required after device failure");
        return 0;
    }

    while (SHARP_GetAudioRingWritableFrames(&implementation->ring) >= SHARP_ALSA_RENDER_BLOCK) {
        backend->render(backend->user, implementation->render_block, SHARP_ALSA_RENDER_BLOCK);
        SHARP_WriteAudioRing(&implementation->ring, implementation->render_block, SHARP_ALSA_RENDER_BLOCK);
    }

    int result = snd_pcm_prepare(implementation->pcm);
    if (result < 0) {
        SHARP_SetBackendError(backend, "cannot prepare device: %s", snd_strerror(result));
        return 0;
    }

    implementation->pcm_prepared = 1;
    atomic_store(&implementation->running, 1);
    implementation->device_thread = sharp_thread_start(sharp_alsa_device_worker, backend);
    implementation->render_thread = sharp_thread_start(sharp_alsa_render_worker, backend);
    if (!implementation->device_thread || !implementation->render_thread) {
        atomic_store(&implementation->running, 0);
        SHARP_SetBackendError(backend, "could not start threads");
        sharp_alsa_stop(backend);
        return 0;
    }
    return 1;
}

static int sharp_alsa_wait(SharpBackend* backend, uint64_t frames) {
    SharpAlsaState* implementation = backend->impl;
    while (atomic_load(&backend->frames) < frames) {
        if (atomic_load_explicit(&implementation->device_lost, memory_order_relaxed)) {
            SHARP_SetBackendError(backend, "audio device was lost (unplugged or changed)");
            return 0;
        }
        if (!atomic_load(&implementation->running)) {
            SHARP_SetBackendError(backend, "audio backend is stopped");
            return 0;
        }
        sharp_thread_sleep_milliseconds(5);
    }
    return 1;
}

static void sharp_alsa_stop(SharpBackend* backend) {
    SharpAlsaState* implementation = backend->impl;
    if (!implementation) {
        return;
    }
    atomic_store(&implementation->running, 0);

    if (implementation->pcm_prepared) {
        snd_pcm_drop(implementation->pcm);
        implementation->pcm_prepared = 0;
    }
    sharp_thread_join(implementation->render_thread);
    implementation->render_thread = NULL;
    sharp_thread_join(implementation->device_thread);
    implementation->device_thread = NULL;
}

static void sharp_alsa_close(SharpBackend* backend) {
    SharpAlsaState* implementation = backend->impl;
    if (!implementation) {
        return;
    }
    sharp_alsa_stop(backend);
    sharp_alsa_release_resources(implementation);
    free(implementation);
    backend->impl = NULL;
}

long sharp_alsa_get_underrun_count(const SharpBackend* backend) {
    const SharpAlsaState* implementation = backend ? (const SharpAlsaState*)backend->impl : NULL;
    return implementation ? atomic_load_explicit(&implementation->underruns, memory_order_relaxed) : 0;
}

const SharpBackendVTable sharp_device_vtable = {"ALSA",          sharp_alsa_open, sharp_alsa_start,
                                                sharp_alsa_wait, sharp_alsa_stop, sharp_alsa_close};
