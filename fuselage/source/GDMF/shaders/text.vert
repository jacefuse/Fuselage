#version 450
// SEE: (GDMF_TEXT.md, Vulkan graphics resources)

layout(location = 0) out vec2 grid_coordinate;

const vec2 positions[6] = vec2[](
    vec2(-1.0,  1.0), vec2(-1.0, -1.0), vec2(1.0,  1.0),
    vec2(-1.0, -1.0), vec2(1.0,  -1.0), vec2(1.0,  1.0)
);

const vec2 grid_coordinates[6] = vec2[](
    vec2(0.0,  0.0),  vec2(0.0,  45.0), vec2(80.0, 0.0),
    vec2(0.0,  45.0), vec2(80.0, 45.0), vec2(80.0, 0.0)
);

void main() {
    gl_Position   = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    grid_coordinate = grid_coordinates[gl_VertexIndex];
}
