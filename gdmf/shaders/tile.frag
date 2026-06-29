#version 450
#extension GL_GOOGLE_cpp_style_line_directive : enable
#extension GL_GOOGLE_include_directive : enable

// Tile fragment shader. Samples the per-layer tile atlas (one array layer per
// tile type slot, raw palette index per pixel) and resolves the index through
// the packed Colors palette buffer -- same lookup as the sprite shader, so
// both layers always agree on what a given palette entry looks like. Index 0
// is discarded unless fragShowZero is set, making tile backgrounds transparent
// by default without needing a separate alpha channel in the atlas.

layout(set = 0, binding = 0) uniform usampler2DArray atlasTexture;

layout(set = 0, binding = 1) readonly buffer PaletteBuffer {
    uint packedColors[256 * 16];
} palette;

layout(location = 0) in vec2  fragUV;
layout(location = 1) in float fragTileTypeID;
layout(location = 2) in float fragPalette;
layout(location = 3) in float fragTransparency;
layout(location = 4) in float fragShowZero;

layout(location = 0) out vec4 outColor;

void main() {
    uint colorIndex = texture(atlasTexture, vec3(fragUV, fragTileTypeID)).r;

    if (colorIndex == 0u && fragShowZero < 0.5) discard;

    uint  paletteBase = uint(fragPalette) * 16u;
    uint  packed      = palette.packedColors[paletteBase + colorIndex];

    float r = float((packed      ) & 0xFFu) / 255.0;
    float g = float((packed >>  8) & 0xFFu) / 255.0;
    float b = float((packed >> 16) & 0xFFu) / 255.0;
    float a = float((packed >> 24) & 0xFFu) / 255.0;

    a *= fragTransparency / 255.0;
    outColor = vec4(r, g, b, a);
}
