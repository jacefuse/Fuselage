#ifndef GDMF_VULKAN_UPLOADS_H
#define GDMF_VULKAN_UPLOADS_H

#include <vulkan/vulkan.h>
#include "../../../../BASE/fuselage_sync.h"

typedef void (*gdmf_vulkan_command_record_callback)(VkCommandBuffer command_buffer, void* user_data);

typedef enum {
    GDMF_VULKAN_UPLOAD_COMPLETE = 0,
    GDMF_VULKAN_UPLOAD_FAILED = -1,
    GDMF_VULKAN_UPLOAD_UNRESOLVED = -2
} gdmf_vulkan_upload_result;

// Caller-owned context. Device, graphics queue, upload pool, and initialized
// locks are borrowed. Initialize retained handles to VK_NULL_HANDLE.
typedef struct {
    VkDevice device;
    VkQueue graphics_queue;
    VkCommandPool upload_pool;
    FuselageMutex* upload_lock;
    FuselageMutex* queue_lock;
    VkCommandBuffer retained_command_buffer;
    VkFence retained_fence;
} gdmf_vulkan_uploads;

// Record, submit, and wait. Returns COMPLETE, FAILED before submission, or
// UNRESOLVED after submit/wait errors or when earlier work blocks the context.
// Supply a valid context and callback; NULL callback is rejected. The callback
// records commands only and must not reenter this service or acquire its locks.
// Upload lock precedes queue lock; the fence wait does not hold the queue lock.
// A submit or wait failure retains handles and blocks further uploads. Referenced
// resources must remain alive until completion is established and handles released.
// Prefer frame staging for recurring uploads.
gdmf_vulkan_upload_result gdmf_execute_one_time_commands(gdmf_vulkan_uploads* uploads,
    gdmf_vulkan_command_record_callback record_commands, void* user_data);

// Stop upload producers and establish completion externally before calling.
// Releases retained handles under the upload lock; performs no wait and does
// not release borrowed resources. Device/pool/locks must still be valid.
// SEE: (GDMF_VULKAN.md, One-time upload commands).
void gdmf_vulkan_uploads_release_completed(gdmf_vulkan_uploads* uploads);

#endif // GDMF_VULKAN_UPLOADS_H
