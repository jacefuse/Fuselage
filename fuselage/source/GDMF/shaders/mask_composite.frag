#version 450

// Composite one priority's offscreen color through its mask coverage.
// SEE: (GDMF_VULKAN.md, Mask composite shaders).
layout(binding = 0) uniform sampler2D priority_color;
layout(binding = 1) uniform sampler2D mask_coverage;

layout(location = 0) in vec2 texture_coordinates;
layout(location = 0) out vec4 output_color;

layout(push_constant) uniform MaskPushConstants {
    vec4 normalized_rect;
    uint inverted;
} mask_push;

void main() {
    vec4 color = texture(priority_color, texture_coordinates);
    vec2 normalized_position = texture_coordinates * 2.0 - 1.0;

    // Outside the rectangle, coverage is zero before applying inversion.
    float mask_value = 0.0;
    if (all(greaterThanEqual(normalized_position, mask_push.normalized_rect.xy)) &&
        all(lessThanEqual(normalized_position, mask_push.normalized_rect.zw))) {
        vec2 mask_coordinates = (normalized_position - mask_push.normalized_rect.xy) /
            (mask_push.normalized_rect.zw - mask_push.normalized_rect.xy);
        mask_value = texture(mask_coverage, mask_coordinates).r;
    }
    float coverage = (mask_push.inverted != 0u) ? mask_value : (1.0 - mask_value);
    output_color = vec4(color.rgb, color.a * coverage);
}
