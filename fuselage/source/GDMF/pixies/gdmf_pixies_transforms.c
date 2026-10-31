// Affine registers and arithmetic. SEE: (GDMF_PIXIES_COMMANDS.md, Transforms)
#include "gdmf_pixies_state_private.h"
#include <math.h>

const PixieTransform gdmf_pixies_identity_transform = {1, 0, 0, 1, 0, 0};

static gdmf_pixies_state* gdmf_pixies_writable_transform(int id, int slot) {
    return slot > 0 && slot < PIXIE_TRANSFORM_SLOTS ? gdmf_pixies_state_get(id) : NULL;
}

bool SetPixieTransform(int id, int slot, float translation_x, float translation_y, float scale_x,
                       float scale_y, float rotation_degrees) {
    gdmf_pixies_state* state = gdmf_pixies_writable_transform(id, slot);
    if (!state) {
        return false;
    }
    float radians = rotation_degrees * 0.01745329251994329577f;
    float cosine = cosf(radians), sine = sinf(radians);
    state->transforms[slot] = (PixieTransform){cosine * scale_x, -sine * scale_y, sine * scale_x,
                                               cosine * scale_y, translation_x,   translation_y};
    return true;
}

bool SetPixieTransformMatrix(int id, int slot, const PixieTransform* matrix) {
    gdmf_pixies_state* state = gdmf_pixies_writable_transform(id, slot);
    if (!state || !matrix) {
        return false;
    }
    state->transforms[slot] = *matrix;
    return true;
}

bool GetPixieTransformMatrix(int id, int slot, PixieTransform* output) {
    const gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    if (!state || slot < 0 || slot >= PIXIE_TRANSFORM_SLOTS || !output) {
        return false;
    }
    *output = state->transforms[slot];
    return true;
}

bool ResetPixieTransform(int id, int slot) {
    return SetPixieTransformMatrix(id, slot, &gdmf_pixies_identity_transform);
}

bool SetPixieCurrentTransform(int id, int slot) {
    uint32_t arguments[4] = {(uint32_t)slot, 0, 0, 0};
    return IssuePixieCommand(id, PIXIE_OP_SET_TRANSFORM, 0, arguments);
}

PixieTransform gdmf_pixies_transform_compose(const PixieTransform* outer, const PixieTransform* inner) {
    PixieTransform result;
    result.a = outer->a * inner->a + outer->b * inner->c;
    result.b = outer->a * inner->b + outer->b * inner->d;
    result.c = outer->c * inner->a + outer->d * inner->c;
    result.d = outer->c * inner->b + outer->d * inner->d;
    result.tx = outer->a * inner->tx + outer->b * inner->ty + outer->tx;
    result.ty = outer->c * inner->tx + outer->d * inner->ty + outer->ty;
    return result;
}

void gdmf_pixies_transform_apply(const PixieTransform* transform, float* coordinate_x, float* coordinate_y) {
    if (transform->a == 1 && transform->b == 0 && transform->c == 0 && transform->d == 1 &&
        transform->tx == 0 && transform->ty == 0) {
        return;
    }
    float input_x = *coordinate_x, input_y = *coordinate_y;
    *coordinate_x = transform->a * input_x + transform->b * input_y + transform->tx;
    *coordinate_y = transform->c * input_x + transform->d * input_y + transform->ty;
}
