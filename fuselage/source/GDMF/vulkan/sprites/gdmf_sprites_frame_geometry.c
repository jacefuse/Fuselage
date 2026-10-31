#include "gdmf_sprites_frame.h"
#include "../../sprites/gdmf_sprites_geometry.h"
#include <math.h>
#include <string.h>

bool gdmf_sprites_frame_quad(const Sprite* sprite, float canvas_width, float canvas_height,
                             gdmf_sprite_vertex output[6]) {
    if (!sprite || !output || !isfinite(canvas_width) || !isfinite(canvas_height) || canvas_width <= 0.0f ||
        canvas_height <= 0.0f) {
        return false;
    }
    float projection_x = 2.0f / canvas_width, projection_y = 2.0f / canvas_height;
    float world_x[4], world_y[4];
    gdmf_sprites_world_quad(sprite, world_x, world_y);
    float texture_x[4] = {0.0f, 1.0f, 0.0f, 1.0f};
    float texture_y[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    float projected_x[4], projected_y[4];
    for (unsigned corner = 0; corner < 4; ++corner) {
        projected_x[corner] = world_x[corner] * projection_x - 1.0f;
        projected_y[corner] = world_y[corner] * projection_y - 1.0f;
        if (!isfinite(projected_x[corner]) || !isfinite(projected_y[corner])) {
            return false;
        }
        if (sprite->flip & SPRITE_FLIP_X) {
            texture_x[corner] = 1.0f - texture_x[corner];
        }
        if (sprite->flip & SPRITE_FLIP_Y) {
            texture_y[corner] = 1.0f - texture_y[corner];
        }
    }
    static const unsigned corners[6] = {0, 2, 1, 2, 3, 1};
    gdmf_sprite_vertex pending[6];
    for (unsigned vertex = 0; vertex < 6; ++vertex) {
        unsigned corner = corners[vertex];
        pending[vertex] = (gdmf_sprite_vertex){{projected_x[corner], projected_y[corner]},
                                               {texture_x[corner], texture_y[corner]},
                                               (float)sprite->bitmapID,
                                               (float)sprite->palette,
                                               sprite->transparency / 255.0f,
                                               sprite->showzero ? 1.0f : 0.0f,
                                               0.0f};
    }
    memcpy(output, pending, sizeof(pending));
    return true;
}
