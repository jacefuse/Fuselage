// One synchronized capture request, pending through readback and file output.
// SEE: (GDMF_VULKAN.md, Capture request handling).
#include "gdmf_vulkan_capture_requests.h"
#include <string.h>

void gdmf_vulkan_capture_requests_init(FuselageMutex* lock, gdmf_vulkan_capture_requests* requests) {
    *requests = (gdmf_vulkan_capture_requests){.lock = lock, .state = GDMF_CAPTURE_IDLE};
}

bool gdmf_vulkan_capture_request_submit(gdmf_vulkan_capture_requests* requests, const char* path) {
    if (!path) { return false; }
    size_t path_length = 0;
    while (path_length < GDMF_VULKAN_CAPTURE_PATH_CAPACITY && path[path_length]) { path_length++; }
    if (path_length == 0 || path_length == GDMF_VULKAN_CAPTURE_PATH_CAPACITY) { return false; }

    fuselage_mutex_lock(requests->lock);
    if (requests->state != GDMF_CAPTURE_IDLE) {
        fuselage_mutex_unlock(requests->lock);
        return false;
    }
    memcpy(requests->request.path, path, path_length + 1);
    requests->state = GDMF_CAPTURE_QUEUED;
    fuselage_mutex_unlock(requests->lock);
    return true;
}

bool gdmf_vulkan_capture_request_pending(gdmf_vulkan_capture_requests* requests) {
    fuselage_mutex_lock(requests->lock);
    bool pending = requests->state != GDMF_CAPTURE_IDLE;
    fuselage_mutex_unlock(requests->lock);
    return pending;
}

bool gdmf_vulkan_capture_request_take(gdmf_vulkan_capture_requests* requests,
    gdmf_vulkan_capture_request* request) {
    fuselage_mutex_lock(requests->lock);
    if (requests->state != GDMF_CAPTURE_QUEUED) {
        fuselage_mutex_unlock(requests->lock);
        return false;
    }
    *request = requests->request;
    requests->state = GDMF_CAPTURE_PROCESSING;
    fuselage_mutex_unlock(requests->lock);
    return true;
}

bool gdmf_vulkan_capture_request_finish(gdmf_vulkan_capture_requests* requests) {
    fuselage_mutex_lock(requests->lock);
    if (requests->state != GDMF_CAPTURE_PROCESSING) {
        fuselage_mutex_unlock(requests->lock);
        return false;
    }
    requests->request.path[0] = '\0';
    requests->state = GDMF_CAPTURE_IDLE;
    fuselage_mutex_unlock(requests->lock);
    return true;
}
