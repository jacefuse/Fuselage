#version 450

// inPosition arrives pre-transformed to NDC -- rotation/scale/skew are
// resolved on the CPU per sprite, same philosophy as the text layer.
layout(location = 0) in vec2  inPosition;
layout(location = 1) in vec2  inUV;              // 0..1 within this sprite's atlas layer
layout(location = 2) in float inBitmapID;        // atlas array layer
layout(location = 3) in float inPalette;         // which of the 256 16-color palettes
layout(location = 4) in float inTransparency;    // 0..1 alpha multiplier
layout(location = 5) in float inShowZero;        // 0 or 1
layout(location = 6) in float inRawGrayscale;    // 0 or 1 -- bypass the palette entirely (atlas debug view)

layout(location = 0) out vec2       fragUV;
layout(location = 1) out flat float fragBitmapID;
layout(location = 2) out flat float fragPalette;
layout(location = 3) out float      fragTransparency;
layout(location = 4) out flat float fragShowZero;
layout(location = 5) out flat float fragRawGrayscale;

void main() {
    fragUV            = inUV;
    fragBitmapID      = inBitmapID;
    fragPalette       = inPalette;
    fragTransparency  = inTransparency;
    fragShowZero      = inShowZero;
    fragRawGrayscale  = inRawGrayscale;

    gl_Position = vec4(inPosition, 0.0, 1.0);
}
