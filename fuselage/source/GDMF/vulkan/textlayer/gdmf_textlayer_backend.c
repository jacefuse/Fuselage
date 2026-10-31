// Text renderer ownership. SEE: (GDMF_TEXT.md, Prepared frames and lifecycle)
#include "../../textlayer/gdmf_textlayer_backend_internal.h"
#include "gdmf_textlayer_frame.h"
#include "../renderer/gdmf_vulkan_internal.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    VkDevice device;
    VkRenderPass render_pass;
    gdmf_textlayer_atlas atlas;
    gdmf_textlayer_descriptor_layout descriptor_layout;
    gdmf_textlayer_descriptors descriptors;
    gdmf_textlayer_pipeline pipeline;
    gdmf_textlayer_cell_buffer* buffers;
    uint32_t image_count;
    bool quiesced;
    gdmf_textlayer_draw draw;
} gdmf_textlayer_backend_state;
static gdmf_textlayer_backend_state gdmf_textlayer_backend;
static atomic_bool gdmf_textlayer_blocked;

bool gdmf_textlayer_backend_healthy(void) {
    return !atomic_load(&gdmf_textlayer_blocked);
}
bool gdmf_textlayer_backend_has_resources(void) {
    return gdmf_textlayer_backend.device != VK_NULL_HANDLE;
}
static bool gdmf_textlayer_backend_block(void) {
    if (!atomic_exchange(&gdmf_textlayer_blocked, true)) {
        fprintf(stderr, "[Text] Lifecycle or upload completion unresolved; resources retained\n");
    }
    return false;
}
void gdmf_textlayer_backend_note_quiesced(void) {
    gdmf_textlayer_backend.quiesced = true;
    gdmf_textlayer_backend.draw = (gdmf_textlayer_draw){0};
}
static void gdmf_textlayer_backend_release_frames(void) {
    gdmf_textlayer_pipeline_destroy(&gdmf_textlayer_backend.pipeline);
    gdmf_textlayer_descriptors_destroy(&gdmf_textlayer_backend.descriptors);
    if (gdmf_textlayer_backend.buffers) {
        for (uint32_t index = 0; index < gdmf_textlayer_backend.image_count; ++index) {
            gdmf_textlayer_cell_buffer_destroy(&gdmf_textlayer_backend.buffers[index]);
        }
    }
    free(gdmf_textlayer_backend.buffers);
    gdmf_textlayer_backend.buffers = NULL;
    gdmf_textlayer_backend.image_count = 0;
    gdmf_textlayer_backend.render_pass = VK_NULL_HANDLE;
    gdmf_textlayer_backend.draw = (gdmf_textlayer_draw){0};
}
bool gdmf_textlayer_shutdown(void) {
    if (gdmf_textlayer_backend.device && !gdmf_textlayer_backend.quiesced) {
        return gdmf_textlayer_backend_block();
    }
    gdmf_textlayer_backend_release_frames();
    if (!gdmf_textlayer_atlas_destroy(&gdmf_textlayer_backend.atlas, true)) {
        return gdmf_textlayer_backend_block();
    }
    gdmf_textlayer_descriptor_layout_destroy(&gdmf_textlayer_backend.descriptor_layout);
    memset(&gdmf_textlayer_backend, 0, sizeof(gdmf_textlayer_backend));
    atomic_store(&gdmf_textlayer_blocked, false);
    return true;
}
void gdmf_textlayer_on_swapchain_recreated(void) {
    if (!gdmf_textlayer_backend.device) {
        return;
    }
    if (!gdmf_textlayer_backend.quiesced || !gdmf_textlayer_backend_healthy()) {
        gdmf_textlayer_backend_block();
        return;
    }
    // Runs before the coordinator replaces the render pass. Atlas survives.
    gdmf_textlayer_backend_release_frames();
}
static bool gdmf_textlayer_backend_setup(void) {
    VkDevice device = gdmf_get_device();
    VkRenderPass render_pass = gdmf_get_render_pass();
    uint32_t image_count = gdmf_get_swapchain_image_count();
    if ((gdmf_textlayer_backend.device && gdmf_textlayer_backend.device != device) ||
        (gdmf_textlayer_backend.render_pass && gdmf_textlayer_backend.render_pass != render_pass) ||
        (gdmf_textlayer_backend.image_count && gdmf_textlayer_backend.image_count != image_count)) {
        return gdmf_textlayer_backend_block();
    }
    if (!device || !render_pass || !image_count || !gdmf_get_memory_properties()) {
        return false;
    }
    if (!gdmf_textlayer_backend.device) {
        gdmf_textlayer_backend.device = device;
        gdmf_textlayer_backend.quiesced = true;
    }
    if (!gdmf_textlayer_backend.atlas.ready) {
        gdmf_textlayer_atlas_config config = {device, gdmf_get_physical_device(),
                                              gdmf_get_memory_properties(), gdmf_get_upload_context()};
        gdmf_vulkan_upload_result result =
            gdmf_textlayer_atlas_create(&config, &gdmf_textlayer_backend.atlas);
        if (result == GDMF_VULKAN_UPLOAD_UNRESOLVED) {
            gdmf_textlayer_backend.quiesced = false;
            return gdmf_textlayer_backend_block();
        }
        if (result != GDMF_VULKAN_UPLOAD_COMPLETE) {
            return false;
        }
    }
    if (!gdmf_textlayer_backend.descriptor_layout.device &&
        !gdmf_textlayer_descriptor_layout_create(device, &gdmf_textlayer_backend.descriptor_layout)) {
        return false;
    }
    if (!gdmf_textlayer_backend.buffers) {
        if ((size_t)image_count > SIZE_MAX / sizeof(gdmf_textlayer_cell_buffer) ||
            (size_t)image_count > SIZE_MAX / sizeof(VkBuffer)) {
            return false;
        }
        gdmf_textlayer_cell_buffer* buffers = calloc(image_count, sizeof(*buffers));
        VkBuffer* buffer_handles = calloc(image_count, sizeof(*buffer_handles));
        gdmf_textlayer_descriptors descriptors = {0};
        if (!buffers || !buffer_handles) {
            goto fail;
        }
        for (uint32_t index = 0; index < image_count; ++index) {
            if (!gdmf_textlayer_cell_buffer_create(device, gdmf_get_memory_properties(), &buffers[index])) {
                goto fail;
            }
            buffer_handles[index] = buffers[index].buffer;
        }
        if (!gdmf_textlayer_descriptors_create(&gdmf_textlayer_backend.descriptor_layout,
                                               &gdmf_textlayer_backend.atlas, buffer_handles, image_count,
                                               &descriptors)) {
            goto fail;
        }
        free(buffer_handles);
        gdmf_textlayer_backend.buffers = buffers;
        gdmf_textlayer_backend.descriptors = descriptors;
        gdmf_textlayer_backend.image_count = image_count;
        gdmf_textlayer_backend.render_pass = render_pass;
        goto ready;
    fail:
        gdmf_textlayer_descriptors_destroy(&descriptors);
        if (buffers) {
            for (uint32_t index = 0; index < image_count; ++index) {
                gdmf_textlayer_cell_buffer_destroy(&buffers[index]);
            }
        }
        free(buffer_handles);
        free(buffers);
        return false;
    }
ready:
    return gdmf_textlayer_backend.pipeline.device ||
           gdmf_textlayer_pipeline_create(device, render_pass, gdmf_get_pipeline_cache(),
                                          gdmf_textlayer_backend.descriptor_layout.layout,
                                          &gdmf_textlayer_backend.pipeline);
}
void gdmf_textlayer_prepare(uint32_t image_index) {
    gdmf_textlayer_backend.draw = (gdmf_textlayer_draw){0};
    if (!gdmf_textlayer_backend_healthy()) {
        return;
    }
    const gdmf_textlayer_state* state = gdmf_textlayer_state_get();
    if (!state->active || image_index >= gdmf_get_swapchain_image_count()) {
        return;
    }
    if (!gdmf_textlayer_backend_setup()) {
        return;
    }
    if (image_index >= gdmf_textlayer_backend.image_count) {
        return;
    }
    if (gdmf_textlayer_frame_prepare(state, &gdmf_textlayer_backend.buffers[image_index],
                                     &gdmf_textlayer_backend.descriptors, &gdmf_textlayer_backend.pipeline,
                                     image_index, gdmf_get_render_viewport_rect(),
                                     &gdmf_textlayer_backend.draw)) {
        gdmf_textlayer_backend.quiesced = false;
    }
}
void gdmf_textlayer_record(VkCommandBuffer command_buffer, uint32_t image_index) {
    if (gdmf_textlayer_backend_healthy()) {
        gdmf_textlayer_draw_record(&gdmf_textlayer_backend.draw, command_buffer, image_index);
    }
}
