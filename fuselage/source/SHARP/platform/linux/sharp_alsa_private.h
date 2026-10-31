#ifndef SHARP_ALSA_PRIVATE_H
#define SHARP_ALSA_PRIVATE_H
#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include <alsa/asoundlib.h>
#include <alloca.h>
#include "../../backend/sharp_backend_private.h"
#include "../../transport/sharp_ring.h"
#include "../../backend/sharp_thread.h"
#define SHARP_ALSA_RENDER_BLOCK 256
typedef enum { SHARP_ALSA_F32, SHARP_ALSA_S16 } SharpAlsaFormat;
typedef struct {
    snd_pcm_t* pcm;
    snd_pcm_uframes_t period_frames, buffer_frames;
    unsigned device_channels;
    SharpAlsaFormat format;
    SharpRing ring;
    SharpThread *render_thread, *device_thread;
    _Atomic int running, device_lost;
    _Atomic long underruns;
    float* device_scratch;
    void* device_buffer;
    float render_block[SHARP_ALSA_RENDER_BLOCK * 2];
    int pcm_prepared;
} SharpAlsaState;
void sharp_alsa_render_worker(void* user);
void sharp_alsa_device_worker(void* user);
void sharp_alsa_copy_frames(const SharpAlsaState* implementation, void* destination, uint32_t frames);
long sharp_alsa_get_underrun_count(const SharpBackend* backend);
#endif
