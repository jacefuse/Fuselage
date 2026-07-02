#version 450

// One full-screen quad, no vertex buffer -- positions and grid coordinates
// are both hardcoded per gl_VertexIndex (matches the old per-cell quad's
// vertex order: TL, BL, TR, BL, BR, TR). Everything that used to vary per
// cell (which character, what color) now comes from a per-fragment lookup
// into CellBuffer in the fragment shader instead of a per-vertex attribute.
//
// kGridCoords' Y=0 at NDC Y=+1 (screen top) and Y=TEXT_LAYER_HEIGHT at NDC
// Y=-1 (screen bottom) -- same top-down convention the old vertex-based
// layer used. 80/45 must match TEXT_LAYER_WIDTH/TEXT_LAYER_HEIGHT in
// gdmf_textlayer.h; nothing enforces that automatically (same situation
// as SPRITE_REFERENCE_CANVAS_WIDTH/HEIGHT already being duplicated as raw
// literals across multiple C files in this codebase).

layout(location = 0) out vec2 fragGridCoord;

const vec2 kPositions[6] = vec2[](
    vec2(-1.0,  1.0), vec2(-1.0, -1.0), vec2(1.0,  1.0),
    vec2(-1.0, -1.0), vec2(1.0,  -1.0), vec2(1.0,  1.0)
);

const vec2 kGridCoords[6] = vec2[](
    vec2(0.0,  0.0),  vec2(0.0,  45.0), vec2(80.0, 0.0),
    vec2(0.0,  45.0), vec2(80.0, 45.0), vec2(80.0, 0.0)
);

void main() {
    gl_Position   = vec4(kPositions[gl_VertexIndex], 0.0, 1.0);
    fragGridCoord = kGridCoords[gl_VertexIndex];
}
