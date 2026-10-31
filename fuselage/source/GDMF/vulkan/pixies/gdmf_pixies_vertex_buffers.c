// Shared allocation path for both vertex layouts. SEE: (GDMF_PIXIES.md, Frame preparation)
#include "gdmf_pixies_graphics.h"
#include <string.h>

void gdmf_pixies_vertex_buffer_destroy(VkDevice device, gdmf_pixies_vertex_buffer* buffer) {
    if (!device || !buffer)
        return;
    if (buffer->mapped && buffer->memory)
        vkUnmapMemory(device, buffer->memory);
    if (buffer->buffer)
        vkDestroyBuffer(device, buffer->buffer, NULL);
    if (buffer->memory)
        vkFreeMemory(device, buffer->memory, NULL);
    memset(buffer, 0, sizeof *buffer);
}

bool gdmf_pixies_vertex_buffer_ensure(VkDevice device, const VkPhysicalDeviceMemoryProperties* properties,
                                      size_t bytes, size_t headroom, gdmf_pixies_vertex_buffer* buffer) {
    if (!device || !properties || !buffer || !bytes || headroom > SIZE_MAX - bytes)
        return false;
    if (buffer->buffer && buffer->mapped && buffer->capacity >= bytes)
        return true;
    gdmf_pixies_vertex_buffer pending = {.capacity = bytes + headroom};
    VkBufferCreateInfo creation = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                   .size = pending.capacity,
                                   .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                   .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    VkBuffer handle = VK_NULL_HANDLE;
    if (vkCreateBuffer(device, &creation, NULL, &handle) != VK_SUCCESS)
        return false;
    pending.buffer = handle;
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(device, pending.buffer, &requirements);
    if (gdmf_allocate_host_visible_prefer_device_local(device, properties, &requirements, &pending.memory) !=
        VK_SUCCESS)
        goto fail;
    if (vkBindBufferMemory(device, pending.buffer, pending.memory, 0) != VK_SUCCESS)
        goto fail;
    void* mapped = NULL;
    if (vkMapMemory(device, pending.memory, 0, pending.capacity, 0, &mapped) != VK_SUCCESS)
        goto fail;
    pending.mapped = mapped;
    gdmf_pixies_vertex_buffer_destroy(device, buffer);
    *buffer = pending;
    return true;
fail:
    gdmf_pixies_vertex_buffer_destroy(device, &pending);
    return false;
}
