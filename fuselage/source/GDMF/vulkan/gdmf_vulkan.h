#ifndef GDMF_VULKAN_H
#define GDMF_VULKAN_H

// Internal lifecycle and frame interface between GDMF and the Vulkan renderer.
// No Vulkan types are exposed here.
#include <stdbool.h>

#define GDMF_VULKAN_VERSION "0.5.2026102501 ENTERON"

// Call on the main/render thread after window creation. Returns 0 on success
// or -1 on initialization failure. The caller must clean up partial resources
// through shutdown before destroying the window.
int gdmf_vulkan_init(void);

// Stop frame operations and resource producers before shutdown. Release layer
// resources that depend on the renderer before destroying the renderer itself.
// False retains resources when completion/draining is unresolved. Keep the
// window/device owners alive and resolve recovery before retrying shutdown.
bool gdmf_vulkan_shutdown(void);

// Stop producers first. Complete work and discard unsubmitted frame recording
// before dependent subsystem teardown. False leaves resources for recovery.
bool gdmf_vulkan_quiesce(void);
bool gdmf_vulkan_should_stop(void);
bool gdmf_vulkan_has_resources(void);

// Optionally acquire an image before locking application drawing state.
// May block for image availability and prior GPU work. True means an image is
// acquired (including one already acquired); false means no image is ready.
// Swapchain recreation is left to prepare_frame under the drawing-state lock.
bool gdmf_vulkan_acquire_frame(void);

// Snapshot drawing state while holding the lock shared with application updates.
// Acquires an image if needed. Returns true when a frame is marked pending for
// submission, false when skipped (including minimization, device loss, or a
// swapchain recreation attempt). True does not guarantee GPU execution success.
bool gdmf_vulkan_prepare_frame(void);

// Submit outside the drawing-state lock. Returns true only when a presentation
// request is accepted (SUCCESS/SUBOPTIMAL), not proof of actual screen display.
// Returns false for no pending frame, skipped/failed work, or unresolved recovery.
// All frame operations belong on the same main/render thread.
bool gdmf_vulkan_submit_frame(void);

#endif // GDMF_VULKAN_H
