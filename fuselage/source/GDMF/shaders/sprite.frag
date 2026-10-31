#version 450

layout(location = 0) in vec2 fragment_uv;
layout(location = 1) in flat float fragment_bitmap_id;
layout(location = 2) in flat float fragment_palette;
layout(location = 3) in float fragment_opacity;
layout(location = 4) in flat float fragment_show_zero;
layout(location = 5) in flat float fragment_raw_grayscale;

layout(binding = 0) uniform usampler2DArray atlas_texture;
layout(std430, binding = 1) readonly buffer PaletteBuffer {
    uint packed_colors[4096];
};
layout(location = 0) out vec4 output_color;

void main() {
    uint color_index = texture(atlas_texture, vec3(fragment_uv, fragment_bitmap_id)).r;
    // Source index zero stays hidden unless explicitly enabled.
    if (color_index == 0u && fragment_show_zero < 0.5) {
        discard;
    }
    vec4 final_color;
    if (fragment_raw_grayscale > 0.5) {
        // Retained shader mode; the atlas preview API does not currently enable it.
        float gray = float(color_index) / 15.0;
        final_color = vec4(gray, gray, gray, 1.0);
    } else {
        uint packed = packed_colors[uint(fragment_palette) * 16u + color_index];
        final_color = vec4(
            float(packed & 0xFFu) / 255.0,
            float((packed >> 8) & 0xFFu) / 255.0,
            float((packed >> 16) & 0xFFu) / 255.0,
            float((packed >> 24) & 0xFFu) / 255.0
        );
    }
    output_color = vec4(final_color.rgb, final_color.a * fragment_opacity);
}
