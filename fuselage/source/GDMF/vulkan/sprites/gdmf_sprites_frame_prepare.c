#include "gdmf_sprites_frame.h"
#include "../../sprites/gdmf_sprites_state_private.h"
#include <math.h>
#include <string.h>

void gdmf_sprites_frame_invalidate(gdmf_sprites_frame* frame) {
    if (!frame) {
        return;
    }
    frame->prepared = false;
    frame->sprite_count = 0;
    frame->pipeline = VK_NULL_HANDLE;
    frame->layout = VK_NULL_HANDLE;
    frame->descriptor = VK_NULL_HANDLE;
    memset(frame->slices, 0, sizeof(frame->slices));
}

void gdmf_sprites_frame_destroy(gdmf_sprites_frame* frame) {
    if (frame) {
        gdmf_sprites_vertex_buffer_destroy(&frame->vertices);
        memset(frame, 0, sizeof(*frame));
    }
}

bool gdmf_sprites_frame_prepare(const gdmf_sprites_frame_context* context, uint32_t image_index,
                                gdmf_sprites_frame* frame) {
    gdmf_sprites_frame_invalidate(frame);
    if (!frame || !context || !context->device || !context->memory_properties || !context->atlas ||
        context->atlas->device != context->device || !context->descriptors ||
        context->descriptors->device != context->device || !context->pipeline ||
        context->pipeline->device != context->device || !context->pipeline->pipeline ||
        !context->pipeline->layout || !isfinite(context->canvas_width) || !isfinite(context->canvas_height) ||
        context->canvas_width <= 0.0f || context->canvas_height <= 0.0f || !context->viewport.extent.width ||
        !context->viewport.extent.height) {
        return false;
    }
    VkDescriptorSet descriptor = gdmf_sprites_descriptor_get(context->descriptors, image_index);
    if (!descriptor) {
        return false;
    }
    int candidates[MAX_SPRITES], sorted[MAX_SPRITES];
    uint32_t starts[257] = {0}, sprite_count = 0;
    gdmf_sprite_vertex quad[6];
    for (int sprite_index = 0; sprite_index < MAX_SPRITES; ++sprite_index) {
        const Sprite* sprite = gdmf_sprites_state_get(sprite_index);
        if (!sprite->enabled || !sprite->visible || !gdmf_sprites_bitmap_data(sprite->bitmapID) ||
            !gdmf_sprites_atlas_layer_ready(context->atlas, sprite->bitmapID) ||
            !gdmf_sprites_frame_quad(sprite, context->canvas_width, context->canvas_height, quad)) {
            continue;
        }
        candidates[sprite_count++] = sprite_index;
        ++starts[sprite->priority + 1];
    }
    for (unsigned priority = 0; priority < 256; ++priority) {
        starts[priority + 1] += starts[priority];
    }
    for (uint32_t index = 0; index < sprite_count; ++index) {
        int sprite_index = candidates[index];
        sorted[starts[gdmf_sprites_state_get(sprite_index)->priority]++] = sprite_index;
    }
    if (sprite_count && !gdmf_sprites_vertex_buffer_ensure(context->device, context->memory_properties,
                                                           sprite_count * 6, &frame->vertices)) {
        return false;
    }
    for (uint32_t index = 0; index < sprite_count; ++index) {
        const Sprite* sprite = gdmf_sprites_state_get(sorted[index]);
        // State is unchanged since the validation pass above.
        if (!gdmf_sprites_frame_quad(sprite, context->canvas_width, context->canvas_height, quad)) {
            gdmf_sprites_frame_invalidate(frame);
            return false;
        }
        memcpy(frame->vertices.mapped + index * 6, quad, sizeof(quad));
        gdmf_sprite_slice* slice = &frame->slices[sprite->priority];
        if (!slice->vertex_count) {
            slice->first_vertex = index * 6;
        }
        slice->vertex_count += 6;
    }
    frame->pipeline = context->pipeline->pipeline;
    frame->layout = context->pipeline->layout;
    frame->descriptor = descriptor;
    frame->viewport = context->viewport;
    frame->image_index = image_index;
    frame->sprite_count = sprite_count;
    frame->prepared = true;
    return true;
}
