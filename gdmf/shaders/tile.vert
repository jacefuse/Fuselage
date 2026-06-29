#version 450
#extension GL_GOOGLE_cpp_style_line_directive : enable
#extension GL_GOOGLE_include_directive : enable

// Tile vertex shader. All positioning is resolved on the CPU (tiles live on a
// regular axis-aligned grid, no per-tile rotation/skew/scale), so this shader
// is a pure pass-through -- it forwards every input attribute to the fragment
// stage unchanged. Flip is handled by the CPU when building UV coordinates, so
// no flip attribute is needed here.

layout(location = 0) out vec2  fragUV;
layout(location = 1) out float fragTileTypeID;
layout(location = 2) out float fragPalette;
layout(location = 3) out float fragTransparency;
layout(location = 4) out float fragShowZero;

layout(location = 0) in vec2  inPosition;
layout(location = 1) in vec2  inUV;
layout(location = 2) in float inTileTypeID;
layout(location = 3) in float inPalette;
layout(location = 4) in float inTransparency;
layout(location = 5) in float inShowZero;

void main() {
    gl_Position    = vec4(inPosition, 0.0, 1.0);
    fragUV         = inUV;
    fragTileTypeID = inTileTypeID;
    fragPalette    = inPalette;
    fragTransparency = inTransparency;
    fragShowZero   = inShowZero;
}
