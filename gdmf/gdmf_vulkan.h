#ifndef GDMF_VULKAN_H
#define GDMF_VULKAN_H

// GDMF internal
// Included only by gdmf.c and gdmf_vulkan.c.
// Does not expose Vulkan types; consumers need not include vulkan.h.

int  gdmf_vulkan_init(void);
void gdmf_vulkan_shutdown(void);

// Split so a caller can synchronize against live game state (sprites/
// tiles/pixies/text layer) across just the prepare half -- it's the only
// half that reads any of it. Submit only touches per-image GPU buffers
// prepare already filled, plus the GPU queue/present, and is where the
// vsync-blocking wait lives; it must never be called under the same lock
// as game logic.
void gdmf_vulkan_prepare_frame(void);
void gdmf_vulkan_submit_frame(void);

#define GDMF_VULKAN_VERSION "0.3.2026071502 COLON"

#endif // GDMF_VULKAN_H