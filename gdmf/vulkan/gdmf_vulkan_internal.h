#ifndef GDMF_VULKAN_INTERNAL_H
#define GDMF_VULKAN_INTERNAL_H

// GDMF internal -- Vulkan state and utilities for GDMF subsystems.
// Include only in GDMF subsystem .c files. Never expose to game code.
//
// Deliberately platform-neutral: no VK_USE_PLATFORM_*_KHR define here, since
// none of the types/declarations below need platform-specific Vulkan
// surface types. Only the platform surface file itself (gdmf_surface_win32.c
// et al.) needs that define.

#include <vulkan/vulkan.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define GDMF_VULKAN_INTERNAL_VERSION "0.4.2026071601 DERRIERE"

// One-time command callback (atlas uploads, buffer copies, etc.)
typedef void (*GDMF_CommandRecordFunc)(VkCommandBuffer cmd, void* user_data);

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

// Memory allocation for buffers the CPU rewrites and the GPU reads hot
// (vertex fetch, per-fragment palette/cell lookups): tries DEVICE_LOCAL +
// HOST_VISIBLE + HOST_COHERENT first (BAR memory -- CPU-writable VRAM, so
// the GPU-side reads stay on-package instead of crossing the PCIe bus
// every access), falling back to plain HOST_VISIBLE + HOST_COHERENT --
// what every caller used originally and remains fully correct -- when the
// BAR type doesn't exist OR when the BAR allocation itself fails. That
// second case is not exotic: without Resizable BAR the whole BAR heap is
// typically 256MB, and one large fluctuating vertex buffer (a heavily
// zoomed-out tile layer) can exhaust it outright, so allocation failure
// here must mean "use ordinary host memory", never "give up". Use for
// CPU-write/GPU-read-per-frame buffers only -- NOT for transfer-src
// staging (the GPU reads those exactly once; keep them in plain host
// memory) and not for pure GPU images (plain DEVICE_LOCAL, no host access).
// Returns the final vkAllocateMemory result (or VK_ERROR_OUT_OF_DEVICE_
// MEMORY if no host-visible type exists at all, which no real device hits).
VkResult gdmfAllocateHostVisiblePreferDeviceLocal(const VkMemoryRequirements* req,
                                                  VkDeviceMemory* out_memory);
int      gdmfExecuteOneTimeCommands(GDMF_CommandRecordFunc record_func, void* user_data);

// Drain the GPU before destroying resources it may still be using. Always
// use this instead of calling vkDeviceWaitIdle directly: device-wait-idle
// is specified as acting on every queue the device owns, and queue access
// must be externally synchronized -- a subsystem teardown on the sim thread
// (ReleaseTileLayer, ShutdownTiles, ...) calling vkDeviceWaitIdle raw would
// race the render thread mid-vkQueueSubmit/vkQueuePresentKHR. This routes
// the wait through the same queue lock every other queue op already takes.
// Returns the vkDeviceWaitIdle result (VK_SUCCESS if no device exists yet).
VkResult gdmf_device_wait_idle(void);

// Deferred GPU-resource destruction -- for releasing a single resource
// mid-game (ReleasePixie, ReleaseTileLayer, ...) without stalling and
// without racing the GPU. A handle passed here is queued, not destroyed:
// actual destruction happens on the render thread only after every frame
// that could still reference the handle -- all in-flight frames PLUS the
// currently-recorded-but-not-yet-submitted one, which no vkDeviceWaitIdle
// can cover -- has completed. Safe to call from any thread (sim thread
// included); VK_NULL_HANDLE handles are ignored, so callers can pass
// whatever they hold without checking. The queue is drained incrementally
// once per frame, and flushed whole on swapchain recreation and at
// shutdown (both points where the device is idle and no recorded frame is
// pending).
void gdmf_defer_destroy_buffer(VkBuffer buffer, VkDeviceMemory memory);
void gdmf_defer_destroy_image(VkImage image, VkImageView view, VkDeviceMemory memory);
void gdmf_defer_free_descriptor_set(VkDescriptorPool pool, VkDescriptorSet set);
void gdmf_defer_destroy_descriptor_pool(VkDescriptorPool pool);
void gdmf_defer_destroy_sampler(VkSampler sampler);

// Call immediately BEFORE destroying a descriptor pool directly (subsystem
// shutdown paths) when individual set-frees against that pool may still be
// queued above: drops those queued entries without calling
// vkFreeDescriptorSets on them -- destroying the pool reclaims every set it
// owns anyway, and freeing a set into an already-destroyed pool would be
// use-after-free.
void gdmf_defer_forget_descriptor_pool(VkDescriptorPool pool);

// Platform surface creation -- implemented once per platform (see
// gdmf_surface_win32.c; gdmf_surface_wayland.c/gdmf_surface_macos.c would
// follow the same shape later), with the Makefile choosing which one gets
// compiled in for the current PLATFORM. Declared here using only core
// Vulkan types so gdmf_vulkan.c (and this header) never need a single
// #ifdef to call the right one -- the build picks the implementation.
const char* gdmf_platform_surface_extension(void);
int         gdmf_create_platform_surface(VkInstance instance, VkSurfaceKHR* outSurface);

// Vulkan state accessors
// True once vkQueueSubmit/vkQueuePresentKHR has reported VK_ERROR_DEVICE_LOST
// -- see g_deviceLost's doc comment in gdmf_vulkan.c. GDMF_Tick() folds this
// into its alive check so the engine shuts down cleanly instead of
// continuing to submit to a dead device every frame.
bool             gdmf_vulkan_device_lost(void);
VkDevice         gdmf_get_device(void);
VkPhysicalDevice gdmf_get_physical_device(void);
VkQueue          gdmf_get_graphics_queue(void);
VkCommandPool    gdmf_get_command_pool(void);
VkRenderPass     gdmf_get_render_pass(void);
// Device-lifetime pipeline cache -- pass to every vkCreateGraphicsPipelines
// call so lazy pipeline rebuilds (after swapchain recreation) hit the
// driver's cache instead of recompiling. May be VK_NULL_HANDLE (cache
// creation failed); vkCreateGraphicsPipelines accepts that as "no cache".
VkPipelineCache  gdmf_get_pipeline_cache(void);
VkExtent2D       gdmf_get_swapchain_extent(void);
uint32_t         gdmf_get_swapchain_image_count(void);

// Centered, aspect-correct sub-rectangle of the current swapchain extent --
// subsystems should set their viewport/scissor to this instead of the full
// extent, so the design aspect ratio is preserved (letterboxed/pillarboxed)
// rather than stretched. See the implementation in gdmf_vulkan.c for why.
VkRect2D gdmf_get_render_viewport_rect(void);

// Shared palette buffer -- one per swapchain image, holding the full
// Colors[256][16] table (PackRGBA8-packed), re-uploaded once per frame by
// gdmf_vulkan.c before any subsystem's own prepare() runs. Subsystems that
// need palette lookup (sprites, tiles, ...) bind this same buffer as a
// VK_DESCRIPTOR_TYPE_STORAGE_BUFFER in their own descriptor set instead of
// maintaining a private, redundantly-uploaded copy of their own. Returns
// VK_NULL_HANDLE if imageIndex is out of range or the buffers don't exist
// yet (e.g. queried before gdmf_vulkan_init() has finished).
#define GDMF_PALETTE_BUFFER_SIZE (256 * 16 * sizeof(uint32_t))
VkBuffer gdmf_get_palette_buffer(uint32_t imageIndex);

// Subsystem prepare hooks (called from gdmf_vulkan_prepare_frame before render pass).
// imageIndex is the swapchain image acquired for this frame -- subsystems
// with per-frame CPU-written GPU buffers (vertex data, dynamic uniforms/
// storage) must keep one copy per image index, since a previous frame's
// command buffer using a different image index may still be executing on
// the GPU and reading its own copy while this one is being overwritten.
void gdmf_textlayer_prepare(uint32_t imageIndex);
void gdmf_sprites_prepare(uint32_t imageIndex);
void gdmf_tiles_prepare(uint32_t imageIndex);
// Pixies additionally take the frame's open command buffer: dirty pixie
// pixel data is recorded as a staging->image copy directly into this
// frame's command buffer (before the render pass begins), instead of the
// old per-upload submit + queue-wait-idle that drained the whole GPU
// pipeline for every redrawn pixie -- see upload_pixie_output.
void gdmf_pixies_prepare(VkCommandBuffer cmd, uint32_t imageIndex);

// Subsystem render hooks (called from gdmf_vulkan_submit_frame inside render
// pass), once per priority level as the loop walks 255 -> 0. Each draws only
// the items sitting at exactly `prio`; the loop's call order per level
// (pixies, then tiles, then sprites) is the type tie-break at equal priority.
void gdmf_textlayer_record(VkCommandBuffer cmd, uint32_t imageIndex);
void gdmf_sprites_record_priority(VkCommandBuffer cmd, uint32_t imageIndex, uint8_t prio);
void gdmf_tiles_record_priority(VkCommandBuffer cmd, uint32_t imageIndex, uint8_t prio);
void gdmf_pixies_record_priority(VkCommandBuffer cmd, uint32_t imageIndex, uint8_t prio);

// Masked compositing support (Approach C -- PIXIE_MODE_MASK, see gdmf_pixies.h).
// A MASK-mode pixie draws nothing of its own; instead the compositor in
// gdmf_vulkan.c renders the items at that pixie's own priority to an offscreen
// target and composites it back onto the swapchain through the mask, so layers
// already drawn behind that priority show through wherever it's masked out.
// These two let the compositor discover and consume masks.
//
// gdmf_pixies_priority_mask_id: id of the lowest-numbered shown+ready MASK
// pixie at priority `prio`, or -1 if that priority has no mask this frame.
int gdmf_pixies_priority_mask_id(uint8_t prio);

// Fills the mask's sampled image view, its display rect in NDC (outNdcRect =
// {minX, minY, maxX, maxY}, computed the same way a normal pixie quad's
// vertices are so a mask lands exactly where a same-attrs pixie would draw),
// and its invert flag. Returns false and touches nothing if `id` isn't a
// currently-ready MASK pixie.
bool gdmf_pixies_get_mask_info(int id, VkImageView* outView,
                               float outNdcRect[4], bool* outInvert);

// Swapchain invalidation hooks (called from gdmf_recreate_swapchain after a
// new swapchain -- and possibly a rebuilt render pass and/or a changed
// image count -- is in place). Subsystems must tear down anything that
// depended on the old render pass or was sized to the old image count;
// the next prepare() call lazily rebuilds it against current state.
void gdmf_textlayer_on_swapchain_recreated(void);
void gdmf_sprites_on_swapchain_recreated(void);
void gdmf_tiles_on_swapchain_recreated(void);
void gdmf_pixies_on_swapchain_recreated(void);

#endif // GDMF_VULKAN_INTERNAL_H
