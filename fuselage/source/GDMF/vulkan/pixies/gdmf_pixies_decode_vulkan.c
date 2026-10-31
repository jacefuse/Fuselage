// Queue-to-frame recording. SEE: (GDMF_PIXIES_FORMATS.md, GPU decode requests)
#include "gdmf_pixies_decode_vulkan.h"
#include "../renderer/gdmf_vulkan_internal.h"
#include "../../pixies/gdmf_pixies_decode_queue.h"
#include <string.h>

static bool gdmf_pixies_decode_record_one(void* context, const gdmf_pixies_decode_request* request) {
    const gdmf_pixies_decode_recording* recording = context;
    const gdmf_pixies_state* state = gdmf_pixies_state_get(request->pixie_id);
    GdmfComputeImage image = {0};
    if (!state || state->mode != PIXIE_MODE_TEXTURE ||
        !recording->lookup_image(recording->image_context, request->pixie_id, &image) || !image.image ||
        !image.view || image.width != (uint32_t)state->output_width ||
        image.height != (uint32_t)state->output_height)
        return false;
    uint32_t alignment = gdmf_compute_buffer_alignment();
    if (!alignment || (alignment & (alignment - 1u)))
        return false;
    uint64_t bytes_span = ((uint64_t)request->byte_count + alignment - 1u) & ~((uint64_t)alignment - 1u);
    uint64_t total = (uint64_t)alignment + bytes_span + request->palette_bytes;
    if (total > SIZE_MAX)
        return false;
    VkBuffer buffer;
    VkDeviceSize offset;
    unsigned char* destination =
        gdmf_frame_staging_alloc(recording->staging, recording->image_index, total, &buffer, &offset);
    if (!destination || !buffer || offset > UINT64_MAX - total)
        return false;
    VkDeviceSize aligned = (offset + alignment - 1u) & ~((uint64_t)alignment - 1u);
    destination += (size_t)(aligned - offset);
    memcpy(destination, request->bytes, request->byte_count);
    if (request->palette_bytes)
        memcpy(destination + (size_t)bytes_span, request->palette, request->palette_bytes);
    const GdmfKernelDescription* description = gdmf_kernel_desc(request->kernel);
    if (!description)
        return false;
    GdmfComputeDispatch dispatch = {
        .kernel = request->kernel,
        .bytes = {.handle = (uint64_t)buffer, .offset = aligned, .size = request->byte_count},
        .palette = {.handle = (uint64_t)buffer, .offset = aligned, .size = request->byte_count},
        .image = image,
        .push = &request->push,
        .push_bytes = request->push_bytes,
    };
    // Raw RGBA does not read the palette, but its binding still needs a descriptor.
    if (request->palette_bytes) {
        dispatch.palette.offset = aligned + bytes_span;
        dispatch.palette.size = request->palette_bytes;
    }
    if (request->kernel == GDMF_KERNEL_DECODE_RLE)
        gdmf_kernel_groups(description, request->rows, 1, &dispatch.groups_x, &dispatch.groups_y);
    else
        gdmf_kernel_groups(description, request->push.raw.width, request->push.raw.height, &dispatch.groups_x,
                           &dispatch.groups_y);
    return gdmf_compute_record(recording->command_buffer, recording->image_index, &dispatch);
}

size_t gdmf_pixies_decode_record_pending(const gdmf_pixies_decode_recording* recording) {
    if (!recording || !recording->command_buffer || !recording->staging || !recording->lookup_image ||
        !gdmf_compute_ready())
        return 0;
    return gdmf_pixies_decode_requests_drain(gdmf_pixies_decode_record_one, (void*)recording);
}
