#ifndef SHARP_COREAUDIO_PRIVATE_H
#define SHARP_COREAUDIO_PRIVATE_H
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include "../../backend/sharp_backend_private.h"
#include "../../transport/sharp_ring.h"
#include "../../backend/sharp_thread.h"
#define SHARP_COREAUDIO_RENDER_BLOCK 256
#define SHARP_COREAUDIO_STALL_MILLISECONDS 2000
typedef struct {
    AudioUnit unit;
    int unit_open, unit_initialized, unit_started;
    UInt32 device_channels, slice_capacity;
    SharpRing ring;
    SharpThread* render_thread;
    _Atomic int running, device_lost;
    _Atomic long underruns;
    float* device_scratch;
    float render_block[SHARP_COREAUDIO_RENDER_BLOCK * 2];
} SharpCoreAudioState;
void sharp_coreaudio_render_worker(void* user);
OSStatus sharp_coreaudio_device_callback(void* context, AudioUnitRenderActionFlags* flags,
                                         const AudioTimeStamp* timestamp, UInt32 bus, UInt32 frames,
                                         AudioBufferList* buffers);
long sharp_coreaudio_get_underrun_count(const SharpBackend* backend);
#endif
