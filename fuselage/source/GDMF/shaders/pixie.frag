#version 450
// Texture pixels already contain resolved RGBA colors.
layout(binding = 0) uniform sampler2D pixie_texture;
layout(location = 0) in vec2 fragment_uv;
layout(location = 0) out vec4 out_color;
void main() {
    out_color = texture(pixie_texture, fragment_uv);
}
