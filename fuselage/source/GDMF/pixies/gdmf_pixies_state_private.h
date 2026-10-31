#ifndef GDMF_PIXIES_STATE_PRIVATE_H
#define GDMF_PIXIES_STATE_PRIVATE_H

// Logical Pixies state shared by host implementation modules, under the caller's
// state lock. Vulkan handles stay with the backend. SEE: (GDMF_PIXIES.md, Host state)
#include "gdmf_pixies.h"
#include "../gdmf_colors.h"

#define GDMF_PIXIES_LIVE_MAX_PRIMITIVES 4096
#define GDMF_PIXIES_LIVE_MAX_VERTICES (GDMF_PIXIES_LIVE_MAX_PRIMITIVES * 6)
#define GDMF_PIXIES_SET_ATTR_ENABLED_BIT 0x0100u

typedef struct {
    float position[2];
    float color[4];
} gdmf_pixies_live_vertex;

typedef struct {
    bool initialized;
    PixieMode mode;
    unsigned char* ram;
    Color* output;
    uint8_t* mask;
    int output_width, output_height;
    float position_x, position_y;
    float display_width, display_height;
    float rotation_radians;
    float hotspot_x, hotspot_y;
    bool rotate_around_hotspot;
    bool linear_filter;
    uint8_t priority;
    bool enabled, shown, dirty, gpu_resident;
    uint16_t draw_pattern;
    uint32_t pattern_offset;
    uint16_t pattern_width, pattern_height;
    bool mask_inverted;
    gdmf_pixies_live_vertex* live_vertices;
    int live_vertex_count, live_vertex_capacity;
    PixieTransform transforms[PIXIE_TRANSFORM_SLOTS];
    char diagnostic[PIXIE_OUTPUT_STRING_LEN];
} gdmf_pixies_state;

extern const PixieTransform gdmf_pixies_identity_transform;

// Borrowed state of an initialized ID, otherwise NULL. Do not retain over release.
gdmf_pixies_state* gdmf_pixies_state_get(int id);
char* gdmf_pixies_diagnostic_buffer(int id);
bool gdmf_pixies_refuse(int id, const char* format, ...);

PixieTransform gdmf_pixies_transform_compose(const PixieTransform* outer, const PixieTransform* inner);
void gdmf_pixies_transform_apply(const PixieTransform* transform, float* coordinate_x, float* coordinate_y);

#endif
