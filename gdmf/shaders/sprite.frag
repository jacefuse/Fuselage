#version 450

layout(location = 0) in vec2       fragUV;
layout(location = 1) in flat float fragBitmapID;
layout(location = 2) in flat float fragPalette;
layout(location = 3) in float      fragTransparency;
layout(location = 4) in flat float fragShowZero;
layout(location = 5) in flat float fragRawGrayscale;

layout(binding = 0) uniform usampler2DArray atlasTexture;

// 256 palettes x 16 colors, RGBA8 packed into a uint per color (matches the
// CPU-side Color{r,g,b,a} struct's in-memory layout exactly, byte for byte).
layout(std430, binding = 1) readonly buffer PaletteBuffer {
    uint packedColors[4096];
};

layout(location = 0) out vec4 outColor;

void main() {
    uint colorIndex = texture(atlasTexture, vec3(fragUV, fragBitmapID)).r;

    // Palette index 0 is conventionally "background" -- skip it unless the
    // sprite explicitly opted in via showzero.
    if (colorIndex == 0u && fragShowZero < 0.5) {
        discard;
    }

    vec4 finalColor;
    if (fragRawGrayscale > 0.5) {
        // Atlas debug view: the atlas stores no color of its own, so show
        // the raw index directly instead of looking it up in any palette --
        // intrinsic to this draw, not dependent on what any palette slot
        // happens to currently hold.
        float gray = float(colorIndex) / 15.0;
        finalColor = vec4(gray, gray, gray, 1.0);
    } else {
        uint packed = packedColors[uint(fragPalette) * 16u + colorIndex];
        finalColor = vec4(
            float(packed & 0xFFu) / 255.0,
            float((packed >> 8) & 0xFFu) / 255.0,
            float((packed >> 16) & 0xFFu) / 255.0,
            float((packed >> 24) & 0xFFu) / 255.0
        );
    }

    outColor = vec4(finalColor.rgb, finalColor.a * fragTransparency);
}
