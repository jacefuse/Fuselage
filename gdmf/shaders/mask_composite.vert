#version 450

// Masked-band composite vertex shader. A fullscreen triangle emitted from
// gl_VertexIndex alone -- no vertex buffer is bound (vkCmdDraw(cmd, 3, ...)).
// fragUV runs 0..1 across the whole framebuffer so the fragment stage can
// sample the pre-rendered offscreen band target 1:1. Same pure-pass-through
// spirit as every other GDMF vertex shader, just with CPU-free geometry.
//
// uv:  (0,0) (2,0) (0,2) -> covers the [0,1] square with one oversized tri.
// pos: uv*2-1 puts (0,0) at NDC (-1,-1) = framebuffer top-left in Vulkan.

layout(location = 0) out vec2 fragUV;

void main() {
    fragUV      = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(fragUV * 2.0 - 1.0, 0.0, 1.0);
}
