#version 450
// Live geometry retains CPU-resolved position and color.
layout(location = 0) in vec2 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 0) out vec4 fragment_color;
void main() {
    fragment_color = in_color;
    gl_Position = vec4(in_position, 0.0, 1.0);
}
