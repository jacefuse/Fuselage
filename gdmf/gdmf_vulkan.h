#pragma once

// GDMF internal
// Included only by gdmf.c and gdmf_vulkan.c.
// Does not expose Vulkan types; consumers need not include vulkan.h.

int  gdmf_vulkan_init(void);
void gdmf_vulkan_shutdown(void);
void gdmf_vulkan_render_frame(void);

#define GDMF_VULKAN_VERSION "0.2.26061701 BUTTOCKS"