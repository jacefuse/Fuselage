#ifndef GDMF_VULKAN_INTERNAL_H
#define GDMF_VULKAN_INTERNAL_H

// Internal Vulkan services used by GDMF layer implementations.
// SEE: (GDMF_VULKAN.md, Internal interfaces).
#include <vulkan/vulkan.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "../surface/gdmf_vulkan_surface.h"
#include "composition/gdmf_vulkan_layers.h"
#include "resources/gdmf_vulkan_memory.h"
#include "palettes/gdmf_vulkan_palettes.h"
#include "resources/gdmf_vulkan_staging.h"
#include "resources/gdmf_vulkan_uploads.h"
#include "resources/gdmf_vulkan_retirement.h"

#define GDMF_VULKAN_INTERNAL_VERSION "0.5.2026102701 ENTERON"

// Evaluate once; log a non-success VkResult and jump to the caller's fail label.
#define GDMF_VULKAN_CHECK(expression)                                               \
    do {                                                                            \
        VkResult gdmf_vulkan_result = (expression);                                 \
        if (gdmf_vulkan_result != VK_SUCCESS) {                                     \
            printf("[Vulkan] %s failed: %d\n", #expression, gdmf_vulkan_result);    \
            goto fail;                                                              \
        }                                                                           \
    } while (0)

// Log a non-success result and continue. This performs no recovery or cleanup.
#define GDMF_VULKAN_LOG_IF_FAILED(expression)                                       \
    do {                                                                            \
        VkResult gdmf_vulkan_result = (expression);                                 \
        if (gdmf_vulkan_result != VK_SUCCESS) {                                     \
            printf("[Vulkan] %s failed: %d\n", #expression, gdmf_vulkan_result);    \
        }                                                                           \
    } while (0)

// Wait through the renderer's queue lock. Returns VK_SUCCESS with no device,
// otherwise the vkDeviceWaitIdle result. Does not cover unsubmitted commands.
VkResult gdmf_device_wait_idle(void);

// Renderer state accessors. Handles are borrowed; callers do not destroy them.
bool gdmf_vulkan_device_lost(void);
VkDevice gdmf_get_device(void);
VkPhysicalDevice gdmf_get_physical_device(void);
VkQueue gdmf_get_graphics_queue(void);
VkCommandPool gdmf_get_command_pool(void);
VkRenderPass gdmf_get_render_pass(void);
VkPipelineCache gdmf_get_pipeline_cache(void); // may be VK_NULL_HANDLE
VkExtent2D gdmf_get_swapchain_extent(void);
uint32_t gdmf_get_swapchain_image_count(void);

// Centered, aspect-correct drawing rectangle within the swapchain extent.
VkRect2D gdmf_get_render_viewport_rect(void);

// Shared packed palette data, refreshed when the tracked generation changes.
// Returns VK_NULL_HANDLE for an unavailable buffer or invalid image_index.
VkBuffer gdmf_get_palette_buffer(uint32_t image_index);

// Borrowed coordinator contexts/properties. Use only during the initialized
// renderer lifetime; resource ownership remains with the coordinator.
const VkPhysicalDeviceMemoryProperties* gdmf_get_memory_properties(void);
gdmf_vulkan_staging* gdmf_get_frame_staging(void);
gdmf_vulkan_uploads* gdmf_get_upload_context(void);
gdmf_vulkan_retirement_queue* gdmf_get_retirement_queue(void);

// Record compute outside the render pass. False means unavailable/invalid work
// or exhausted per-frame descriptors; zero workgroups can return true without
// recording work. Not a guarantee that all malformed inputs are validated.
struct GdmfComputeDispatch;
bool gdmf_compute_record(VkCommandBuffer command_buffer, uint32_t image_index,
                         const struct GdmfComputeDispatch* dispatch);

#endif // GDMF_VULKAN_INTERNAL_H
