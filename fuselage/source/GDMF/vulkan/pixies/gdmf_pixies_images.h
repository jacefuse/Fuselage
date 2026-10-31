#ifndef GDMF_PIXIES_IMAGES_H
#define GDMF_PIXIES_IMAGES_H

#include "../renderer/resources/gdmf_vulkan_memory.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    VkDevice device;
    VkPhysicalDevice physical_device;
    VkPhysicalDeviceMemoryProperties memory_properties;
    VkSampler nearest_sampler, linear_sampler;
    VkDescriptorSetLayout descriptor_layout;
    VkDescriptorPool descriptor_pool;
} gdmf_pixies_image_context;

typedef struct {
    VkBuffer buffer;
    VkDeviceMemory memory;
    void* mapped;
} gdmf_pixies_image_staging;

typedef struct {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    VkDescriptorSet nearest_set, linear_set;
    uint32_t width, height;
    bool mask, layout_initialized;
    gdmf_pixies_image_staging* staging;
    uint32_t staging_count;
} gdmf_pixies_image;

// Caller-owned results; device and physical-device handles are borrowed.
// Supply zeroed outputs; outputs change only on success. Linear sampling is
// optional. No GPU commands are submitted and no global renderer is consulted.
bool gdmf_pixies_image_context_create(VkDevice device, VkPhysicalDevice physical_device,
                                      const VkPhysicalDeviceMemoryProperties* memory_properties,
                                      gdmf_pixies_image_context* context);
bool gdmf_pixies_image_create(const gdmf_pixies_image_context* context, uint32_t width, uint32_t height,
                              bool mask, gdmf_pixies_image* image);

// The first successful upload initializes the layout and every pixel. Later
// uploads start in SHADER_READ_ONLY_OPTIMAL. The frame's prior image-index work
// must be complete before its staging slot is reused. False leaves contents
// pending; caller clears dirty only on true. Recorded work is not completion.
bool gdmf_pixies_image_upload(const gdmf_pixies_image_context* context, gdmf_pixies_image* image,
                              VkCommandBuffer command_buffer, uint32_t image_index, uint32_t image_count,
                              const void* pixels, size_t byte_count);

// Only after GPU completion, discarded recorded references and excluded users.
// False retains ownership if descriptor freeing fails. Clear image resources
// before context teardown. These functions neither wait nor perform retirement.
bool gdmf_pixies_image_destroy(const gdmf_pixies_image_context* context, gdmf_pixies_image* image);
// Same completion/reference preconditions; preserves image pixels/descriptors
// while releasing the upload ring before a swapchain image-count change.
void gdmf_pixies_image_staging_reset(const gdmf_pixies_image_context* context, gdmf_pixies_image* image);
void gdmf_pixies_image_context_destroy(gdmf_pixies_image_context* context);

#endif
