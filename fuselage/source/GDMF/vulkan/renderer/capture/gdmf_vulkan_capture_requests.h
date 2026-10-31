#ifndef GDMF_VULKAN_CAPTURE_REQUESTS_H
#define GDMF_VULKAN_CAPTURE_REQUESTS_H

#include <stdbool.h>
#include "../../../../BASE/fuselage_sync.h"

#define GDMF_VULKAN_CAPTURE_PATH_CAPACITY 512

typedef struct {
    char path[GDMF_VULKAN_CAPTURE_PATH_CAPACITY];
} gdmf_vulkan_capture_request;

typedef enum {
    GDMF_CAPTURE_IDLE,
    GDMF_CAPTURE_QUEUED,
    GDMF_CAPTURE_PROCESSING
} gdmf_vulkan_capture_request_state;

// Caller-owned request slot; lock is borrowed and protects all mutable state.
typedef struct {
    FuselageMutex* lock;
    gdmf_vulkan_capture_request_state state;
    gdmf_vulkan_capture_request request;
} gdmf_vulkan_capture_requests;

// Before producers start, initialize with a valid, initialized mutex. Stop all
// users and resolve processing before reinitializing or releasing the mutex.
void gdmf_vulkan_capture_requests_init(FuselageMutex* lock, gdmf_vulkan_capture_requests* requests);

// Any thread. Copies a valid path and returns true only when accepted.
// Rejects NULL/empty paths, paths without a terminator within capacity, and an
// outstanding queued/processing request. Caller must keep input stable while copied.
bool gdmf_vulkan_capture_request_submit(gdmf_vulkan_capture_requests* requests, const char* path);

// Any thread. True while queued or processing, not a file-write success status.
bool gdmf_vulkan_capture_request_pending(gdmf_vulkan_capture_requests* requests);

// Render-thread consumer only. Copies a queued request and marks it processing.
// Returns false without changing output if none is queued. Returns with the
// lock released before GPU/file work; pending stays true until finish.
bool gdmf_vulkan_capture_request_take(gdmf_vulkan_capture_requests* requests,
    gdmf_vulkan_capture_request* request);

// Same render-thread consumer, after the attempt has safely resolved. Returns
// true only when clearing processing state. Does not report whether a file saved.
// Do not finish unresolved GPU work that still requires recovery/resource retention.
bool gdmf_vulkan_capture_request_finish(gdmf_vulkan_capture_requests* requests);

#endif // GDMF_VULKAN_CAPTURE_REQUESTS_H
