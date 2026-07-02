#version 450

// Pixie vertex shader. Position is fully resolved on the CPU from the
// pixie's x/y/w/h display attrs each frame -- no rotation/scale/skew to
// apply here, so this is a pure pass-through, same philosophy as the
// text/sprite/tile vertex shaders.

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 fragUV;

void main() {
    fragUV      = inUV;
    gl_Position = vec4(inPosition, 0.0, 1.0);
}
