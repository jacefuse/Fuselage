#ifndef SHARP_WASAPI_PRIVATE_H
#define SHARP_WASAPI_PRIVATE_H
#define COBJMACROS
#define CINTERFACE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include "../../backend/sharp_backend_private.h"
#include "../../transport/sharp_ring.h"
#include "../../backend/sharp_thread.h"

#define SHARP_WASAPI_RENDER_BLOCK 256
typedef enum { SHARP_WASAPI_F32, SHARP_WASAPI_S16 } SharpWasapiFormat;
typedef struct {
    IMMDeviceEnumerator* enumerator;
    IMMDevice* device;
    IAudioClient* client;
    IAudioRenderClient* render_client;
    WAVEFORMATEX* mix_format;
    HANDLE event;
    UINT32 buffer_frames;
    UINT32 device_channels;
    SharpWasapiFormat format;
    SharpRing ring;
    SharpThread* render_thread;
    SharpThread* device_thread;
    volatile LONG running;
    volatile LONG device_lost;
    volatile LONG underruns;
    int com_initialized;
    int client_started;
    float* device_scratch;
    float render_block[SHARP_WASAPI_RENDER_BLOCK * 2];
} SharpWasapiState;

void sharp_wasapi_render_worker(void* user);
void sharp_wasapi_device_worker(void* user);
int sharp_wasapi_select_format(const WAVEFORMATEX* format, SharpWasapiFormat* selected);
void sharp_wasapi_copy_frames(const SharpWasapiState* implementation, void* destination, uint32_t frames);
long sharp_wasapi_get_underrun_count(const SharpBackend* backend);
#endif
