#version 450
// SEE: (GDMF_TEXT.md, Vulkan graphics resources)

layout(location = 0) in vec2 grid_coordinate;

layout(binding = 0) uniform sampler2D atlas_texture;

struct CellData {
    uint character;
    uint color;
};

layout(std430, binding = 1) readonly buffer CellBuffer {
    CellData cells[];
};

layout(location = 0) out vec4 output_color;

void main() {
    ivec2 cell = ivec2(floor(grid_coordinate));
    cell.x = clamp(cell.x, 0, 79);
    cell.y = clamp(cell.y, 0, 44);

    vec2 local_coordinate = grid_coordinate - vec2(cell);

    CellData cell_data = cells[cell.y * 80 + cell.x];

    uint glyph_column = cell_data.character % 16u;
    uint glyph_row = cell_data.character / 16u;

    vec2 flipped_coordinate = vec2(local_coordinate.x, 1.0 - local_coordinate.y);
    vec2 atlas_coordinate = vec2(
        (float(glyph_column) + flipped_coordinate.x) / 16.0,
        (float(glyph_row) + flipped_coordinate.y) / 16.0
    );

    vec4 cell_color = vec4(
        float((cell_data.color >> 0)  & 0xFFu) / 255.0,
        float((cell_data.color >> 8)  & 0xFFu) / 255.0,
        float((cell_data.color >> 16) & 0xFFu) / 255.0,
        float((cell_data.color >> 24) & 0xFFu) / 255.0
    );

    vec4 atlas_color = texture(atlas_texture, atlas_coordinate);
    output_color = vec4(cell_color.rgb * atlas_color.rgb, atlas_color.a * cell_color.a);
}
