// Attributes and command wrappers. SEE: (GDMF_PIXIES.md, Host state)
#include "gdmf_pixies_state_private.h"
#include "gdmf_pixies_backend.h"

bool SetPixiePatternBitmap(int id, uint32_t ram_offset, uint16_t width, uint16_t height) {
    gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    if (!state || !width || !height || (size_t)width > SIZE_MAX / height / sizeof(Color)) {
        return false;
    }
    size_t bytes = (size_t)width * height * sizeof(Color);
    if (ram_offset > PIXIE_RAM_SIZE || bytes > PIXIE_RAM_SIZE - ram_offset) {
        return false;
    }
    state->pattern_offset = ram_offset;
    state->pattern_width = width;
    state->pattern_height = height;
    return true;
}

bool ClearPixiePatternBitmap(int id) {
    gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    if (!state) {
        return false;
    }
    state->pattern_width = state->pattern_height = 0;
    return true;
}

bool SetPixieRotation(int id, float radians) {
    gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    if (!state) {
        return false;
    }
    if (state->mode == PIXIE_MODE_MASK) {
        return gdmf_pixies_refuse(id, "SetPixieRotation: masks require an axis-aligned rectangle");
    }
    state->rotation_radians = radians;
    return true;
}

bool SetPixieHotspot(int id, float hotspot_x, float hotspot_y) {
    gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    if (!state) {
        return false;
    }
    state->hotspot_x = hotspot_x;
    state->hotspot_y = hotspot_y;
    return true;
}

bool SetPixieRotateAroundHotspot(int id, bool enabled) {
    gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    if (!state) {
        return false;
    }
    state->rotate_around_hotspot = enabled;
    return true;
}

bool SetPixieFilterLinear(int id, bool enabled) {
    gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    if (!state) {
        return false;
    }
    if (enabled && !gdmf_pixies_backend_linear_filter_available()) {
        state->linear_filter = false;
        return false;
    }
    state->linear_filter = enabled;
    return true;
}

bool SetPixieMaskInvert(int id, bool invert) {
    gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    if (!state || state->mode != PIXIE_MODE_MASK) {
        return false;
    }
    state->mask_inverted = invert;
    return true;
}

bool GetPixieMaskInvert(int id) {
    const gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    return state && state->mode == PIXIE_MODE_MASK ? state->mask_inverted : false;
}

static uint16_t gdmf_pixies_current_attr_flags(int id) {
    return (uint16_t)(GetPixiePriority(id) | (GetPixieEnabled(id) ? GDMF_PIXIES_SET_ATTR_ENABLED_BIT : 0u));
}

bool SetPixiePosition(int id, float x, float y) {
    uint32_t arguments[4] = {PackPixieFloatArg(x), PackPixieFloatArg(y),
                             PackPixieFloatArg(GetPixieDisplayWidth(id)),
                             PackPixieFloatArg(GetPixieDisplayHeight(id))};

    return IssuePixieCommand(id, PIXIE_OP_SET_ATTR, gdmf_pixies_current_attr_flags(id), arguments);
}

bool SetPixieDisplaySize(int id, float width, float height) {
    uint32_t arguments[4] = {PackPixieFloatArg(GetPixieX(id)), PackPixieFloatArg(GetPixieY(id)),
                             PackPixieFloatArg(width), PackPixieFloatArg(height)};

    return IssuePixieCommand(id, PIXIE_OP_SET_ATTR, gdmf_pixies_current_attr_flags(id), arguments);
}

bool SetPixiePriority(int id, unsigned char priority) {
    uint32_t arguments[4] = {PackPixieFloatArg(GetPixieX(id)), PackPixieFloatArg(GetPixieY(id)),
                             PackPixieFloatArg(GetPixieDisplayWidth(id)),
                             PackPixieFloatArg(GetPixieDisplayHeight(id))};
    uint16_t flags = (uint16_t)(priority | (GetPixieEnabled(id) ? GDMF_PIXIES_SET_ATTR_ENABLED_BIT : 0u));
    return IssuePixieCommand(id, PIXIE_OP_SET_ATTR, flags, arguments);
}

bool SetPixieEnabled(int id, bool enabled) {
    uint32_t arguments[4] = {PackPixieFloatArg(GetPixieX(id)), PackPixieFloatArg(GetPixieY(id)),
                             PackPixieFloatArg(GetPixieDisplayWidth(id)),
                             PackPixieFloatArg(GetPixieDisplayHeight(id))};
    uint16_t flags = (uint16_t)(GetPixiePriority(id) | (enabled ? GDMF_PIXIES_SET_ATTR_ENABLED_BIT : 0u));
    return IssuePixieCommand(id, PIXIE_OP_SET_ATTR, flags, arguments);
}

bool ShowPixie(int id) {
    return IssuePixieCommand(id, PIXIE_OP_SHOW, 0, NULL);
}

bool HidePixie(int id) {
    return IssuePixieCommand(id, PIXIE_OP_HIDE, 0, NULL);
}

bool SetPixieDrawPattern(int id, uint16_t pattern) {
    uint32_t arguments[4] = {(uint32_t)pattern, 0, 0, 0};

    return IssuePixieCommand(id, PIXIE_OP_SET_DRAW_PATTERN, 0, arguments);
}

bool GetPixieInitialized(int id) {
    return gdmf_pixies_state_get(id) != NULL;
}

PixieMode GetPixieMode(int id) {
    const gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    return state ? state->mode : PIXIE_MODE_TEXTURE;
}

int GetPixieOutputWidth(int id) {
    const gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    return state ? state->output_width : 0;
}

int GetPixieOutputHeight(int id) {
    const gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    return state ? state->output_height : 0;
}

float GetPixieX(int id) {
    const gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    return state ? state->position_x : 0.0f;
}

float GetPixieY(int id) {
    const gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    return state ? state->position_y : 0.0f;
}

float GetPixieDisplayWidth(int id) {
    const gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    return state ? state->display_width : 0.0f;
}

float GetPixieDisplayHeight(int id) {
    const gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    return state ? state->display_height : 0.0f;
}

unsigned char GetPixiePriority(int id) {
    const gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    return state ? state->priority : 0;
}

bool GetPixieEnabled(int id) {
    const gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    return state ? state->enabled : false;
}

bool GetPixieShown(int id) {
    const gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    return state ? state->shown : false;
}

uint16_t GetPixieDrawPattern(int id) {
    const gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    return state ? state->draw_pattern : 0xFFFF;
}

float GetPixieRotation(int id) {
    const gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    return state ? state->rotation_radians : 0.0f;
}

float GetPixieHotspotX(int id) {
    const gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    return state ? state->hotspot_x : 0.0f;
}

float GetPixieHotspotY(int id) {
    const gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    return state ? state->hotspot_y : 0.0f;
}

bool GetPixieRotateAroundHotspot(int id) {
    const gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    return state ? state->rotate_around_hotspot : false;
}

bool GetPixieFilterLinear(int id) {
    const gdmf_pixies_state* state = gdmf_pixies_state_get(id);
    return state ? state->linear_filter : false;
}
