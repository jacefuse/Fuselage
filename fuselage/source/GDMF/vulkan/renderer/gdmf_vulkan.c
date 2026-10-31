// Renderer lifecycle, owned resources, and borrowed subsystem services.
// SEE: (GDMF_VULKAN.md, Renderer coordination).
#include "gdmf_vulkan_renderer_private.h"
#include "../gdmf_vulkan.h"
#include "composition/gdmf_vulkan_geometry.h"
#include "../pixies/gdmf_pixies_backend_internal.h"
#include "../sprites/gdmf_sprites_backend_internal.h"
#include "../tiles/gdmf_tiles_backend_internal.h"
#include "../../textlayer/gdmf_textlayer_backend_internal.h"
#include "../../compute/gdmf_compute.h"
#include "../../shaders/mask_composite_vert.h"
#include "../../shaders/mask_composite_frag.h"
#include <stdlib.h>

static gdmf_vulkan_renderer gdmf_renderer = {
    .vsync_enabled = true, .clear_color = {0, 0, 0, 255}
};

void gdmf_renderer_note_error(gdmf_vulkan_renderer* renderer, const char* operation, VkResult result) {
    printf("[Vulkan] %s failed: %d\n", operation, result);
    renderer->needs_recreation = true;
    if (result == VK_ERROR_DEVICE_LOST) {
        renderer->device_lost = true;
        renderer->stopped = true;
    }
}

static void gdmf_renderer_release_frames(gdmf_vulkan_renderer* renderer) {
    gdmf_vulkan_mask_targets_destroy(&renderer->targets);
    gdmf_vulkan_masks_destroy(&renderer->masks);
    gdmf_vulkan_palettes_destroy(renderer->logical.device, &renderer->palettes);
    gdmf_frame_staging_destroy(&renderer->staging);
    gdmf_vulkan_frame_commands_destroy(renderer->logical.device, &renderer->frames);
    gdmf_vulkan_framebuffers_destroy(renderer->logical.device, &renderer->framebuffers);
    gdmf_vulkan_swapchain_destroy(renderer->logical.device, &renderer->swapchain);
    free(renderer->submitted_images);
    renderer->submitted_images = NULL;
    renderer->frames_ready = false;
}

static int gdmf_renderer_build_frames(gdmf_vulkan_renderer* renderer) {
    int width = GDMF_GetWidth(), height = GDMF_GetHeight();
    if (width <= 0 || height <= 0) { return -2; }
    VkDevice device = renderer->logical.device;
    if (gdmf_vulkan_swapchain_create(&renderer->selection, device, renderer->surface,
        (VkExtent2D){(uint32_t)width, (uint32_t)height}, renderer->vsync_enabled, &renderer->swapchain) != 0) { goto fail; }
    if (renderer->render_pass == VK_NULL_HANDLE || renderer->render_pass_format != renderer->swapchain.format) {
        gdmf_vulkan_render_pass_destroy(device, &renderer->render_pass);
        renderer->render_pass_format = VK_FORMAT_UNDEFINED;
        if (gdmf_vulkan_render_pass_create(device, renderer->swapchain.format, &renderer->render_pass) != 0) { goto fail; }
        renderer->render_pass_format = renderer->swapchain.format;
    }
    if (gdmf_vulkan_framebuffers_create(device, renderer->render_pass, &renderer->swapchain, &renderer->framebuffers) != 0) { goto fail; }
    uint32_t image_count = renderer->swapchain.image_count;
    if (gdmf_vulkan_frame_commands_create(device, renderer->pools.render_pool, image_count, &renderer->frames) != 0) { goto fail; }
    if (gdmf_frame_staging_create(device, &renderer->selection.memory_properties, image_count, &renderer->staging) != 0) { goto fail; }
    if (gdmf_vulkan_palettes_create(device, &renderer->selection.memory_properties, image_count, &renderer->palettes) != 0) { goto fail; }
    renderer->submitted_images = calloc(image_count, sizeof(*renderer->submitted_images));
    if (!renderer->submitted_images) { goto fail; }
    int numerator = GDMF_GetAspectRatioNum(), denominator = GDMF_GetAspectRatioDen();
    if (numerator > 0 && denominator > 0) {
        gdmf_vulkan_viewport_rect(renderer->swapchain.extent, (uint32_t)numerator,
            (uint32_t)denominator, &renderer->viewport);
    }
    renderer->frames_ready = true;
    return 0;
fail:
    gdmf_renderer_release_frames(renderer);
    return -1;
}

bool gdmf_renderer_quiesce(gdmf_vulkan_renderer* renderer) {
    if (renderer->logical.device == VK_NULL_HANDLE) {
        if (gdmf_pixies_backend_has_resources() || gdmf_sprites_backend_has_resources() || gdmf_tiles_backend_has_resources() || gdmf_textlayer_backend_has_resources()) { return false; }
        gdmf_pixies_backend_note_quiesced();
        gdmf_sprites_backend_note_quiesced();
        gdmf_tiles_backend_note_quiesced();
        gdmf_textlayer_backend_note_quiesced();
        return true;
    }
    if (renderer->submit_unresolved) {
        printf("[Vulkan] Frame submission recovery is required before teardown\n");
        return false;
    }
    fuselage_mutex_lock(&renderer->upload_lock);
    fuselage_mutex_lock(&renderer->queue_lock);
    VkResult result = VK_SUCCESS;
    if (renderer->acquire_wait_pending) {
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkSubmitInfo drain = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .waitSemaphoreCount = 1,
            .pWaitSemaphores = &renderer->frames.acquire_semaphores[renderer->image_index], .pWaitDstStageMask = &wait_stage};
        result = vkQueueSubmit(renderer->logical.graphics_queue, 1, &drain, VK_NULL_HANDLE);
        if (result == VK_SUCCESS) { renderer->acquire_wait_pending = false; }
        else { renderer->submit_unresolved = renderer->stopped = true; }
    }
    if (result == VK_SUCCESS) { result = vkDeviceWaitIdle(renderer->logical.device); }
    fuselage_mutex_unlock(&renderer->queue_lock);
    if (result == VK_SUCCESS && renderer->pools.render_pool != VK_NULL_HANDLE) {
        result = vkResetCommandPool(renderer->logical.device, renderer->pools.render_pool, 0);
    }
    fuselage_mutex_unlock(&renderer->upload_lock);
    if (result != VK_SUCCESS) {
        renderer->stopped = true;
        gdmf_renderer_note_error(renderer, "Renderer quiesce", result);
        return false;
    }
    renderer->acquired = renderer->frame_pending = renderer->completion_observed = false;
    gdmf_vulkan_uploads_release_completed(&renderer->uploads);
    gdmf_vulkan_capture_readback_release_completed(&renderer->readback);
    gdmf_vulkan_capture_request_finish(&renderer->requests);
    gdmf_pixies_backend_note_quiesced();
    gdmf_sprites_backend_note_quiesced();
    gdmf_tiles_backend_note_quiesced();
    gdmf_textlayer_backend_note_quiesced();
    return true;
}

int gdmf_renderer_recreate(gdmf_vulkan_renderer* renderer) {
    if (GDMF_GetWidth() <= 0 || GDMF_GetHeight() <= 0) { return -2; }
    if (!gdmf_renderer_quiesce(renderer)) { return -1; }
    if (!gdmf_defer_flush(&renderer->retirement)) { return -1; }
    // Sprite and Tile descriptor sets borrow the old per-image palette buffers.
    gdmf_sprites_on_swapchain_recreated();
    if (!gdmf_sprites_backend_healthy()) { renderer->stopped = true; return -1; }
    gdmf_tiles_on_swapchain_recreated();
    if (!gdmf_tiles_backend_healthy()) { renderer->stopped = true; return -1; }
    gdmf_textlayer_on_swapchain_recreated();
    if (!gdmf_textlayer_backend_healthy()) { renderer->stopped = true; return -1; }
    gdmf_renderer_release_frames(renderer);
    int result = gdmf_renderer_build_frames(renderer);
    if (result != 0) { return result; }
    if (gdmf_defer_queue_init(renderer->logical.device, &renderer->retirement_lock,
        renderer->swapchain.image_count, &renderer->retirement) != 0) { return -1; }
    gdmf_pixies_on_swapchain_recreated();
    if (!gdmf_pixies_backend_healthy()) { renderer->stopped = true; return -1; }
    gdmf_compute_on_swapchain_recreated();
    return 0;
}

void gdmf_renderer_ensure_masks(gdmf_vulkan_renderer* renderer) {
    if (renderer->masks.pipeline != VK_NULL_HANDLE || !renderer->composition.mask_count) { return; }
    gdmf_vulkan_mask_config config = {
        .device = renderer->logical.device, .presentation_render_pass = renderer->render_pass,
        .pipeline_cache = renderer->logical.pipeline_cache, .format = renderer->swapchain.format,
        .image_count = renderer->swapchain.image_count, .masks_per_image = MAX_TILE_LAYERS,
        .vertex_shader = {gdmf_mask_composite_vertex_spirv, gdmf_mask_composite_vertex_spirv_byte_count},
        .fragment_shader = {gdmf_mask_composite_fragment_spirv, gdmf_mask_composite_fragment_spirv_byte_count}
    };
    if (gdmf_vulkan_masks_create(&config, &renderer->masks) != 0) { return; }
    gdmf_vulkan_mask_target_config targets = {
        .device = renderer->logical.device, .memory_properties = renderer->selection.memory_properties,
        .render_pass = renderer->masks.offscreen_render_pass, .format = renderer->swapchain.format,
        .extent = renderer->swapchain.extent, .image_count = renderer->swapchain.image_count,
        .masks_per_image = MAX_TILE_LAYERS
    };
    if (gdmf_vulkan_mask_targets_create(&targets, &renderer->targets) != 0) {
        gdmf_vulkan_masks_destroy(&renderer->masks);
    }
}

int gdmf_vulkan_init(void) {
    gdmf_vulkan_renderer* renderer = &gdmf_renderer;
    if (renderer->locks_ready || renderer->logical.device != VK_NULL_HANDLE ||
        gdmf_pixies_backend_has_resources() || gdmf_sprites_backend_has_resources() || gdmf_tiles_backend_has_resources() || gdmf_textlayer_backend_has_resources()) { return -1; }
    renderer->stopped = renderer->device_lost = renderer->needs_recreation = false;
    fuselage_mutex_init(&renderer->queue_lock); fuselage_mutex_init(&renderer->upload_lock);
    fuselage_mutex_init(&renderer->retirement_lock); fuselage_mutex_init(&renderer->capture_lock);
    renderer->locks_ready = true;
    gdmf_vulkan_capture_requests_init(&renderer->capture_lock, &renderer->requests);
    if (gdmf_vulkan_instance_create() != 0) { return -1; }
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (gdmf_create_platform_surface(gdmf_vulkan_instance_get(), &surface) != 0) { return -1; }
    renderer->surface = surface;
    if (gdmf_vulkan_select_physical_device(gdmf_vulkan_instance_get(), renderer->surface, &renderer->selection) != 0) { return -1; }
    if (gdmf_vulkan_device_create(&renderer->selection, &renderer->logical) != 0) { return -1; }
    renderer->uploads = (gdmf_vulkan_uploads){.device = renderer->logical.device,
        .graphics_queue = renderer->logical.graphics_queue, .upload_lock = &renderer->upload_lock, .queue_lock = &renderer->queue_lock};
    renderer->readback = (gdmf_vulkan_capture_readback){.device = renderer->logical.device,
        .memory_properties = renderer->selection.memory_properties, .uploads = &renderer->uploads};
    if (gdmf_vulkan_command_pools_create(renderer->logical.device, renderer->selection.graphics_family, &renderer->pools) != 0) { return -1; }
    renderer->uploads.upload_pool = renderer->pools.upload_pool;
    if (gdmf_renderer_build_frames(renderer) != 0) { return -1; }
    if (gdmf_defer_queue_init(renderer->logical.device, &renderer->retirement_lock,
        renderer->swapchain.image_count, &renderer->retirement) != 0) { return -1; }
    renderer->initialized = true;
    return 0;
}

bool gdmf_vulkan_quiesce(void) { return gdmf_renderer_quiesce(&gdmf_renderer); }

bool gdmf_vulkan_shutdown(void) {
    gdmf_vulkan_renderer* renderer = &gdmf_renderer;
    if (!gdmf_renderer_quiesce(renderer)) { return false; }
    if (!gdmf_textlayer_shutdown()) { renderer->stopped = true; return false; }
    ShutdownPixies();
    // Public shutdown is void; the checked service refuses retained logical IDs
    // or backend resources before their device and locks can be destroyed.
    if (!gdmf_pixies_backend_shutdown()) { renderer->stopped = true; return false; }
    ShutdownTiles();
    if (!gdmf_tiles_backend_shutdown()) { renderer->stopped = true; return false; }
    ShutdownSprites();
    if (!gdmf_sprites_backend_shutdown()) { renderer->stopped = true; return false; }
    if (renderer->retirement.lock && !gdmf_defer_flush(&renderer->retirement)) { renderer->stopped = true; return false; }
    if (renderer->logical.device != VK_NULL_HANDLE) { gdmf_compute_shutdown(); }
    if (renderer->retirement.lock && !gdmf_defer_flush(&renderer->retirement)) { renderer->stopped = true; return false; }
    gdmf_renderer_release_frames(renderer);
    gdmf_vulkan_command_pools_destroy(renderer->logical.device, &renderer->pools);
    gdmf_vulkan_render_pass_destroy(renderer->logical.device, &renderer->render_pass);
    gdmf_vulkan_device_destroy(&renderer->logical);
    if (renderer->surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(gdmf_vulkan_instance_get(), renderer->surface, NULL);
    }
    gdmf_vulkan_instance_destroy();
    if (renderer->locks_ready) {
        fuselage_mutex_destroy(&renderer->capture_lock); fuselage_mutex_destroy(&renderer->retirement_lock);
        fuselage_mutex_destroy(&renderer->upload_lock); fuselage_mutex_destroy(&renderer->queue_lock);
    }
    bool vsync = renderer->vsync_enabled; Color clear_color = renderer->clear_color;
    *renderer = (gdmf_vulkan_renderer){.vsync_enabled = vsync, .clear_color = clear_color};
    return true;
}

bool gdmf_vulkan_acquire_frame(void) { return gdmf_renderer_acquire(&gdmf_renderer); }
bool gdmf_vulkan_prepare_frame(void) { return gdmf_renderer_prepare(&gdmf_renderer); }
bool gdmf_vulkan_submit_frame(void) { return gdmf_renderer_submit(&gdmf_renderer); }
bool gdmf_vulkan_should_stop(void) {
    return gdmf_renderer.stopped || !gdmf_pixies_backend_healthy() || !gdmf_sprites_backend_healthy() || !gdmf_tiles_backend_healthy() || !gdmf_textlayer_backend_healthy();
}
bool gdmf_vulkan_has_resources(void) {
    return gdmf_renderer.locks_ready || gdmf_pixies_backend_has_resources() || gdmf_sprites_backend_has_resources() || gdmf_tiles_backend_has_resources() || gdmf_textlayer_backend_has_resources();
}
bool gdmf_vulkan_device_lost(void) { return gdmf_renderer.device_lost; }
void GDMF_SetClearColor(Color color) { gdmf_renderer.clear_color = color; }
Color GDMF_GetClearColor(void) { return gdmf_renderer.clear_color; }
void GDMF_SetVSync(bool enabled) {
    if (gdmf_renderer.vsync_enabled != enabled) {
        gdmf_renderer.vsync_enabled = enabled; gdmf_renderer.needs_recreation = true;
    }
}
bool GDMF_GetVSync(void) { return gdmf_renderer.vsync_enabled; }
bool GDMF_CaptureFrame(const char* path) {
    return gdmf_renderer.initialized && !gdmf_renderer.stopped &&
        gdmf_vulkan_capture_request_submit(&gdmf_renderer.requests, path);
}
bool GDMF_CapturePending(void) {
    return gdmf_renderer.initialized && gdmf_vulkan_capture_request_pending(&gdmf_renderer.requests);
}
bool GDMF_GetDeviceInfo(const char** name, const char** type, int* score) {
    if (gdmf_renderer.selection.device == VK_NULL_HANDLE) { return false; }
    if (name) { *name = gdmf_renderer.selection.properties.deviceName; }
    if (type) { *type = gdmf_vulkan_physical_device_type_name(gdmf_renderer.selection.properties.deviceType); }
    if (score) { *score = gdmf_renderer.selection.score; }
    return true;
}
VkDevice gdmf_get_device(void) { return gdmf_renderer.logical.device; }
VkPhysicalDevice gdmf_get_physical_device(void) { return gdmf_renderer.selection.device; }
VkQueue gdmf_get_graphics_queue(void) { return gdmf_renderer.logical.graphics_queue; }
VkCommandPool gdmf_get_command_pool(void) { return gdmf_renderer.pools.render_pool; }
VkRenderPass gdmf_get_render_pass(void) { return gdmf_renderer.render_pass; }
VkPipelineCache gdmf_get_pipeline_cache(void) { return gdmf_renderer.logical.pipeline_cache; }
VkExtent2D gdmf_get_swapchain_extent(void) { return gdmf_renderer.swapchain.extent; }
uint32_t gdmf_get_swapchain_image_count(void) { return gdmf_renderer.swapchain.image_count; }
VkRect2D gdmf_get_render_viewport_rect(void) { return gdmf_renderer.viewport; }
VkBuffer gdmf_get_palette_buffer(uint32_t image_index) { return gdmf_vulkan_palettes_get_buffer(&gdmf_renderer.palettes, image_index); }
const VkPhysicalDeviceMemoryProperties* gdmf_get_memory_properties(void) { return &gdmf_renderer.selection.memory_properties; }
gdmf_vulkan_staging* gdmf_get_frame_staging(void) { return &gdmf_renderer.staging; }
gdmf_vulkan_uploads* gdmf_get_upload_context(void) { return &gdmf_renderer.uploads; }
gdmf_vulkan_retirement_queue* gdmf_get_retirement_queue(void) { return &gdmf_renderer.retirement; }
VkResult gdmf_device_wait_idle(void) {
    if (gdmf_renderer.logical.device == VK_NULL_HANDLE) { return VK_SUCCESS; }
    fuselage_mutex_lock(&gdmf_renderer.queue_lock);
    VkResult result = vkDeviceWaitIdle(gdmf_renderer.logical.device);
    fuselage_mutex_unlock(&gdmf_renderer.queue_lock);
    return result;
}
