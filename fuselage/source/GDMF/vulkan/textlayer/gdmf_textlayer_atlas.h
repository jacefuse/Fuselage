#ifndef GDMF_TEXTLAYER_ATLAS_H
#define GDMF_TEXTLAYER_ATLAS_H
#include "../renderer/resources/gdmf_vulkan_memory.h"
#include "../renderer/resources/gdmf_vulkan_uploads.h"
#include "../../textlayer/gdmf_textlayer_font.h"

typedef struct {
    VkDevice device;
    VkPhysicalDevice physical_device;
    const VkPhysicalDeviceMemoryProperties* memory_properties;
    gdmf_vulkan_uploads* uploads;
} gdmf_textlayer_atlas_config;
typedef struct {
    VkDevice device;
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    VkSampler sampler;
    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    gdmf_vulkan_uploads* unresolved_uploads;
    bool ready;
} gdmf_textlayer_atlas;

// Zeroed caller-owned output. COMPLETE publishes a sampleable atlas; FAILED
// leaves output unchanged. UNRESOLVED retains all owners and is not drawable.
// An already-blocked upload context returns UNRESOLVED without new resources.
// Device, properties and upload context are borrowed. SEE: (GDMF_TEXT.md, Vulkan atlas)
// Serialize atlas ownership; do not call while holding the upload or queue lock.
gdmf_vulkan_upload_result gdmf_textlayer_atlas_create(const gdmf_textlayer_atlas_config* config,
                                                      gdmf_textlayer_atlas* atlas);
// Stop producers; confirm completed GPU use, discarded command references and released descriptor
// users. Release retained uploader handles first. False retains every owner.
// No wait, submission or renderer-global access; empty cleanup is repeatable.
bool gdmf_textlayer_atlas_destroy(gdmf_textlayer_atlas* atlas, bool quiesced);
#endif
