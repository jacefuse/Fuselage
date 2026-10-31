#ifndef GDMF_VULKAN_RENDERER_PRIVATE_H
#define GDMF_VULKAN_RENDERER_PRIVATE_H

// Shared only by coordinator lifecycle and frame implementation files.
// SEE: (GDMF_VULKAN.md, Renderer coordination).
#include "../../gdmf.h"
#include "gdmf_vulkan_internal.h"
#include "device/gdmf_vulkan_instance.h"
#include "device/gdmf_vulkan_device.h"
#include "presentation/gdmf_vulkan_swapchain.h"
#include "presentation/gdmf_vulkan_render_pass.h"
#include "resources/gdmf_vulkan_commands.h"
#include "palettes/gdmf_vulkan_palettes.h"
#include "composition/gdmf_vulkan_composition.h"
#include "capture/gdmf_vulkan_capture_requests.h"
#include "capture/gdmf_vulkan_capture_readback.h"

typedef struct {
    bool locks_ready;
    bool initialized;
    bool frames_ready;
    bool stopped;
    bool device_lost;
    bool submit_unresolved;
    bool needs_recreation;
    bool acquired;
    bool acquire_wait_pending;
    bool completion_observed;
    bool frame_pending;
    bool vsync_enabled;
    uint32_t image_index;
    uint32_t mask_fallbacks;
    Color clear_color;
    VkClearColorValue frame_clear_color;
    VkExtent2D frame_canvas;
    VkRect2D viewport;
    VkSurfaceKHR surface;
    VkRenderPass render_pass;
    VkFormat render_pass_format;
    uint8_t* submitted_images;
    FuselageMutex queue_lock;
    FuselageMutex upload_lock;
    FuselageMutex retirement_lock;
    FuselageMutex capture_lock;
    gdmf_vulkan_physical_device_selection selection;
    gdmf_vulkan_logical_device logical;
    gdmf_vulkan_command_pools pools;
    gdmf_vulkan_swapchain swapchain;
    gdmf_vulkan_framebuffers framebuffers;
    gdmf_vulkan_frame_commands frames;
    gdmf_vulkan_palettes palettes;
    gdmf_vulkan_staging staging;
    gdmf_vulkan_uploads uploads;
    gdmf_vulkan_retirement_queue retirement;
    gdmf_vulkan_masks masks;
    gdmf_vulkan_mask_targets targets;
    gdmf_vulkan_composition_plan composition;
    gdmf_vulkan_capture_requests requests;
    gdmf_vulkan_capture_readback readback;
} gdmf_vulkan_renderer;

bool gdmf_renderer_quiesce(gdmf_vulkan_renderer* renderer);
int gdmf_renderer_recreate(gdmf_vulkan_renderer* renderer);
void gdmf_renderer_ensure_masks(gdmf_vulkan_renderer* renderer);
bool gdmf_renderer_acquire(gdmf_vulkan_renderer* renderer);
bool gdmf_renderer_prepare(gdmf_vulkan_renderer* renderer);
bool gdmf_renderer_submit(gdmf_vulkan_renderer* renderer);
void gdmf_renderer_note_error(gdmf_vulkan_renderer* renderer, const char* operation, VkResult result);

#endif // GDMF_VULKAN_RENDERER_PRIVATE_H
