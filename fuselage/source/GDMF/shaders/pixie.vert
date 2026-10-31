#version 450
// Position already includes the CPU display transform.
layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_uv;
layout(location = 0) out vec2 fragment_uv;
void main() {
    fragment_uv = in_uv;
    gl_Position = vec4(in_position, 0.0, 1.0);
}
