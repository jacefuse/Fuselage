#version 450

// Position already includes the CPU sprite transform and canvas projection.
layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in float in_bitmap_id;
layout(location = 3) in float in_palette;
layout(location = 4) in float in_opacity;
layout(location = 5) in float in_show_zero;
layout(location = 6) in float in_raw_grayscale;

layout(location = 0) out vec2 fragment_uv;
layout(location = 1) out flat float fragment_bitmap_id;
layout(location = 2) out flat float fragment_palette;
layout(location = 3) out float fragment_opacity;
layout(location = 4) out flat float fragment_show_zero;
layout(location = 5) out flat float fragment_raw_grayscale;

void main() {
    fragment_uv = in_uv;
    fragment_bitmap_id = in_bitmap_id;
    fragment_palette = in_palette;
    fragment_opacity = in_opacity;
    fragment_show_zero = in_show_zero;
    fragment_raw_grayscale = in_raw_grayscale;
    gl_Position = vec4(in_position, 0.0, 1.0);
}
