// Logical lifetime and RAM. SEE: (GDMF_PIXIES.md, Host state)
#include "gdmf_pixies_state_private.h"
#include "gdmf_pixies_backend.h"
#include "gdmf_pixies_decode_queue.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gdmf_pixies_state gdmf_pixies_states[MAX_PIXIES];

gdmf_pixies_state* gdmf_pixies_state_get(int id) {
    if (id < 0 || id >= MAX_PIXIES || !gdmf_pixies_states[id].initialized) {
        return NULL;
    }
    return &gdmf_pixies_states[id];
}

bool InitPixie(int id, PixieMode mode, int output_width, int output_height) {
    if (id < 0 || id >= MAX_PIXIES || output_width <= 0 || output_height <= 0 ||
        (mode != PIXIE_MODE_TEXTURE && mode != PIXIE_MODE_LIVE && mode != PIXIE_MODE_MASK)) {
        return false;
    }
    if ((size_t)output_width > SIZE_MAX / (size_t)output_height) {
        return false;
    }
    size_t pixel_count = (size_t)output_width * (size_t)output_height;
    // Existing drawing paths use signed integer pixel indices.
    if (pixel_count > INT_MAX || (mode == PIXIE_MODE_TEXTURE && pixel_count > SIZE_MAX / sizeof(Color))) {
        return false;
    }
    if (gdmf_pixies_states[id].initialized && !ReleasePixie(id)) {
        return false;
    }

    gdmf_pixies_state pending = {0};
    pending.ram = calloc(1, PIXIE_RAM_SIZE);
    if (!pending.ram) {
        return false;
    }
    if (mode == PIXIE_MODE_TEXTURE) {
        pending.output = calloc(pixel_count, sizeof(Color));
        if (!pending.output) {
            goto fail;
        }
    } else if (mode == PIXIE_MODE_MASK) {
        pending.mask = calloc(pixel_count, 1);
        if (!pending.mask) {
            goto fail;
        }
    } else {
        pending.live_vertices = calloc(GDMF_PIXIES_LIVE_MAX_VERTICES, sizeof(gdmf_pixies_live_vertex));
        if (!pending.live_vertices) {
            goto fail;
        }
        pending.live_vertex_capacity = GDMF_PIXIES_LIVE_MAX_VERTICES;
    }
    pending.mode = mode;
    pending.output_width = output_width;
    pending.output_height = output_height;
    pending.display_width = (float)output_width;
    pending.display_height = (float)output_height;
    pending.dirty = mode != PIXIE_MODE_LIVE;
    pending.draw_pattern = 0xFFFF;
    for (int slot = 0; slot < PIXIE_TRANSFORM_SLOTS; ++slot) {
        pending.transforms[slot] = gdmf_pixies_identity_transform;
    }
    pending.initialized = true;
    gdmf_pixies_states[id] = pending;
    return true;
fail:
    free(pending.ram);
    free(pending.output);
    free(pending.mask);
    free(pending.live_vertices);
    return false;
}

bool ReleasePixie(int id) {
    gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    if (!state) {
        return false;
    }
    // A failed backend handoff must not erase the state needed to retry release.
    if (!gdmf_pixies_backend_release(id)) {
        return false;
    }
    gdmf_pixies_decode_requests_drop(id);
    free(state->ram);
    free(state->output);
    free(state->mask);
    free(state->live_vertices);
    memset(state, 0, sizeof(*state));
    return true;
}

void ShutdownPixies(void) {
    bool released = true;
    for (int id = 0; id < MAX_PIXIES; ++id) {
        if (gdmf_pixies_state_get(id) && !ReleasePixie(id)) {
            released = false;
        }
    }
    if (!released) {
        fprintf(stderr, "[Pixies] Shutdown retained pixies whose backend release failed\n");
        return;
    }
    if (!gdmf_pixies_backend_shutdown()) {
        fprintf(stderr, "[Pixies] Shutdown retained shared backend resources\n");
        return;
    }
    gdmf_pixies_decode_requests_shutdown();
}

bool WritePixieRAM(int id, size_t offset, const void* data, size_t size) {
    gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    if (!state || !data || offset > PIXIE_RAM_SIZE || size > PIXIE_RAM_SIZE - offset) {
        return false;
    }
    memmove(state->ram + offset, data, size);
    return true;
}

size_t GetPixieRAMSize(int id) {
    return gdmf_pixies_state_get(id) ? PIXIE_RAM_SIZE : 0;
}

const void* GetPixieRAMReadPtr(int id) {
    const gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    return state ? state->ram : NULL;
}
