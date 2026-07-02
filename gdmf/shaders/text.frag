#version 450

layout(location = 0) in vec2 fragGridCoord;

layout(binding = 0) uniform sampler2D atlasTexture;

// Matches gdmf_textlayer.c's TextCellData exactly (two uints, no padding
// needed in std430 -- both members are already 4-byte aligned). color is
// packed RGBA8 the same way colors.c's PackRGBA8 does (R in bits 0-7, G in
// 8-15, B in 16-23, A in 24-31), so the CPU side can just call that
// directly rather than needing a separate packing routine here.
struct CellData {
    uint character;
    uint color;
};

layout(std430, binding = 1) readonly buffer CellBuffer {
    CellData cells[];
};

layout(location = 0) out vec4 outColor;

void main() {
    ivec2 cell = ivec2(floor(fragGridCoord));
    cell.x = clamp(cell.x, 0, 79);
    cell.y = clamp(cell.y, 0, 44);

    // 0..1 within this cell -- y: 0 at the cell's screen-top edge, 1 at its
    // screen-bottom edge (same convention the old per-cell fragLocalCoord
    // used, since fragGridCoord itself is top-down, see text.vert).
    vec2 localCoord = fragGridCoord - vec2(cell);

    CellData data = cells[cell.y * 80 + cell.x];

    uint char_x = data.character % 16u;
    uint char_y = data.character / 16u;

    // Same Y-flip the old per-cell shader applied to correct for the
    // atlas's own stored orientation -- not something this rewrite changed,
    // just carried forward unchanged.
    vec2 flippedCoord = vec2(localCoord.x, 1.0 - localCoord.y);
    vec2 atlasCoord = vec2(
        (float(char_x) + flippedCoord.x) / 16.0,
        (float(char_y) + flippedCoord.y) / 16.0
    );

    vec4 cellColor = vec4(
        float((data.color >> 0)  & 0xFFu) / 255.0,
        float((data.color >> 8)  & 0xFFu) / 255.0,
        float((data.color >> 16) & 0xFFu) / 255.0,
        float((data.color >> 24) & 0xFFu) / 255.0
    );

    vec4 atlasColor = texture(atlasTexture, atlasCoord);
    outColor = vec4(cellColor.rgb * atlasColor.rgb, atlasColor.a * cellColor.a);
}
