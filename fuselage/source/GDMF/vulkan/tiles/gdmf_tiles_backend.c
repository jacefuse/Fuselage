// Renderer ownership. SEE: (GDMF_TILES.md, Backend lifecycle)
#include "gdmf_tiles_backend_internal.h"
#include "gdmf_tiles_frame.h"
#include "../renderer/gdmf_vulkan_internal.h"
#include "../../tiles/gdmf_tiles_state_private.h"
#include "../../window/gdmf_canvas.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    gdmf_tiles_atlas atlas;
    gdmf_tiles_descriptors descriptors;
    gdmf_tiles_frame* frames;
    bool* dirty;
    bool retiring;
} gdmf_tiles_backend_layer;

typedef struct {
    VkDevice device;
    VkRenderPass render_pass;
    gdmf_tiles_descriptor_layout descriptor_layout;
    gdmf_tiles_pipeline pipeline;
    VkBuffer* palette_buffers;
    uint32_t frame_count;
    bool quiesced;
    gdmf_tiles_backend_layer layers[MAX_TILE_LAYERS];
    gdmf_tiles_draw draws[MAX_TILE_LAYERS];
} gdmf_tiles_backend_state;

static gdmf_tiles_backend_state gdmf_tiles_backend;
static atomic_bool gdmf_tiles_backend_blocked;
static bool gdmf_tiles_atlas_preview[MAX_TILE_LAYERS];

bool gdmf_tiles_backend_healthy(void) {
    return !atomic_load(&gdmf_tiles_backend_blocked);
}
bool gdmf_tiles_backend_has_resources(void) {
    return gdmf_tiles_backend.device != VK_NULL_HANDLE;
}
static bool gdmf_tiles_backend_block(void) {
    // A release caller can run while the render thread reads the detached batch.
    // Stop future work without mutating that batch or destroying its handles.
    if (!atomic_exchange(&gdmf_tiles_backend_blocked, true)) {
        fprintf(stderr, "[Tiles] Cleanup or lifecycle mismatch; resources retained\n");
    }
    return false;
}
void gdmf_tiles_backend_mark_dirty(uint8_t layer) {
    if (layer >= MAX_TILE_LAYERS) {
        return;
    }
    gdmf_tiles_backend_layer* resources = &gdmf_tiles_backend.layers[layer];
    if (resources->dirty) {
        for (uint32_t index = 0; index < gdmf_tiles_backend.frame_count; ++index) {
            resources->dirty[index] = true;
        }
    }
}
void gdmf_tiles_backend_note_quiesced(void) {
    gdmf_tiles_backend.quiesced = true;
    memset(gdmf_tiles_backend.draws, 0, sizeof(gdmf_tiles_backend.draws));
}
static void gdmf_tiles_backend_release_layer_frames(gdmf_tiles_backend_layer* resources) {
    if (resources->frames) {
        for (uint32_t index = 0; index < gdmf_tiles_backend.frame_count; ++index) {
            gdmf_tiles_frame_destroy(&resources->frames[index]);
        }
    }
    free(resources->frames);
    free(resources->dirty);
    resources->frames = NULL;
    resources->dirty = NULL;
    gdmf_tiles_descriptors_destroy(&resources->descriptors);
}
static void gdmf_tiles_backend_release_frames(void) {
    for (uint8_t layer = 0; layer < MAX_TILE_LAYERS; ++layer) {
        gdmf_tiles_backend_release_layer_frames(&gdmf_tiles_backend.layers[layer]);
    }
    free(gdmf_tiles_backend.palette_buffers);
    gdmf_tiles_backend.palette_buffers = NULL;
    gdmf_tiles_backend.frame_count = 0;
    gdmf_tiles_backend.render_pass = VK_NULL_HANDLE;
    gdmf_tiles_pipeline_destroy(&gdmf_tiles_backend.pipeline);
    memset(gdmf_tiles_backend.draws, 0, sizeof(gdmf_tiles_backend.draws));
}
bool gdmf_tiles_backend_shutdown(void) {
    if (gdmf_tiles_backend.device && !gdmf_tiles_backend.quiesced) {
        return gdmf_tiles_backend_block();
    }
    gdmf_tiles_backend_release_frames();
    for (uint8_t layer = 0; layer < MAX_TILE_LAYERS; ++layer) {
        gdmf_tiles_atlas_destroy(&gdmf_tiles_backend.layers[layer].atlas);
        gdmf_tiles_bitmaps_clear(layer);
        gdmf_tiles_state_clear(layer);
    }
    gdmf_tiles_descriptor_layout_destroy(&gdmf_tiles_backend.descriptor_layout);
    memset(&gdmf_tiles_backend, 0, sizeof(gdmf_tiles_backend));
    memset(gdmf_tiles_atlas_preview, 0, sizeof(gdmf_tiles_atlas_preview));
    atomic_store(&gdmf_tiles_backend_blocked, false);
    return true;
}
void ShutdownTiles(void) {
    (void)gdmf_tiles_backend_shutdown();
}
void gdmf_tiles_on_swapchain_recreated(void) {
    if (!gdmf_tiles_backend.device) {
        return;
    }
    if (!gdmf_tiles_backend.quiesced || !gdmf_tiles_backend_healthy()) {
        gdmf_tiles_backend_block();
        return;
    }
    // Before replacing borrowed palettes or the render pass; atlases survive.
    gdmf_tiles_backend_release_frames();
}

bool ReleaseTileLayer(uint8_t layer) {
    if (layer >= MAX_TILE_LAYERS) {
        return false;
    }
    gdmf_tiles_backend_layer* resources = &gdmf_tiles_backend.layers[layer];
    if (!gdmf_tiles_layer_valid(layer) && !resources->retiring) {
        return false;
    }
    resources->retiring = true;
    if (gdmf_tiles_backend.quiesced) {
        gdmf_tiles_backend_release_layer_frames(resources);
        gdmf_tiles_atlas_destroy(&resources->atlas);
    } else {
        gdmf_vulkan_retirement_queue* queue = gdmf_get_retirement_queue();
        if (!queue || queue->device != gdmf_tiles_backend.device || !queue->lock) {
            return gdmf_tiles_backend_block();
        }
        // Only clear an owner after the queue accepts it. A failed transfer keeps
        // the remaining owners for checked shutdown after renderer quiescence.
        if (resources->descriptors.pool) {
            if (!gdmf_defer_destroy_descriptor_pool(queue, resources->descriptors.pool)) {
                return gdmf_tiles_backend_block();
            }
            resources->descriptors.pool = VK_NULL_HANDLE;
            free(resources->descriptors.sets);
            memset(&resources->descriptors, 0, sizeof(resources->descriptors));
        }
        if (resources->frames) {
            for (uint32_t index = 0; index < gdmf_tiles_backend.frame_count; ++index) {
                gdmf_tiles_vertex_buffer* vertices = &resources->frames[index].vertices;
                if (vertices->buffer || vertices->memory) {
                    if (!gdmf_defer_destroy_buffer(queue, vertices->buffer, vertices->memory)) {
                        return gdmf_tiles_backend_block();
                    }
                    // VkFreeMemory unmaps when the retirement queue frees it.
                    memset(vertices, 0, sizeof(*vertices));
                }
            }
        }
        if (resources->atlas.image || resources->atlas.view || resources->atlas.memory) {
            if (!gdmf_defer_destroy_image(queue, resources->atlas.image, resources->atlas.view,
                                          resources->atlas.memory)) {
                return gdmf_tiles_backend_block();
            }
            resources->atlas.image = VK_NULL_HANDLE;
            resources->atlas.view = VK_NULL_HANDLE;
            resources->atlas.memory = VK_NULL_HANDLE;
        }
        if (resources->atlas.sampler && !gdmf_defer_destroy_sampler(queue, resources->atlas.sampler)) {
            return gdmf_tiles_backend_block();
        }
        memset(&resources->atlas, 0, sizeof(resources->atlas));
        gdmf_tiles_backend_release_layer_frames(resources);
    }
    memset(resources, 0, sizeof(*resources));
    gdmf_tiles_bitmaps_clear(layer);
    gdmf_tiles_state_clear(layer);
    gdmf_tiles_atlas_preview[layer] = false;
    // Detached draws retain queued handles through the pending frame.
    return true;
}
bool InitTileLayer(uint8_t layer, uint16_t map_width, uint16_t map_height, uint16_t tile_width,
                   uint16_t tile_height, uint16_t tile_count, float scale) {
    if (layer >= MAX_TILE_LAYERS || !map_width || map_width > MAX_MAP_WIDTH || !map_height ||
        map_height > MAX_MAP_HEIGHT || !tile_width || tile_width > MAX_TILE_WIDTH || !tile_height ||
        tile_height > MAX_TILE_HEIGHT || !tile_count || tile_count > MAX_TILES || scale <= 0.0f ||
        !gdmf_tiles_backend_healthy()) {
        return false;
    }
    VkDevice device = gdmf_get_device();
    if (gdmf_tiles_backend.device && gdmf_tiles_backend.device != device) {
        return gdmf_tiles_backend_block();
    }
    if (!device || !gdmf_get_memory_properties()) {
        return false;
    }
    if (gdmf_tiles_layer_valid(layer) && !ReleaseTileLayer(layer)) {
        return false;
    }
    gdmf_tiles_atlas_config config = {.device = device,
                                      .physical_device = gdmf_get_physical_device(),
                                      .memory_properties = gdmf_get_memory_properties(),
                                      .layer = layer,
                                      .width = tile_width,
                                      .height = tile_height,
                                      .tile_count = tile_count};
    gdmf_tiles_backend_layer* resources = &gdmf_tiles_backend.layers[layer];
    if (!gdmf_tiles_atlas_create(&config, &resources->atlas)) {
        return false;
    }
    if (!gdmf_tiles_backend.device) {
        gdmf_tiles_backend.device = device;
        gdmf_tiles_backend.quiesced = true;
    }
    if (!gdmf_tiles_state_initialize(layer, map_width, map_height, tile_width, tile_height, scale,
                                     GDMF_GetCanvasWidth(), GDMF_GetCanvasHeight()) ||
        !gdmf_tiles_bitmaps_reset(layer, tile_count)) {
        gdmf_tiles_atlas_destroy(&resources->atlas);
        gdmf_tiles_bitmaps_clear(layer);
        gdmf_tiles_state_clear(layer);
        return false;
    }
    return true;
}

static bool gdmf_tiles_backend_setup(void) {
    VkDevice device = gdmf_get_device();
    VkRenderPass render_pass = gdmf_get_render_pass();
    uint32_t image_count = gdmf_get_swapchain_image_count();
    if ((gdmf_tiles_backend.device && gdmf_tiles_backend.device != device) ||
        (gdmf_tiles_backend.render_pass && gdmf_tiles_backend.render_pass != render_pass) ||
        (gdmf_tiles_backend.frame_count && gdmf_tiles_backend.frame_count != image_count)) {
        return gdmf_tiles_backend_block();
    }
    if (!device || !gdmf_tiles_backend.device || !render_pass || !image_count) {
        return false;
    }
    if (!gdmf_tiles_backend.palette_buffers) {
        if ((size_t)image_count > SIZE_MAX / sizeof(VkBuffer)) {
            return false;
        }
        gdmf_tiles_backend.palette_buffers = calloc(image_count, sizeof(VkBuffer));
        if (!gdmf_tiles_backend.palette_buffers) {
            return false;
        }
        gdmf_tiles_backend.frame_count = image_count;
        gdmf_tiles_backend.render_pass = render_pass;
    }
    for (uint32_t index = 0; index < image_count; ++index) {
        VkBuffer palette = gdmf_get_palette_buffer(index);
        if (gdmf_tiles_backend.palette_buffers[index] &&
            gdmf_tiles_backend.palette_buffers[index] != palette) {
            return gdmf_tiles_backend_block();
        }
        if (!palette) {
            return false;
        }
        gdmf_tiles_backend.palette_buffers[index] = palette;
    }
    if (!gdmf_tiles_backend.descriptor_layout.device &&
        !gdmf_tiles_descriptor_layout_create(device, &gdmf_tiles_backend.descriptor_layout)) {
        return false;
    }
    if (!gdmf_tiles_backend.pipeline.device &&
        !gdmf_tiles_pipeline_create(device, render_pass, gdmf_get_pipeline_cache(),
                                    gdmf_tiles_backend.descriptor_layout.layout,
                                    &gdmf_tiles_backend.pipeline)) {
        return false;
    }
    return true;
}
static bool gdmf_tiles_backend_setup_layer(gdmf_tiles_backend_layer* resources) {
    uint32_t image_count = gdmf_tiles_backend.frame_count;
    if (!resources->frames) {
        if ((size_t)image_count > SIZE_MAX / sizeof(*resources->frames)) {
            return false;
        }
        gdmf_tiles_frame* frames = calloc(image_count, sizeof(*frames));
        bool* dirty = calloc(image_count, sizeof(*dirty));
        if (!frames || !dirty) {
            free(frames);
            free(dirty);
            return false;
        }
        for (uint32_t index = 0; index < image_count; ++index) {
            dirty[index] = true;
        }
        resources->frames = frames;
        resources->dirty = dirty;
    }
    return resources->descriptors.device ||
           gdmf_tiles_descriptors_create(&gdmf_tiles_backend.descriptor_layout, &resources->atlas,
                                         gdmf_tiles_backend.palette_buffers, image_count,
                                         &resources->descriptors);
}
void gdmf_tiles_prepare(VkCommandBuffer command_buffer, uint32_t image_index) {
    memset(gdmf_tiles_backend.draws, 0, sizeof(gdmf_tiles_backend.draws));
    if (!gdmf_tiles_backend_healthy() || !command_buffer) {
        return;
    }
    bool graphics_ready = gdmf_tiles_backend_setup();
    if (!gdmf_tiles_backend_healthy() || image_index >= gdmf_get_swapchain_image_count()) {
        return;
    }
    float canvas_width = (float)GDMF_GetCanvasWidth(), canvas_height = (float)GDMF_GetCanvasHeight();
    VkRect2D viewport = gdmf_get_render_viewport_rect();
    for (uint8_t layer = 0; layer < MAX_TILE_LAYERS; ++layer) {
        gdmf_tiles_backend_layer* resources = &gdmf_tiles_backend.layers[layer];
        if (!gdmf_tiles_layer_valid(layer) || resources->retiring || !resources->atlas.device) {
            continue;
        }
        gdmf_tiles_backend.quiesced = false;
        // Uploads remain possible when descriptor/pipeline allocation needs retry.
        gdmf_tiles_atlas_upload(&resources->atlas, command_buffer, gdmf_get_frame_staging(), image_index);
        if (!graphics_ready || !gdmf_tiles_backend_setup_layer(resources)) {
            continue;
        }
        gdmf_tiles_frame_context context = {.device = gdmf_tiles_backend.device,
                                            .memory_properties = gdmf_get_memory_properties(),
                                            .atlas = &resources->atlas,
                                            .descriptors = &resources->descriptors,
                                            .pipeline = &gdmf_tiles_backend.pipeline,
                                            .canvas_width = canvas_width,
                                            .canvas_height = canvas_height,
                                            .viewport = viewport,
                                            .rebuild = resources->dirty[image_index]};
        if (gdmf_tiles_frame_prepare(&context, image_index, &resources->frames[image_index])) {
            resources->dirty[image_index] = false;
            gdmf_tiles_backend.draws[layer] = resources->frames[image_index].draw;
        }
    }
}
void gdmf_tiles_record_priority(VkCommandBuffer command_buffer, uint32_t image_index, uint8_t priority) {
    if (gdmf_tiles_backend_healthy()) {
        gdmf_tiles_draw_record_priority(gdmf_tiles_backend.draws, command_buffer, image_index, priority);
    }
}
void ToggleTileAtlasView(uint8_t layer) {
    if (layer < MAX_TILE_LAYERS) {
        gdmf_tiles_atlas_preview[layer] = !gdmf_tiles_atlas_preview[layer];
    }
}
bool GetTileAtlasViewActive(uint8_t layer) {
    return layer < MAX_TILE_LAYERS && gdmf_tiles_atlas_preview[layer];
}
