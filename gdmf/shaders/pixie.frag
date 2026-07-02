#version 450

// Pixie fragment shader. Unlike sprites/tiles, a pixie's output buffer is
// already true RGBA -- no palette index to resolve, no atlas array (each
// pixie owns exactly one image, not a shared set of slots), no
// index-0-transparent convention. Alpha comes straight from whatever the
// pixie wrote into its own output buffer (PIXIE_OP_CLEAR/PLOT/DRAW/etc.),
// so normal alpha blending applies with no special-casing here.

layout(binding = 0) uniform sampler2D pixieTexture;

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(pixieTexture, fragUV);
}
