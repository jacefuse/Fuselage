#pragma once
// GDMF internal -- Vulkan state and utilities for GDMF subsystems.
// Include only in GDMF subsystem .c files. Never expose to game code.
//
// Deliberately platform-neutral: no VK_USE_PLATFORM_*_KHR define here, since
// none of the types/declarations below need platform-specific Vulkan
// surface types. Only the platform surface file itself (gdmf_surface_win32.c
// et al.) needs that define.

#include <vulkan/vulkan.h>
#include <stdint.h>
#include <stdio.h>

#define GDMF_VULKAN_INTERNAL_VERSION "0.2.26061701 BUTTOCKS"

// One-time command callback (atlas uploads, buffer copies, etc.)
typedef void (*GDMFCommandRecordFunc)(VkCommandBuffer cmd, void* user_data);

// Wrap a VkResult-returning call inside a resource-creation function that
// has a local "fail:" label. On failure, logs which call failed and jumps
// to fail: for that function's own cleanup -- use this for setup/teardown
// sequences where a failure partway through means unwinding whatever was
// already created. Do NOT use this in steady-state per-frame code (nothing
// to unwind to); see VK_LOG_IF_FAILED for that case instead.
#define VK_CHECK(expr)                                                     \
    do {                                                                   \
        VkResult vkResult__ = (expr);                                     \
        if (vkResult__ != VK_SUCCESS) {                                    \
            printf("[Vulkan] %s failed: %d\n", #expr, vkResult__);         \
            goto fail;                                                     \
        }                                                                  \
    } while (0)

// Wrap a VkResult-returning call that has no meaningful way to unwind --
// the steady-state per-frame path (vkBeginCommandBuffer, vkQueueSubmit,
// etc.), where a mid-frame failure can't be rolled back, only logged so
// it's visible instead of silently swallowed. The frame is simply allowed
// to continue/end; next frame's fence wait or subsequent calls will surface
// anything seriously wrong.
#define VK_LOG_IF_FAILED(expr)                                             \
    do {                                                                   \
        VkResult vkResult__ = (expr);                                     \
        if (vkResult__ != VK_SUCCESS)                                      \
            printf("[Vulkan] %s failed: %d\n", #expr, vkResult__);         \
    } while (0)

// Memory utilities
uint32_t gdmfFindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties);
int      gdmfExecuteOneTimeCommands(GDMFCommandRecordFunc record_func, void* user_data);

// Platform surface creation -- implemented once per platform (see
// gdmf_surface_win32.c; gdmf_surface_wayland.c/gdmf_surface_macos.c would
// follow the same shape later), with the Makefile choosing which one gets
// compiled in for the current PLATFORM. Declared here using only core
// Vulkan types so gdmf_vulkan.c (and this header) never need a single
// #ifdef to call the right one -- the build picks the implementation.
const char* gdmf_platform_surface_extension(void);
int         gdmf_create_platform_surface(VkInstance instance, VkSurfaceKHR* outSurface);

// Vulkan state accessors
VkDevice         gdmf_get_device(void);
VkPhysicalDevice gdmf_get_physical_device(void);
VkQueue          gdmf_get_graphics_queue(void);
VkCommandPool    gdmf_get_command_pool(void);
VkRenderPass     gdmf_get_render_pass(void);
VkExtent2D       gdmf_get_swapchain_extent(void);
uint32_t         gdmf_get_swapchain_image_count(void);

// Centered, aspect-correct sub-rectangle of the current swapchain extent --
// subsystems should set their viewport/scissor to this instead of the full
// extent, so the design aspect ratio is preserved (letterboxed/pillarboxed)
// rather than stretched. See the implementation in gdmf_vulkan.c for why.
VkRect2D gdmf_get_render_viewport_rect(void);

// Subsystem prepare hooks (called from gdmf_vulkan_render_frame before render pass).
// imageIndex is the swapchain image acquired for this frame -- subsystems
// with per-frame CPU-written GPU buffers (vertex data, dynamic uniforms/
// storage) must keep one copy per image index, since a previous frame's
// command buffer using a different image index may still be executing on
// the GPU and reading its own copy while this one is being overwritten.
void gdmf_textlayer_prepare(uint32_t imageIndex);
void gdmf_sprites_prepare(uint32_t imageIndex);
void gdmf_tiles_prepare(uint32_t imageIndex);

// Subsystem render hooks (called from gdmf_vulkan_render_frame inside render pass)
void gdmf_textlayer_record(VkCommandBuffer cmd, uint32_t imageIndex);
void gdmf_sprites_record_band(VkCommandBuffer cmd, uint32_t imageIndex, uint8_t band);
void gdmf_tiles_record_layer(VkCommandBuffer cmd, uint32_t imageIndex, uint8_t layer);

// Swapchain invalidation hooks (called from gdmf_recreate_swapchain after a
// new swapchain -- and possibly a rebuilt render pass and/or a changed
// image count -- is in place). Subsystems must tear down anything that
// depended on the old render pass or was sized to the old image count;
// the next prepare() call lazily rebuilds it against current state.
void gdmf_textlayer_on_swapchain_recreated(void);
void gdmf_sprites_on_swapchain_recreated(void);
void gdmf_tiles_on_swapchain_recreated(void);
