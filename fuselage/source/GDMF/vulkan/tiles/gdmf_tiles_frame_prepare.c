// Slot cache and detached draw values. SEE: (GDMF_TILES.md, Prepared frames)
#include "gdmf_tiles_frame.h"
#include <limits.h>
#include <math.h>
#include <string.h>

void gdmf_tiles_frame_invalidate(gdmf_tiles_frame* frame) {
    if (frame) {
        memset(&frame->draw, 0, sizeof(frame->draw));
        frame->geometry_valid = false;
    }
}
void gdmf_tiles_frame_destroy(gdmf_tiles_frame* frame) {
    if (frame) {
        gdmf_tiles_vertex_buffer_destroy(&frame->vertices);
        memset(frame, 0, sizeof(*frame));
    }
}
static bool gdmf_tiles_frame_scissor(const TileMap* map, const gdmf_tiles_frame_context* context,
                                     VkRect2D* output) {
    VkRect2D viewport = context->viewport;
    int64_t left = viewport.offset.x, top = viewport.offset.y;
    int64_t right = left + viewport.extent.width, bottom = top + viewport.extent.height;
    if (left < 0 || top < 0 || right > INT32_MAX || bottom > INT32_MAX || !isfinite(context->canvas_width) ||
        !isfinite(context->canvas_height) || context->canvas_width <= 0 || context->canvas_height <= 0) {
        return false;
    }
    float scale_x = (float)viewport.extent.width / context->canvas_width;
    float scale_y = (float)viewport.extent.height / context->canvas_height;
    double converted[4] = {
        trunc((double)(map->viewportX * scale_x)), trunc((double)(map->viewportY * scale_y)),
        trunc((double)(map->viewportWidth * scale_x)), trunc((double)(map->viewportHeight * scale_y))};
    for (unsigned index = 0; index < 4; ++index) {
        if (!isfinite(converted[index]) || converted[index] < INT32_MIN || converted[index] > INT32_MAX) {
            return false;
        }
    }
    int64_t layer_left = left + (int64_t)converted[0], layer_top = top + (int64_t)converted[1];
    int64_t layer_right = layer_left + (int64_t)converted[2],
            layer_bottom = layer_top + (int64_t)converted[3];
    if (layer_left < left) {
        layer_left = left;
    }
    if (layer_left > right) {
        layer_left = right;
    }
    if (layer_top < top) {
        layer_top = top;
    }
    if (layer_top > bottom) {
        layer_top = bottom;
    }
    if (layer_right < left) {
        layer_right = left;
    }
    if (layer_right > right) {
        layer_right = right;
    }
    if (layer_bottom < top) {
        layer_bottom = top;
    }
    if (layer_bottom > bottom) {
        layer_bottom = bottom;
    }
    *output = (VkRect2D){{(int32_t)layer_left, (int32_t)layer_top},
                         {(uint32_t)(layer_right > layer_left ? layer_right - layer_left : 0),
                          (uint32_t)(layer_bottom > layer_top ? layer_bottom - layer_top : 0)}};
    return true;
}
bool gdmf_tiles_frame_prepare(const gdmf_tiles_frame_context* context, uint32_t image_index,
                              gdmf_tiles_frame* frame) {
    if (!frame) {
        return false;
    }
    uint32_t previous_image = frame->draw.image_index;
    memset(&frame->draw, 0, sizeof(frame->draw));
    if (!context || !context->atlas || !context->device || !context->memory_properties ||
        context->atlas->device != context->device || !context->descriptors ||
        context->descriptors->device != context->device || !context->pipeline ||
        context->pipeline->device != context->device || !context->pipeline->pipeline ||
        !context->pipeline->layout || !context->viewport.extent.width || !context->viewport.extent.height) {
        goto fail;
    }
    const TileMap* map = gdmf_tiles_state_get(context->atlas->layer);
    if (!map || !map->initialized || map->tileWidth != context->atlas->width ||
        map->tileHeight != context->atlas->height ||
        gdmf_tiles_bitmap_count(context->atlas->layer) != context->atlas->tile_count) {
        goto fail;
    }
    VkDescriptorSet descriptor = gdmf_tiles_descriptor_get(context->descriptors, image_index);
    VkRect2D scissor;
    if (!descriptor || !gdmf_tiles_frame_scissor(map, context, &scissor)) {
        goto fail;
    }
    bool rebuild = context->rebuild || !frame->geometry_valid || previous_image != image_index ||
                   frame->layer != context->atlas->layer || frame->geometry_atlas != context->atlas->image ||
                   frame->canvas_width != context->canvas_width ||
                   frame->canvas_height != context->canvas_height;
    if (rebuild) {
        frame->geometry_valid = false;
    }
    if (!map->visible || !map->enabled || !scissor.extent.width || !scissor.extent.height) {
        return true;
    }
    if (rebuild) {
        gdmf_tiles_geometry_context geometry = {map, context->atlas, context->canvas_width,
                                                context->canvas_height};
        gdmf_tiles_geometry_plan plan;
        if (!gdmf_tiles_geometry_plan_create(&geometry, &plan) ||
            !gdmf_tiles_vertex_buffer_ensure(context->device, context->memory_properties,
                                             plan.required_vertices, &frame->vertices) ||
            !gdmf_tiles_geometry_build(&geometry, &plan, frame->vertices.mapped, frame->vertices.capacity,
                                       &frame->vertex_count)) {
            goto fail;
        }
        frame->geometry_valid = true;
        frame->canvas_width = context->canvas_width;
        frame->canvas_height = context->canvas_height;
        frame->geometry_atlas = context->atlas->image;
        frame->layer = context->atlas->layer;
    }
    frame->draw = (gdmf_tiles_draw){.vertices = frame->vertices.buffer,
                                    .descriptor = descriptor,
                                    .pipeline = context->pipeline->pipeline,
                                    .layout = context->pipeline->layout,
                                    .viewport = context->viewport,
                                    .scissor = scissor,
                                    .vertex_count = frame->vertex_count,
                                    .image_index = image_index,
                                    .priority = map->priority,
                                    .drawable = frame->vertex_count != 0};
    return true;
fail:
    gdmf_tiles_frame_invalidate(frame);
    return false;
}
