// UNPACK validation and diagnostics. SEE: (GDMF_PIXIES_FORMATS.md, Color headers and payloads)
#include "gdmf_pixies_drawing_private.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "gdmf_pixies_decode_private.h"

bool gdmf_pixies_unpack_command(int id, gdmf_pixies_state* state, uint16_t flags, const uint32_t args[4]) {

    if (!args) {
        return gdmf_pixies_refuse(id, "UNPACK: args is NULL");
    }

    if (state->mode == PIXIE_MODE_MASK) {
        if (flags > PIXIE_MASK_FORMAT_MAX) {
            return gdmf_pixies_refuse(id, "UNPACK: MASK format %u unknown (0..%d)", (unsigned)flags,
                                      (int)PIXIE_MASK_FORMAT_MAX);
        }
        if (flags == PIXIE_MASK_FORMAT_CTXTILE) {
            if (!gdmf_pixies_unpack_mask_ctxtile(state, (size_t)args[0])) {
                return gdmf_pixies_refuse(id,
                                          "UNPACK: malformed CTXTILE mask image at "
                                          "RAM offset %u (header, probability table "
                                          "or tile list out of range)",
                                          (unsigned)args[0]);
            }
            return true;
        }

        size_t mask_bytes = (size_t)state->output_width * (size_t)state->output_height;
        size_t offset = (size_t)args[0];
        if (offset > PIXIE_RAM_SIZE || mask_bytes > PIXIE_RAM_SIZE - offset) {
            return gdmf_pixies_refuse(id,
                                      "UNPACK: %zu mask bytes at RAM offset %zu "
                                      "runs past the %u-byte RAM",
                                      mask_bytes, offset, (unsigned)PIXIE_RAM_SIZE);
        }
        memcpy(state->mask, state->ram + offset, mask_bytes);
        state->dirty = true;
        return true;
    }
    if (state->mode != PIXIE_MODE_TEXTURE) {

        return gdmf_pixies_refuse(id,
                                  "UNPACK: mode %d has no surface to unpack into "
                                  "(LIVE draws straight to the GPU with no texture "
                                  "sampling; use TEXTURE or MASK)",
                                  (int)state->mode);
    }
    if (flags > PIXIE_FORMAT_MAX) {
        return gdmf_pixies_refuse(id, "UNPACK: format %u unknown (0..%d)", (unsigned)flags,
                                  (int)PIXIE_FORMAT_MAX);
    }
    int destination_x, destination_y;
    gdmf_pixies_unpack_xy(args[1], &destination_x, &destination_y);
    unsigned char palette_index = (unsigned char)(args[2] & 0xFFu);

    bool kernel_tier = (flags == PIXIE_FORMAT_RGBA8 || flags == PIXIE_FORMAT_PALETTE4BPP ||
                        flags == PIXIE_FORMAT_PALETTE8BPP) ||
                       gdmf_pixies_image_has_row_table(state, (size_t)args[0]);
    if (state->gpu_resident && !kernel_tier) {
        return gdmf_pixies_refuse(id,
                                  "UNPACK: pixie %d is GPU-resident and format %u is "
                                  "decoded on the host; only the raw tiers and a "
                                  "row-tabled RLE image may be unpacked into it",
                                  id, (unsigned)flags);
    }
    bool accepted = gdmf_pixies_unpack_packed(state, (size_t)args[0], (PixieUnpackFormat)flags, destination_x,
                                              destination_y, palette_index);
    if (!accepted && state->gpu_resident) {
        return gdmf_pixies_refuse(id,
                                  "UNPACK: pixie %d is GPU-resident and the decode could "
                                  "not be queued (malformed image, or the kernel queue "
                                  "is full)",
                                  id);
    }

    if (!accepted) {
        size_t offset = (size_t)args[0];
        if (offset > PIXIE_RAM_SIZE - 8) {
            return gdmf_pixies_refuse(id,
                                      "UNPACK: image offset %zu is past the end of the "
                                      "%u-byte RAM",
                                      offset, (unsigned)PIXIE_RAM_SIZE);
        }
        const unsigned char* h = state->ram + offset;
        return gdmf_pixies_refuse(id,
                                  "UNPACK: format %u refused the image at offset %zu; "
                                  "header read as %ux%u, %u pass(es) [%u %u %u] (u16 w, "
                                  "u16 h, u8 passCount, passes). Malformed header, a pass "
                                  "the format cannot take, or a short body",
                                  (unsigned)flags, offset, (unsigned)(h[0] | (h[1] << 8)),
                                  (unsigned)(h[2] | (h[3] << 8)), (unsigned)h[4], (unsigned)h[5],
                                  (unsigned)h[6], (unsigned)h[7]);
    }
    return accepted;
}
