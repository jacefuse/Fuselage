#version 450

// Mode 1 ("live") pixie fragment shader. No texture, no sampler, no
// descriptor set at all -- unlike Mode 0, there is nothing to sample.
// Every primitive already carries its final color from the CPU side.

layout(location = 0) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = fragColor;
}
