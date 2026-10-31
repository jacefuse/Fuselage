// Per-image mapped vertex storage. SEE: (GDMF_TILES.md, Vulkan graphics resources)
#include "gdmf_tiles_graphics.h"
#include <string.h>

void gdmf_tiles_vertex_buffer_destroy(gdmf_tiles_vertex_buffer* buffer) {
    if (!buffer || !buffer->device) {
        return;
    }
    if (buffer->mapped && buffer->memory) {
        vkUnmapMemory(buffer->device, buffer->memory);
    }
    if (buffer->buffer) {
        vkDestroyBuffer(buffer->device, buffer->buffer, NULL);
    }
    if (buffer->memory) {
        vkFreeMemory(buffer->device, buffer->memory, NULL);
    }
    memset(buffer, 0, sizeof(*buffer));
}

bool gdmf_tiles_vertex_buffer_ensure(VkDevice device,
                                     const VkPhysicalDeviceMemoryProperties* memory_properties,
                                     uint32_t required_vertices, gdmf_tiles_vertex_buffer* buffer) {
    if (!device || !memory_properties || !buffer || !required_vertices ||
        (buffer->device && buffer->device != device)) {
        return false;
    }
    if (buffer->buffer && buffer->mapped && buffer->capacity >= required_vertices) {
        return true;
    }
    uint32_t capacity = required_vertices < 4096 ? 4096 : required_vertices;
    if ((size_t)capacity > SIZE_MAX / sizeof(gdmf_tile_vertex)) {
        return false;
    }
    gdmf_tiles_vertex_buffer pending = {.device = device, .capacity = capacity};
    VkBufferCreateInfo creation = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = (VkDeviceSize)capacity * sizeof(gdmf_tile_vertex),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkBuffer handle = VK_NULL_HANDLE;
    if (vkCreateBuffer(device, &creation, NULL, &handle) != VK_SUCCESS) {
        return false;
    }
    pending.buffer = handle;
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(device, handle, &requirements);
    if (gdmf_allocate_host_visible_prefer_device_local(device, memory_properties, &requirements,
                                                       &pending.memory) != VK_SUCCESS) {
        goto fail;
    }
    if (vkBindBufferMemory(device, handle, pending.memory, 0) != VK_SUCCESS) {
        goto fail;
    }
    void* mapped = NULL;
    if (vkMapMemory(device, pending.memory, 0, creation.size, 0, &mapped) != VK_SUCCESS) {
        goto fail;
    }
    pending.mapped = mapped;
    gdmf_tiles_vertex_buffer_destroy(buffer);
    *buffer = pending;
    return true;
fail:
    gdmf_tiles_vertex_buffer_destroy(&pending);
    return false;
}
