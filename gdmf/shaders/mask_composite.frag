#version 450

// Masked-band composite fragment shader (Approach C -- see PIXIE_MODE_MASK in
// gdmf_pixies.h). binding 0 is the offscreen target holding ONE fully-
// composited priority band (all its tiles/sprites/normal pixies blended
// together against a transparent clear); binding 1 is the pixie's 8-bit
// coverage mask (R8_UNORM, .r is the value). This lays the band onto the
// swapchain with its alpha scaled by the mask, so layers already drawn behind
// this band show through wherever the band is masked out.
//
// The band was rendered straight-alpha over a transparent clear, so this uses
// straight-alpha output and relies on the pipeline's standard src-alpha blend
// onto the swapchain. Fully opaque band content (the common case: a solid tile
// layer revealed through a silhouette) composites exactly; heavily semi-
// transparent band content can fringe slightly, the usual straight-alpha-over-
// transparent artifact -- premultiplied alpha is the refinement if that ever
// matters.

layout(binding = 0) uniform sampler2D bandColor;
layout(binding = 1) uniform sampler2D maskCoverage;

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

// ndcRect: the mask pixie's display rect in NDC, (minX, minY, maxX, maxY),
// computed CPU-side the same way a normal pixie quad's vertices are (so a mask
// lands exactly where a same-attrs pixie would draw). invert: 0 = hide where
// the mask is set (default polarity), 1 = show only where the mask is set.
layout(push_constant) uniform Push {
    vec4 ndcRect;
    uint invert;
} pc;

void main() {
    vec4 band = texture(bandColor, fragUV);

    vec2 ndc = fragUV * 2.0 - 1.0;

    // Outside the mask rect the mask simply isn't set, so coverage there falls
    // out of the same expression with m = 0: the band shows in the default
    // polarity, and is hidden when inverted (an inverted mask is a spotlight --
    // its priority's items show ONLY inside the shape, nowhere else). A
    // fullscreen mask has no outside, which is why both readings agreed until
    // BadPixie ran an inverted one.
    float m = 0.0;
    if (all(greaterThanEqual(ndc, pc.ndcRect.xy)) &&
        all(lessThanEqual(ndc, pc.ndcRect.zw))) {
        vec2 muv = (ndc - pc.ndcRect.xy) / (pc.ndcRect.zw - pc.ndcRect.xy);
        m = texture(maskCoverage, muv).r;
    }
    float coverage = (pc.invert != 0u) ? m : (1.0 - m);

    outColor = vec4(band.rgb, band.a * coverage);
}
