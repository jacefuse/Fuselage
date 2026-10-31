#version 450

// Fullscreen triangle generated from gl_VertexIndex; no vertex buffer.
// SEE: (GDMF_VULKAN.md, Mask composite shaders).
layout(location = 0) out vec2 texture_coordinates;

void main() {
    texture_coordinates = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(texture_coordinates * 2.0 - 1.0, 0.0, 1.0);
}
