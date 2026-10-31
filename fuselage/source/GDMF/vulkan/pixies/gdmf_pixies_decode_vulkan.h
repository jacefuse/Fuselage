#ifndef GDMF_PIXIES_DECODE_VULKAN_H
#define GDMF_PIXIES_DECODE_VULKAN_H

#include "../renderer/resources/gdmf_vulkan_staging.h"
#include "../../compute/gdmf_compute.h"

// Lookup succeeds only after host uploads and image initialization are recorded.
// The image/view are borrowed, in SHADER_READ_ONLY_OPTIMAL layout, with matching
// output dimensions. The compute recorder performs and restores its own transitions.
typedef bool (*gdmf_pixies_decode_image_lookup)(void* context, int id, GdmfComputeImage* image);
typedef struct {
    VkCommandBuffer command_buffer;
    uint32_t image_index;
    gdmf_vulkan_staging* staging;
    gdmf_pixies_decode_image_lookup lookup_image;
    void* image_context;
} gdmf_pixies_decode_recording;

// Outside a render pass, under the state lock, after compute/frame preparation
// and dirty uploads. Stops at the first unavailable target, staging or dispatch.
// Returns requests recorded, not GPU completion. The backend supplies readiness.
size_t gdmf_pixies_decode_record_pending(const gdmf_pixies_decode_recording* recording);

#endif
