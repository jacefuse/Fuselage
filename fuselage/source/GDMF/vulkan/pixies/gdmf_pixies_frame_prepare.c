// Build a frozen frame under the state lock. SEE: (GDMF_PIXIES.md, Frame preparation)
#include "gdmf_pixies_frame.h"
#include "gdmf_pixies_decode_vulkan.h"
#include <math.h>
#include <string.h>

static bool gdmf_pixies_frame_image(void* user, int id, GdmfComputeImage* result) {
    const gdmf_pixies_frame_context* context = user;
    const gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    const gdmf_pixies_image* image = &context->images[id];
    if (!state || state->mode != PIXIE_MODE_TEXTURE || state->dirty || !image->layout_initialized)
        return false;
    *result = (GdmfComputeImage){.image = (uint64_t)image->image,
                                 .view = (uint64_t)image->view,
                                 .width = image->width,
                                 .height = image->height};
    return true;
}

bool gdmf_pixies_frame_prepare(const gdmf_pixies_frame_context* context, gdmf_pixies_frame* frame,
                               VkCommandBuffer command_buffer, uint32_t image_index) {
    if (!frame)
        return false;
    frame->valid = false;
    memset(frame->draws, 0, sizeof frame->draws);
    memset(frame->masks, 0, sizeof frame->masks);
    for (unsigned priority = 0; priority < 256; priority++)
        frame->masks[priority].id = -1;
    frame->unavailable_images = 0;
    frame->recorded_decodes = 0;
    if (!context || !context->image_context || !context->image_context->device || !context->images ||
        !command_buffer || image_index >= context->image_count || !isfinite(context->canvas_width) ||
        !isfinite(context->canvas_height) || context->canvas_width <= 0 || context->canvas_height <= 0 ||
        context->viewport.offset.x < 0 || context->viewport.offset.y < 0 ||
        context->viewport.extent.width > (uint32_t)(INT32_MAX - context->viewport.offset.x) ||
        context->viewport.extent.height > (uint32_t)(INT32_MAX - context->viewport.offset.y))
        return false;
    frame->viewport = context->viewport;
    frame->image_index = image_index;
    frame->texture_pipeline =
        context->texture_pipeline ? *context->texture_pipeline : (gdmf_pixies_pipeline){0};
    frame->live_pipeline = context->live_pipeline ? *context->live_pipeline : (gdmf_pixies_pipeline){0};
    for (int id = 0; id < MAX_PIXIES; id++) {
        gdmf_pixies_state* state = gdmf_pixies_state_get(id);
        if (!state || state->mode == PIXIE_MODE_LIVE)
            continue;
        gdmf_pixies_image* image = &context->images[id];
        if (!image->image && !gdmf_pixies_image_create(context->image_context, (uint32_t)state->output_width,
                                                       (uint32_t)state->output_height,
                                                       state->mode == PIXIE_MODE_MASK, image)) {
            frame->unavailable_images |= 1u << id;
            continue;
        }
        if (image->width != (uint32_t)state->output_width ||
            image->height != (uint32_t)state->output_height ||
            image->mask != (state->mode == PIXIE_MODE_MASK)) {
            frame->unavailable_images |= 1u << id;
            continue;
        }
        if (state->dirty || !image->layout_initialized) {
            size_t pixels = (size_t)state->output_width * state->output_height,
                   bytes_per_pixel = image->mask ? 1u : 4u;
            const void* source = image->mask ? (const void*)state->mask : (const void*)state->output;
            if (pixels > SIZE_MAX / bytes_per_pixel ||
                !gdmf_pixies_image_upload(context->image_context, image, command_buffer, image_index,
                                          context->image_count, source, pixels * bytes_per_pixel))
                frame->unavailable_images |= 1u << id;
            else
                state->dirty = false;
        }
    }
    if (context->decode_staging) {
        gdmf_pixies_decode_recording recording = {.command_buffer = command_buffer,
                                                  .image_index = image_index,
                                                  .staging = context->decode_staging,
                                                  .lookup_image = gdmf_pixies_frame_image,
                                                  .image_context = (void*)context};
        frame->recorded_decodes = gdmf_pixies_decode_record_pending(&recording);
    }
    uint32_t texture_count = 0, live_count = 0;
    for (int id = 0; id < MAX_PIXIES; id++) {
        const gdmf_pixies_state* state = gdmf_pixies_state_get(id);
        if (!state || !state->enabled || !state->shown)
            continue;
        const gdmf_pixies_image* image = &context->images[id];
        bool ready = image->layout_initialized && image->view &&
                     image->width == (uint32_t)state->output_width &&
                     image->height == (uint32_t)state->output_height &&
                     image->mask == (state->mode == PIXIE_MODE_MASK);
        if (state->mode == PIXIE_MODE_MASK) {
            gdmf_pixies_mask_snapshot* mask = &frame->masks[state->priority];
            if (!ready || mask->id >= 0)
                continue;
            float rect[4] = {state->position_x / context->canvas_width * 2 - 1,
                             state->position_y / context->canvas_height * 2 - 1,
                             (state->position_x + state->display_width) / context->canvas_width * 2 - 1,
                             (state->position_y + state->display_height) / context->canvas_height * 2 - 1};
            if (!isfinite(rect[0]) || !isfinite(rect[1]) || !isfinite(rect[2]) || !isfinite(rect[3]))
                continue;
            *mask =
                (gdmf_pixies_mask_snapshot){.id = id, .view = image->view, .inverted = state->mask_inverted};
            memcpy(mask->rect, rect, sizeof rect);
            continue;
        }
        gdmf_pixies_draw_snapshot* draw = &frame->draws[id];
        if (!gdmf_pixies_frame_scissor(state, context->canvas_width, context->canvas_height,
                                       context->viewport, &draw->scissor))
            continue;
        draw->mode = state->mode;
        draw->priority = state->priority;
        if (state->mode == PIXIE_MODE_TEXTURE && ready && frame->texture_pipeline.pipeline) {
            draw->descriptor =
                state->linear_filter && image->linear_set ? image->linear_set : image->nearest_set;
            if (!draw->descriptor)
                continue;
            draw->first_vertex = texture_count;
            draw->vertex_count = 6;
            texture_count += 6;
        } else if (state->mode == PIXIE_MODE_LIVE && frame->live_pipeline.pipeline &&
                   state->live_vertex_count > 0 && state->live_vertex_count <= state->live_vertex_capacity &&
                   state->live_vertex_count <= GDMF_PIXIES_LIVE_MAX_VERTICES && state->live_vertices) {
            draw->first_vertex = live_count;
            draw->vertex_count = (uint32_t)state->live_vertex_count;
            live_count += draw->vertex_count;
        }
    }
    VkDevice device = context->image_context->device;
    const VkPhysicalDeviceMemoryProperties* properties = &context->image_context->memory_properties;
    bool texture_ready =
        texture_count && gdmf_pixies_vertex_buffer_ensure(device, properties,
                                                          texture_count * sizeof(gdmf_pixies_texture_vertex),
                                                          0, &frame->texture_vertices);
    bool live_ready = live_count && gdmf_pixies_vertex_buffer_ensure(
                                        device, properties, live_count * sizeof(gdmf_pixies_live_vertex),
                                        256 * sizeof(gdmf_pixies_live_vertex), &frame->live_vertices);
    for (int id = 0; id < MAX_PIXIES; id++) {
        gdmf_pixies_draw_snapshot* draw = &frame->draws[id];
        const gdmf_pixies_state* state = gdmf_pixies_state_get(id);
        if (!state || !draw->vertex_count)
            continue;
        if (draw->mode == PIXIE_MODE_TEXTURE && texture_ready) {
            gdmf_pixies_frame_quad(state, context->canvas_width, context->canvas_height,
                                   (gdmf_pixies_texture_vertex*)frame->texture_vertices.mapped +
                                       draw->first_vertex);
            draw->drawn = true;
        } else if (draw->mode == PIXIE_MODE_LIVE && live_ready) {
            memcpy((gdmf_pixies_live_vertex*)frame->live_vertices.mapped + draw->first_vertex,
                   state->live_vertices, draw->vertex_count * sizeof(gdmf_pixies_live_vertex));
            draw->drawn = true;
        }
    }
    frame->valid = true;
    return true;
}
