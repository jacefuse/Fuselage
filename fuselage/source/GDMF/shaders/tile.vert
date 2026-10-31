#version 450
#extension GL_GOOGLE_cpp_style_line_directive : enable
#extension GL_GOOGLE_include_directive : enable

// CPU-resolved geometry; discrete selectors stay flat.
layout(location = 0) out vec2 fragment_uv;
layout(location = 1) out flat float fragment_bitmap_id;
layout(location = 2) out flat float fragment_palette;
layout(location = 3) out float fragment_opacity;
layout(location = 4) out flat float fragment_show_zero;

layout(location = 0) in vec2 input_position;
layout(location = 1) in vec2 input_uv;
layout(location = 2) in float input_bitmap_id;
layout(location = 3) in float input_palette;
layout(location = 4) in float input_opacity;
layout(location = 5) in float input_show_zero;

void main() {
    gl_Position = vec4(input_position, 0.0, 1.0);
    fragment_uv = input_uv;
    fragment_bitmap_id = input_bitmap_id;
    fragment_palette = input_palette;
    fragment_opacity = input_opacity;
    fragment_show_zero = input_show_zero;
}
