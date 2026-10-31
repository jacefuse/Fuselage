#ifndef GDMF_SPRITES_PROBE_H
#define GDMF_SPRITES_PROBE_H

#include <stdbool.h>

// Internal transform snapshot with borrowed packed bitmap storage. Use within
// one serialized query; upload/reset can invalidate its contents.
// SEE: (GDMF_SPRITES.md, Point sampling)
typedef struct {
    const unsigned char* bitmap;
    float pivot_x, pivot_y;
    float center_offset_x, center_offset_y;
    float cosine, sine;
    float skew_x, skew_y, determinant;
    float scale_x, scale_y;
    bool invertible;
    bool flip_x, flip_y;
} gdmf_sprite_probe;

// False for invalid index/NULL output. A valid index can produce a noninvertible probe.
bool gdmf_sprite_probe_init(int sprite_index, gdmf_sprite_probe* probe);
// False leaves outputs untouched. All pointers must be non-NULL.
bool gdmf_sprite_probe_local(const gdmf_sprite_probe* probe, float world_x, float world_y, int* local_x,
                             int* local_y);
// Requires registered bitmap and valid local coordinates; no transform is applied.
unsigned char gdmf_sprite_probe_sample(const gdmf_sprite_probe* probe, int local_x, int local_y);
// Palette index 0..15 or -1 when sampling is unavailable.
int gdmf_sprite_probe_pixel(const gdmf_sprite_probe* probe, float world_x, float world_y);
// Candidate integer x range [first_x, end_x) for one world row. May be wider than
// accepted samples; not an exact footprint. Requires initialized probe/non-NULL outputs.
// Invalid/noninvertible probes or empty ranges return false without output writes.
bool gdmf_sprite_probe_row_span(const gdmf_sprite_probe* probe, int world_y, int* first_x, int* end_x);

#endif
