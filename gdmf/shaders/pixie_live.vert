#version 450

// Mode 1 ("live") pixie vertex shader. Position and color are both
// already fully resolved on the CPU (see pixie_live_push_quad/
// pixie_live_push_line in gdmf_pixies.c) -- this is a pure pass-through,
// same philosophy as every other GDMF vertex shader.

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 fragColor;

void main() {
    fragColor   = inColor;
    gl_Position = vec4(inPosition, 0.0, 1.0);
}
