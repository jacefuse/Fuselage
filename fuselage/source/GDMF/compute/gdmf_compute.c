// Kernel metadata and lazy initialization. SEE: (GDMF_COMPUTE.md, Kernel table)
#include "gdmf_compute.h"
#include <stdio.h>
static const GdmfKernelDescription gdmf_compute_kernels[GDMF_KERNEL_COUNT] = {
    [GDMF_KERNEL_DECODE_RAW] = {
        .id        = GDMF_KERNEL_DECODE_RAW,
        .name      = "decode_raw",
        .kind      = GDMF_KERNEL_BIND_BYTES_PALETTE_IMAGE,
        .push_bytes = (uint32_t)sizeof(GdmfDecodeRawPush),
        .group_width    = 16,
        .group_height    = 16,
    },
    [GDMF_KERNEL_DECODE_RLE] = {
        .id        = GDMF_KERNEL_DECODE_RLE,
        .name      = "decode_rle",
        .kind      = GDMF_KERNEL_BIND_BYTES_PALETTE_IMAGE,
        .push_bytes = (uint32_t)sizeof(GdmfDecodeRlePush),
        .group_width    = 64,
        .group_height    = 1,
    },
    [GDMF_KERNEL_DECODE_MASKTILE] = {
        .id        = GDMF_KERNEL_DECODE_MASKTILE,
        .name      = "decode_masktile",
        .kind      = GDMF_KERNEL_BIND_BYTES_PROBS_PREV_IMAGE,
        .push_bytes = (uint32_t)sizeof(GdmfDecodeMaskTilePush),
        .group_width    = 64,
        .group_height    = 1,
    },
};

_Static_assert(sizeof(GdmfDecodeMaskTilePush) == 12 * 4,
               "GdmfDecodeMaskTilePush must match decode_masktile.comp's push block: twelve 32-bit words");
_Static_assert(sizeof(GdmfDecodeRawPush) == 9 * 4,
               "GdmfDecodeRawPush must match decode_raw.comp's push block: nine 32-bit words");
_Static_assert(sizeof(GdmfDecodeRlePush) == 10 * 4,
               "GdmfDecodeRlePush must match decode_rle.comp's push block: ten 32-bit words");

const GdmfKernelDescription* gdmf_kernel_desc(GdmfKernelId id) {
    if ((unsigned)id >= (unsigned)GDMF_KERNEL_COUNT) { return NULL; }
    return &gdmf_compute_kernels[id];
}

void gdmf_kernel_groups(const GdmfKernelDescription* description, uint32_t width, uint32_t height,
                        uint32_t* groups_x, uint32_t* groups_y) {
    *groups_x = width / description->group_width + (width % description->group_width != 0);
    *groups_y = height / description->group_height + (height % description->group_height != 0);
}

static bool gdmf_compute_initialized  = false;
static bool gdmf_compute_failed = false;
static bool gdmf_compute_frame_ready = false;

bool gdmf_compute_prepare(uint32_t image_index) {
    gdmf_compute_frame_ready = false;
    if (gdmf_compute_failed) { return false; }
    if (!gdmf_compute_initialized) {
        if (!gdmf_compute_backend_init()) {
            gdmf_compute_failed = true;
            printf("[Compute] Initialization failed; compute remains unavailable\n");
            return false;
        }
        gdmf_compute_initialized = true;
#ifdef DEBUG
        printf("[Compute] %d kernel%s ready\n", (int)GDMF_KERNEL_COUNT,
               GDMF_KERNEL_COUNT == 1 ? "" : "s");
#endif
    }
    gdmf_compute_frame_ready = gdmf_compute_backend_begin_frame(image_index);
    return gdmf_compute_frame_ready;
}

bool gdmf_compute_ready(void) { return gdmf_compute_frame_ready; }

void gdmf_compute_on_swapchain_recreated(void) {
    gdmf_compute_frame_ready = false;
    if (gdmf_compute_initialized) {
        gdmf_compute_backend_shutdown();
        gdmf_compute_initialized = false;
    }
}

void gdmf_compute_shutdown(void) {
    gdmf_compute_frame_ready = false;
    if (gdmf_compute_initialized) {
        gdmf_compute_backend_shutdown();
    }
    gdmf_compute_initialized  = false;
    gdmf_compute_failed = false;
}
