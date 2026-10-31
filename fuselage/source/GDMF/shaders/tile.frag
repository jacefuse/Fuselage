#version 450
#extension GL_GOOGLE_cpp_style_line_directive : enable
#extension GL_GOOGLE_include_directive : enable

// Indexed atlas colors; opacity arrives in 0..255 units.
layout(set = 0, binding = 0) uniform usampler2DArray atlas_texture;

layout(set = 0, binding = 1) readonly buffer PaletteBuffer {
    uint packed_colors[256 * 16];
}
palette;

layout(location = 0) in vec2 fragment_uv;
layout(location = 1) in flat float fragment_bitmap_id;
layout(location = 2) in flat float fragment_palette;
layout(location = 3) in float fragment_opacity;
layout(location = 4) in flat float fragment_show_zero;

layout(location = 0) out vec4 output_color;

void main() {
    uint color_index = texture(atlas_texture, vec3(fragment_uv, fragment_bitmap_id)).r;

    if (color_index == 0u && fragment_show_zero < 0.5) {
        discard;
    }

    uint palette_base = uint(fragment_palette) * 16u;
    uint packed_color = palette.packed_colors[palette_base + color_index];

    float red = float((packed_color) & 0xFFu) / 255.0;
    float green = float((packed_color >> 8) & 0xFFu) / 255.0;
    float blue = float((packed_color >> 16) & 0xFFu) / 255.0;
    float alpha = float((packed_color >> 24) & 0xFFu) / 255.0;

    alpha *= fragment_opacity / 255.0;
    output_color = vec4(red, green, blue, alpha);
}
