// Renderer ownership and layer hooks. SEE: (GDMF_SPRITES.md, Backend lifecycle)
#include "gdmf_sprites_backend_internal.h"
#include "gdmf_sprites_frame.h"
#include "../renderer/gdmf_vulkan_internal.h"
#include "../../sprites/gdmf_sprites_collision_private.h"
#include "../../window/gdmf_canvas.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    VkDevice device;
    VkRenderPass render_pass;
    gdmf_sprites_atlas atlas;
    gdmf_sprites_descriptors descriptors;
    gdmf_sprites_pipeline pipeline;
    gdmf_sprites_frame* frames;
    VkBuffer* palette_buffers;
    uint32_t frame_count;
    gdmf_sprites_frame* active_frame;
    bool quiesced;
} gdmf_sprites_backend_state;

static gdmf_sprites_backend_state gdmf_sprites_backend;
static atomic_bool gdmf_sprites_backend_blocked;

bool gdmf_sprites_backend_healthy(void) {
    return !atomic_load(&gdmf_sprites_backend_blocked);
}
bool gdmf_sprites_backend_has_resources(void) {
    return gdmf_sprites_backend.device != VK_NULL_HANDLE;
}
static bool gdmf_sprites_backend_block(void) {
    gdmf_sprites_backend.active_frame = NULL;
    if (!atomic_exchange(&gdmf_sprites_backend_blocked, true)) {
        fprintf(stderr, "[Sprites] Lifecycle mismatch; resources retained and preparation stopped\n");
    }
    return false;
}
void gdmf_sprites_backend_note_quiesced(void) {
    gdmf_sprites_backend_state* backend = &gdmf_sprites_backend;
    backend->quiesced = true;
    backend->active_frame = NULL;
    for (uint32_t index = 0; index < backend->frame_count; ++index) {
        gdmf_sprites_frame_invalidate(&backend->frames[index]);
    }
}
static void gdmf_sprites_backend_release_frames(void) {
    gdmf_sprites_backend_state* backend = &gdmf_sprites_backend;
    for (uint32_t index = 0; index < backend->frame_count; ++index) {
        gdmf_sprites_frame_destroy(&backend->frames[index]);
    }
    free(backend->frames);
    free(backend->palette_buffers);
    backend->frames = NULL;
    backend->palette_buffers = NULL;
    backend->frame_count = 0;
    backend->active_frame = NULL;
    gdmf_sprites_pipeline_destroy(&backend->pipeline);
    gdmf_sprites_descriptors_destroy(&backend->descriptors);
    backend->render_pass = VK_NULL_HANDLE;
}
bool gdmf_sprites_backend_shutdown(void) {
    gdmf_sprites_backend_state* backend = &gdmf_sprites_backend;
    if (backend->device && !backend->quiesced) {
        return gdmf_sprites_backend_block();
    }
    gdmf_sprites_backend_release_frames();
    gdmf_sprites_atlas_destroy(&backend->atlas);
    memset(backend, 0, sizeof(*backend));
    atomic_store(&gdmf_sprites_backend_blocked, false);
    return true;
}
void gdmf_sprites_on_swapchain_recreated(void) {
    gdmf_sprites_backend_state* backend = &gdmf_sprites_backend;
    if (!backend->device) {
        return;
    }
    if (!backend->quiesced || !gdmf_sprites_backend_healthy()) {
        gdmf_sprites_backend_block();
        return;
    }
    // Called before the renderer destroys borrowed palette buffers/render pass.
    gdmf_sprites_backend_release_frames();
}

static bool gdmf_sprites_backend_setup(void) {
    gdmf_sprites_backend_state* backend = &gdmf_sprites_backend;
    if (!gdmf_sprites_backend_healthy()) {
        return false;
    }
    VkDevice device = gdmf_get_device();
    VkRenderPass render_pass = gdmf_get_render_pass();
    uint32_t image_count = gdmf_get_swapchain_image_count();
    const VkPhysicalDeviceMemoryProperties* properties = gdmf_get_memory_properties();
    if ((backend->device && backend->device != device) ||
        (backend->render_pass && backend->render_pass != render_pass) ||
        (backend->frame_count && backend->frame_count != image_count)) {
        return gdmf_sprites_backend_block();
    }
    if (!device || !properties) {
        return false;
    }
    if (!backend->device) {
        backend->device = device;
        backend->quiesced = true;
    }
    if (!backend->atlas.device &&
        !gdmf_sprites_atlas_create(device, gdmf_get_physical_device(), properties, &backend->atlas)) {
        return false;
    }
    if (!render_pass || !image_count) {
        return false;
    }
    if (!backend->frames) {
        if ((size_t)image_count > SIZE_MAX / sizeof(*backend->frames) ||
            (size_t)image_count > SIZE_MAX / sizeof(*backend->palette_buffers)) {
            return false;
        }
        gdmf_sprites_frame* frames = calloc(image_count, sizeof(*frames));
        VkBuffer* palettes = calloc(image_count, sizeof(*palettes));
        if (!frames || !palettes) {
            free(frames);
            free(palettes);
            return false;
        }
        backend->frames = frames;
        backend->palette_buffers = palettes;
        backend->frame_count = image_count;
        backend->render_pass = render_pass;
    }
    for (uint32_t index = 0; index < image_count; ++index) {
        VkBuffer palette = gdmf_get_palette_buffer(index);
        if (backend->descriptors.device && backend->palette_buffers[index] != palette) {
            return gdmf_sprites_backend_block();
        }
        if (!palette) {
            return false;
        }
        backend->palette_buffers[index] = palette;
    }
    if (!backend->descriptors.device &&
        !gdmf_sprites_descriptors_create(device, &backend->atlas, backend->palette_buffers, image_count,
                                         &backend->descriptors)) {
        return false;
    }
    if (!backend->pipeline.device &&
        !gdmf_sprites_pipeline_create(device, render_pass, gdmf_get_pipeline_cache(),
                                      backend->descriptors.layout, &backend->pipeline)) {
        return false;
    }
    return true;
}
bool gdmf_sprites_backend_initialize(void) {
    // Reinitializing host state reuses GPU ownership; it does not destroy frames
    // that may still be in flight. The new test bitmap is already marked dirty.
    gdmf_sprites_backend.active_frame = NULL;
    return gdmf_sprites_backend_setup();
}
void gdmf_sprites_prepare(VkCommandBuffer command_buffer, uint32_t image_index) {
    gdmf_sprites_backend_state* backend = &gdmf_sprites_backend;
    backend->active_frame = NULL;
    if (!gdmf_sprites_backend_healthy()) {
        return;
    }
    gdmf_sprites_collisions_run();
    bool graphics_ready = gdmf_sprites_backend_setup();
    if (!gdmf_sprites_backend_healthy() || !command_buffer || !backend->atlas.device ||
        image_index >= gdmf_get_swapchain_image_count()) {
        return;
    }
    // Transfers do not depend on descriptor or pipeline construction succeeding.
    backend->quiesced = false;
    gdmf_sprites_atlas_upload(&backend->atlas, command_buffer, gdmf_get_frame_staging(), image_index);
    if (!graphics_ready || image_index >= backend->frame_count) {
        return;
    }
    gdmf_sprites_frame_context context = {.device = backend->device,
                                          .memory_properties = gdmf_get_memory_properties(),
                                          .atlas = &backend->atlas,
                                          .descriptors = &backend->descriptors,
                                          .pipeline = &backend->pipeline,
                                          .canvas_width = (float)GDMF_GetCanvasWidth(),
                                          .canvas_height = (float)GDMF_GetCanvasHeight(),
                                          .viewport = gdmf_get_render_viewport_rect()};
    gdmf_sprites_frame* frame = &backend->frames[image_index];
    if (gdmf_sprites_frame_prepare(&context, image_index, frame)) {
        backend->active_frame = frame;
    }
}
void gdmf_sprites_record_priority(VkCommandBuffer command_buffer, uint32_t image_index, uint8_t priority) {
    if (gdmf_sprites_backend_healthy()) {
        gdmf_sprites_frame_record(gdmf_sprites_backend.active_frame, command_buffer, image_index, priority);
    }
}
int GetSpriteRenderedCount(void) {
    const gdmf_sprites_frame* frame = gdmf_sprites_backend.active_frame;
    return frame && frame->prepared ? (int)frame->sprite_count : 0;
}
void ToggleSpriteAtlasView(void) {}
bool GetSpriteAtlasViewActive(void) {
    return false;
}
