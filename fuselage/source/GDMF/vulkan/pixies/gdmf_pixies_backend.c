// Backend ownership and layer hooks. SEE: (GDMF_PIXIES.md, Backend lifecycle)
#include "gdmf_pixies_backend_internal.h"
#include "gdmf_pixies_frame.h"
#include "../renderer/gdmf_vulkan_internal.h"
#include "../../window/gdmf_canvas.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

typedef struct {
    VkDevice device;
    VkRenderPass render_pass;
    gdmf_pixies_image_context image_context;
    gdmf_pixies_image images[MAX_PIXIES];
    gdmf_pixies_pipeline texture_pipeline, live_pipeline;
    gdmf_pixies_frame* frames;
    uint32_t frame_count;
    gdmf_pixies_frame* active_frame;
    gdmf_vulkan_retirement_queue* retirement;
    bool retiring[MAX_PIXIES];
    bool quiesced, lifecycle_error;
} gdmf_pixies_backend_state;

static gdmf_pixies_backend_state gdmf_pixies_backend;
// Stop polling can occur outside the drawing-state lock. This flag publishes
// status only; resource access still requires the documented lock boundaries.
static atomic_bool gdmf_pixies_backend_blocked;

bool gdmf_pixies_backend_healthy(void) {
    return !atomic_load(&gdmf_pixies_backend_blocked);
}
static void gdmf_pixies_backend_publish_status(void) {
    bool blocked = gdmf_pixies_backend.lifecycle_error;
    for (int id = 0; id < MAX_PIXIES; id++)
        blocked = blocked || gdmf_pixies_backend.retiring[id];
    atomic_store(&gdmf_pixies_backend_blocked, blocked);
}
bool gdmf_pixies_backend_has_resources(void) {
    return gdmf_pixies_backend.device != VK_NULL_HANDLE;
}
bool gdmf_pixies_backend_linear_filter_available(void) {
    return gdmf_pixies_backend_healthy() &&
           gdmf_pixies_backend.image_context.linear_sampler != VK_NULL_HANDLE;
}
void gdmf_pixies_backend_note_quiesced(void) {
    gdmf_pixies_backend.quiesced = true;
    gdmf_pixies_backend.active_frame = NULL;
    for (uint32_t index = 0; index < gdmf_pixies_backend.frame_count; index++)
        gdmf_pixies_backend.frames[index].valid = false;
}

static bool gdmf_pixies_backend_retire_image(int id) {
    gdmf_pixies_backend_state* backend = &gdmf_pixies_backend;
    gdmf_pixies_image* image = &backend->images[id];
    gdmf_vulkan_retirement_queue* queue = backend->retirement;
    if (!queue || !queue->lock || queue->device != backend->device)
        return false;
    if (image->nearest_set) {
        if (!gdmf_defer_free_descriptor_set(queue, backend->image_context.descriptor_pool,
                                            image->nearest_set))
            return false;
        image->nearest_set = VK_NULL_HANDLE;
    }
    if (image->linear_set) {
        if (!gdmf_defer_free_descriptor_set(queue, backend->image_context.descriptor_pool, image->linear_set))
            return false;
        image->linear_set = VK_NULL_HANDLE;
    }
    if (image->image || image->view || image->memory) {
        if (!gdmf_defer_destroy_image(queue, image->image, image->view, image->memory))
            return false;
        image->image = VK_NULL_HANDLE;
        image->view = VK_NULL_HANDLE;
        image->memory = VK_NULL_HANDLE;
    }
    for (uint32_t index = 0; index < image->staging_count; index++) {
        gdmf_pixies_image_staging* slot = &image->staging[index];
        if (slot->buffer || slot->memory) {
            if (!gdmf_defer_destroy_buffer(queue, slot->buffer, slot->memory))
                return false;
            // Freeing the retired memory will implicitly unmap it.
            memset(slot, 0, sizeof *slot);
        }
    }
    free(image->staging);
    memset(image, 0, sizeof *image);
    return true;
}

bool gdmf_pixies_backend_release(int id) {
    if (id < 0 || id >= MAX_PIXIES)
        return false;
    gdmf_pixies_backend_state* backend = &gdmf_pixies_backend;
    if (!backend->device)
        return true;
    gdmf_pixies_image* image = &backend->images[id];
    if (!image->image && !image->view && !image->memory && !image->nearest_set && !image->linear_set &&
        !image->staging) {
        backend->retiring[id] = false;
        gdmf_pixies_backend_publish_status();
        return true;
    }
    backend->retiring[id] = true;
    bool released = backend->quiesced ? gdmf_pixies_image_destroy(&backend->image_context, image)
                                      : gdmf_pixies_backend_retire_image(id);
    if (!released) {
        gdmf_pixies_backend_publish_status();
        fprintf(stderr, "[Pixies] Release retained resources for ID %d; preparation stopped\n", id);
        return false;
    }
    backend->retiring[id] = false;
    gdmf_pixies_backend_publish_status();
    return true;
}

static void gdmf_pixies_backend_release_frames(void) {
    gdmf_pixies_backend_state* backend = &gdmf_pixies_backend;
    for (uint32_t index = 0; index < backend->frame_count; index++)
        gdmf_pixies_frame_destroy(backend->device, &backend->frames[index]);
    free(backend->frames);
    backend->frames = NULL;
    backend->frame_count = 0;
    backend->active_frame = NULL;
    gdmf_pixies_pipeline_destroy(backend->device, &backend->texture_pipeline);
    gdmf_pixies_pipeline_destroy(backend->device, &backend->live_pipeline);
    backend->render_pass = VK_NULL_HANDLE;
}

bool gdmf_pixies_backend_shutdown(void) {
    gdmf_pixies_backend_state* backend = &gdmf_pixies_backend;
    for (int id = 0; id < MAX_PIXIES; id++)
        if (gdmf_pixies_state_get(id))
            return false;
    if (!backend->device) {
        memset(backend, 0, sizeof *backend);
        gdmf_pixies_backend_publish_status();
        return true;
    }
    if (!backend->quiesced)
        return false;
    for (int id = 0; id < MAX_PIXIES; id++) {
        const gdmf_pixies_image* image = &backend->images[id];
        if (image->image || image->view || image->memory || image->nearest_set || image->linear_set ||
            image->staging)
            return false;
    }
    gdmf_pixies_backend_release_frames();
    // Completed, discarded references permit pool destruction. The pool reclaims
    // deferred sets, so remove their individual frees before destroying it.
    if (backend->retirement && backend->retirement->lock && backend->image_context.descriptor_pool)
        gdmf_defer_forget_descriptor_pool(backend->retirement, backend->image_context.descriptor_pool);
    gdmf_pixies_image_context_destroy(&backend->image_context);
    memset(backend, 0, sizeof *backend);
    gdmf_pixies_backend_publish_status();
    return true;
}

void gdmf_pixies_on_swapchain_recreated(void) {
    gdmf_pixies_backend_state* backend = &gdmf_pixies_backend;
    if (!backend->device)
        return;
    if (!backend->quiesced || !gdmf_pixies_backend_healthy()) {
        backend->lifecycle_error = true;
        gdmf_pixies_backend_publish_status();
        return;
    }
    gdmf_pixies_backend_release_frames();
    for (int id = 0; id < MAX_PIXIES; id++)
        gdmf_pixies_image_staging_reset(&backend->image_context, &backend->images[id]);
}

void gdmf_pixies_prepare(VkCommandBuffer command_buffer, uint32_t image_index) {
    gdmf_pixies_backend_state* backend = &gdmf_pixies_backend;
    backend->active_frame = NULL;
    if (!gdmf_pixies_backend_healthy())
        return;
    VkDevice device = gdmf_get_device();
    VkRenderPass render_pass = gdmf_get_render_pass();
    uint32_t image_count = gdmf_get_swapchain_image_count();
    const VkPhysicalDeviceMemoryProperties* properties = gdmf_get_memory_properties();
    if (!device || !render_pass || !properties || !command_buffer || image_index >= image_count)
        return;
    if ((backend->device && backend->device != device) ||
        (backend->render_pass && backend->render_pass != render_pass) ||
        (backend->frame_count && backend->frame_count != image_count)) {
        backend->lifecycle_error = true;
        gdmf_pixies_backend_publish_status();
        return;
    }
    backend->device = device;
    backend->quiesced = false;
    backend->retirement = gdmf_get_retirement_queue();
    if (!backend->frames) {
        if ((size_t)image_count > SIZE_MAX / sizeof *backend->frames)
            return;
        backend->frames = calloc(image_count, sizeof *backend->frames);
        if (!backend->frames)
            return;
        backend->frame_count = image_count;
        backend->render_pass = render_pass;
    }
    if (!backend->image_context.device)
        gdmf_pixies_image_context_create(device, gdmf_get_physical_device(), properties,
                                         &backend->image_context);
    if (backend->image_context.device && !backend->texture_pipeline.pipeline)
        gdmf_pixies_pipeline_create(device, render_pass, gdmf_get_pipeline_cache(),
                                    backend->image_context.descriptor_layout, false,
                                    &backend->texture_pipeline);
    if (!backend->live_pipeline.pipeline)
        gdmf_pixies_pipeline_create(device, render_pass, gdmf_get_pipeline_cache(), VK_NULL_HANDLE, true,
                                    &backend->live_pipeline);
    // LIVE can still prepare if shared image setup failed this frame.
    gdmf_pixies_image_context live_context = {.device = device, .memory_properties = *properties};
    gdmf_pixies_frame_context context = {
        .image_context = backend->image_context.device ? &backend->image_context : &live_context,
        .images = backend->images,
        .texture_pipeline = &backend->texture_pipeline,
        .live_pipeline = &backend->live_pipeline,
        .image_count = image_count,
        .canvas_width = (float)GDMF_GetCanvasWidth(),
        .canvas_height = (float)GDMF_GetCanvasHeight(),
        .viewport = gdmf_get_render_viewport_rect(),
        .decode_staging = gdmf_get_frame_staging()};
    gdmf_pixies_frame* frame = &backend->frames[image_index];
    if (gdmf_pixies_frame_prepare(&context, frame, command_buffer, image_index))
        backend->active_frame = frame;
}

void gdmf_pixies_record_priority(VkCommandBuffer command_buffer, uint32_t image_index, uint8_t priority) {
    const gdmf_pixies_frame* frame = gdmf_pixies_backend.active_frame;
    if (frame && frame->image_index == image_index)
        gdmf_pixies_frame_record_priority(frame, command_buffer, priority);
}
int gdmf_pixies_priority_mask_id(uint8_t priority) {
    return gdmf_pixies_frame_mask_id(gdmf_pixies_backend.active_frame, priority);
}
bool gdmf_pixies_get_mask_info(int id, VkImageView* view, float rect[4], bool* inverted) {
    return gdmf_pixies_frame_mask_info(gdmf_pixies_backend.active_frame, id, view, rect, inverted);
}
