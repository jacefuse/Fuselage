// Shared packed palette storage, one buffer per swapchain image.
// SEE: (GDMF_VULKAN.md, Shared palette buffers).
#include "gdmf_vulkan_palettes.h"
#include "../gdmf_vulkan_internal.h"
#include "../../../gdmf_colors.h"
#include <stdlib.h>

void gdmf_vulkan_palettes_destroy(VkDevice device, gdmf_vulkan_palettes* palettes) {
    if (palettes->slots) {
        for (uint32_t image_index = 0; image_index < palettes->image_count; image_index++) {
            gdmf_vulkan_palette_slot* slot = &palettes->slots[image_index];
            if (slot->buffer != VK_NULL_HANDLE) { vkDestroyBuffer(device, slot->buffer, NULL); }
            if (slot->memory != VK_NULL_HANDLE) { vkFreeMemory(device, slot->memory, NULL); }
        }
        free(palettes->slots);
    }
    *palettes = (gdmf_vulkan_palettes){0};
}

int gdmf_vulkan_palettes_create(VkDevice device,
    const VkPhysicalDeviceMemoryProperties* memory_properties, uint32_t image_count,
    gdmf_vulkan_palettes* palettes) {
    if (image_count == 0) {
        printf("[Vulkan] Cannot create palette buffers for zero images\n");
        return -1;
    }
    gdmf_vulkan_palettes created_palettes = {.image_count = image_count};
    created_palettes.slots = calloc(image_count, sizeof(gdmf_vulkan_palette_slot));
    VkResult result = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (!created_palettes.slots) { goto fail; }

    for (uint32_t image_index = 0; image_index < image_count; image_index++) {
        gdmf_vulkan_palette_slot* slot = &created_palettes.slots[image_index];
        VkBufferCreateInfo buffer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = GDMF_PALETTE_BUFFER_SIZE,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        VkBuffer buffer = VK_NULL_HANDLE;
        result = vkCreateBuffer(device, &buffer_info, NULL, &buffer);
        if (result != VK_SUCCESS) { goto fail; }
        slot->buffer = buffer;

        VkMemoryRequirements requirements;
        vkGetBufferMemoryRequirements(device, buffer, &requirements);
        VkDeviceMemory memory = VK_NULL_HANDLE;
        result = gdmf_allocate_host_visible_prefer_device_local(device, memory_properties,
            &requirements, &memory);
        if (result != VK_SUCCESS) { goto fail; }
        slot->memory = memory;
        result = vkBindBufferMemory(device, buffer, memory, 0);
        if (result != VK_SUCCESS) { goto fail; }

        void* mapped = NULL;
        result = vkMapMemory(device, memory, 0, GDMF_PALETTE_BUFFER_SIZE, 0, &mapped);
        if (result != VK_SUCCESS) { goto fail; }
        slot->mapped = mapped;
    }

    *palettes = created_palettes;
    printf("[Vulkan] %u palette buffers ready\n", image_count);
    return 0;

fail:
    gdmf_vulkan_palettes_destroy(device, &created_palettes);
    printf("[Vulkan] Palette buffer setup failed: %d\n", result);
    return -1;
}

void gdmf_vulkan_palettes_prepare(gdmf_vulkan_palettes* palettes, uint32_t image_index) {
    if (!palettes->slots || image_index >= palettes->image_count) { return; }
    gdmf_vulkan_palette_slot* slot = &palettes->slots[image_index];
    if (!slot->mapped) { return; }

    uint32_t generation = GetColorsGeneration();
    if (slot->uploaded && slot->uploaded_generation == generation) { return; }

    uint32_t* packed_colors = slot->mapped;
    for (uint32_t palette_index = 0; palette_index < 256; palette_index++) {
        for (uint32_t color_index = 0; color_index < 16; color_index++) {
            packed_colors[palette_index * 16 + color_index] =
                PackColorsRGBA8(Colors[palette_index][color_index]);
        }
    }
    slot->uploaded_generation = generation;
    slot->uploaded = true;
}

VkBuffer gdmf_vulkan_palettes_get_buffer(const gdmf_vulkan_palettes* palettes,
    uint32_t image_index) {
    if (!palettes->slots || image_index >= palettes->image_count) { return VK_NULL_HANDLE; }
    return palettes->slots[image_index].buffer;
}
