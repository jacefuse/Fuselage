// Deferred resource ownership using the renderer's existing frame countdown.
// SEE: (GDMF_VULKAN.md, Deferred destruction).
#include "gdmf_vulkan_retirement.h"
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    GDMF_RETIRE_BUFFER, GDMF_RETIRE_IMAGE, GDMF_RETIRE_DESCRIPTOR_SET,
    GDMF_RETIRE_DESCRIPTOR_POOL, GDMF_RETIRE_SAMPLER
} gdmf_vulkan_retirement_kind;

struct gdmf_vulkan_retirement_entry {
    gdmf_vulkan_retirement_entry* next;
    uint32_t frames_remaining;
    gdmf_vulkan_retirement_kind kind;
    union {
        struct { VkBuffer buffer; VkImage image; VkImageView image_view; VkDeviceMemory memory; } resource;
        struct { VkDescriptorPool pool; VkDescriptorSet descriptor_set; } descriptor;
        VkDescriptorPool pool;
        VkSampler sampler;
    } handles;
};

int gdmf_defer_queue_init(VkDevice device, FuselageMutex* lock, uint32_t image_count,
    gdmf_vulkan_retirement_queue* queue) {
    if (image_count == 0 || image_count == UINT32_MAX) { return -1; }
    *queue = (gdmf_vulkan_retirement_queue){.device = device, .lock = lock, .frame_delay = image_count + 1};
    return 0;
}

// The caller holds the retirement lock.
static void gdmf_defer_forget_sets(gdmf_vulkan_retirement_queue* queue, VkDescriptorPool pool) {
    gdmf_vulkan_retirement_entry** link = &queue->entries;
    while (*link) {
        gdmf_vulkan_retirement_entry* entry = *link;
        if (entry->kind == GDMF_RETIRE_DESCRIPTOR_SET && entry->handles.descriptor.pool == pool) {
            *link = entry->next;
            free(entry);
        } else { link = &entry->next; }
    }
}

static bool gdmf_defer_push(gdmf_vulkan_retirement_queue* queue,
    const gdmf_vulkan_retirement_entry* resource) {
    gdmf_vulkan_retirement_entry* entry = malloc(sizeof(*entry));
    if (!entry) {
        printf("[Vulkan] Resource retirement allocation failed; caller retains ownership\n");
        return false;
    }
    *entry = *resource;
    fuselage_mutex_lock(queue->lock);
    if (entry->kind == GDMF_RETIRE_DESCRIPTOR_SET) {
        for (gdmf_vulkan_retirement_entry* pending = queue->entries; pending; pending = pending->next) {
            if (pending->kind == GDMF_RETIRE_DESCRIPTOR_POOL &&
                pending->handles.pool == entry->handles.descriptor.pool) {
                fuselage_mutex_unlock(queue->lock);
                free(entry);
                return true; // The queued pool destruction will reclaim its sets.
            }
        }
    }
    if (entry->kind == GDMF_RETIRE_DESCRIPTOR_POOL) {
        gdmf_defer_forget_sets(queue, entry->handles.pool);
    }
    entry->frames_remaining = queue->frame_delay;
    entry->next = queue->entries;
    queue->entries = entry;
    fuselage_mutex_unlock(queue->lock);
    return true;
}

bool gdmf_defer_destroy_buffer(gdmf_vulkan_retirement_queue* queue,
    VkBuffer buffer, VkDeviceMemory memory) {
    if (buffer == VK_NULL_HANDLE && memory == VK_NULL_HANDLE) { return true; }
    gdmf_vulkan_retirement_entry entry = {.kind = GDMF_RETIRE_BUFFER};
    entry.handles.resource.buffer = buffer;
    entry.handles.resource.memory = memory;
    return gdmf_defer_push(queue, &entry);
}

bool gdmf_defer_destroy_image(gdmf_vulkan_retirement_queue* queue,
    VkImage image, VkImageView image_view, VkDeviceMemory memory) {
    if (image == VK_NULL_HANDLE && image_view == VK_NULL_HANDLE && memory == VK_NULL_HANDLE) { return true; }
    gdmf_vulkan_retirement_entry entry = {.kind = GDMF_RETIRE_IMAGE};
    entry.handles.resource.image = image;
    entry.handles.resource.image_view = image_view;
    entry.handles.resource.memory = memory;
    return gdmf_defer_push(queue, &entry);
}

bool gdmf_defer_free_descriptor_set(gdmf_vulkan_retirement_queue* queue,
    VkDescriptorPool pool, VkDescriptorSet descriptor_set) {
    if (pool == VK_NULL_HANDLE || descriptor_set == VK_NULL_HANDLE) { return true; }
    gdmf_vulkan_retirement_entry entry = {.kind = GDMF_RETIRE_DESCRIPTOR_SET};
    entry.handles.descriptor.pool = pool;
    entry.handles.descriptor.descriptor_set = descriptor_set;
    return gdmf_defer_push(queue, &entry);
}

bool gdmf_defer_destroy_descriptor_pool(gdmf_vulkan_retirement_queue* queue, VkDescriptorPool pool) {
    if (pool == VK_NULL_HANDLE) { return true; }
    gdmf_vulkan_retirement_entry entry = {.kind = GDMF_RETIRE_DESCRIPTOR_POOL};
    entry.handles.pool = pool;
    return gdmf_defer_push(queue, &entry);
}

bool gdmf_defer_destroy_sampler(gdmf_vulkan_retirement_queue* queue, VkSampler sampler) {
    if (sampler == VK_NULL_HANDLE) { return true; }
    gdmf_vulkan_retirement_entry entry = {.kind = GDMF_RETIRE_SAMPLER};
    entry.handles.sampler = sampler;
    return gdmf_defer_push(queue, &entry);
}

void gdmf_defer_forget_descriptor_pool(gdmf_vulkan_retirement_queue* queue, VkDescriptorPool pool) {
    if (pool == VK_NULL_HANDLE) { return; }
    fuselage_mutex_lock(queue->lock);
    gdmf_defer_forget_sets(queue, pool);
    fuselage_mutex_unlock(queue->lock);
}

static bool gdmf_defer_destroy_entry(VkDevice device, const gdmf_vulkan_retirement_entry* entry) {
    switch (entry->kind) {
    case GDMF_RETIRE_BUFFER:
    case GDMF_RETIRE_IMAGE:
        if (entry->handles.resource.buffer != VK_NULL_HANDLE) { vkDestroyBuffer(device, entry->handles.resource.buffer, NULL); }
        if (entry->handles.resource.image_view != VK_NULL_HANDLE) { vkDestroyImageView(device, entry->handles.resource.image_view, NULL); }
        if (entry->handles.resource.image != VK_NULL_HANDLE) { vkDestroyImage(device, entry->handles.resource.image, NULL); }
        if (entry->handles.resource.memory != VK_NULL_HANDLE) { vkFreeMemory(device, entry->handles.resource.memory, NULL); }
        break;
    case GDMF_RETIRE_DESCRIPTOR_SET: {
        VkResult result = vkFreeDescriptorSets(device, entry->handles.descriptor.pool,
            1, &entry->handles.descriptor.descriptor_set);
        if (result != VK_SUCCESS) {
            printf("[Vulkan] Deferred descriptor-set free failed: %d\n", result);
            return false;
        }
        break;
    }
    case GDMF_RETIRE_DESCRIPTOR_POOL:
        vkDestroyDescriptorPool(device, entry->handles.pool, NULL);
        break;
    case GDMF_RETIRE_SAMPLER:
        vkDestroySampler(device, entry->handles.sampler, NULL);
        break;
    }
    return true;
}

// The caller holds the retirement lock.
static bool gdmf_defer_process(gdmf_vulkan_retirement_queue* queue, bool flush) {
    gdmf_vulkan_retirement_entry** link = &queue->entries;
    while (*link) {
        gdmf_vulkan_retirement_entry* entry = *link;
        if (entry->frames_remaining > 0) { entry->frames_remaining--; }
        if ((flush || entry->frames_remaining == 0) && gdmf_defer_destroy_entry(queue->device, entry)) {
            *link = entry->next;
            free(entry);
        } else { link = &entry->next; }
    }
    return queue->entries == NULL;
}

void gdmf_defer_tick(gdmf_vulkan_retirement_queue* queue) {
    fuselage_mutex_lock(queue->lock);
    gdmf_defer_process(queue, false);
    fuselage_mutex_unlock(queue->lock);
}

bool gdmf_defer_flush(gdmf_vulkan_retirement_queue* queue) {
    fuselage_mutex_lock(queue->lock);
    bool drained = gdmf_defer_process(queue, true);
    fuselage_mutex_unlock(queue->lock);
    return drained;
}
