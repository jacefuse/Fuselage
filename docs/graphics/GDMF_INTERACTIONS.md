# Interactions

Interactions combines Sprite geometry and Tile collision queries. Include
`gdmf_interactions.h`. Calls read current host state and do not wait for GPU
uploads. Keep Sprite transforms, bitmap registrations, Tile maps and masks stable
through a query or batch. The implementation uses public APIs and the internal
prepared probes shared with each subsystem's point queries.

## Geometry

`GetInteractionsSpriteDistance` measures between two derived Sprite centers in
canvas pixels. Each center includes uniform/per-axis scale and hotspot offset.
It is the unrotated geometric center used to construct the Sprite transform;
hotspot-pivot rotation can move the rendered center away from that point.

`GetInteractionsSpriteDirection` returns the signed atan2 angle between those
centers, in degrees, normally from -180 to 180. Zero points right; positive angles
point toward positive canvas y, so they appear clockwise on a downward-positive
canvas. It does not report either Sprite's facing. Both functions return zero
for an invalid index; zero can also be a valid result.

`GetInteractionsSpritePixelWorldPosition` transforms an integer bitmap coordinate
into canvas space. Coordinates must be inside the Sprite bitmap and both output
pointers must be supplied. Invalid indices, coordinates or pointers return false
without changing outputs. The transform applies flips, uniform/per-axis scale,
shear and rotation about the selected center or hotspot. It uses the bitmap
coordinate itself, without adding a half-pixel sampling offset. Nonfinite or
overflowing transform values are not fully validated by this forward query.

## Tile collisions

`CheckInteractionsSpriteTileCollision` returns the requested collision bits that
matched. Unknown bits are ignored. Zero means no match, invalid input or an
unavailable query. Visibility, enablement and automatic collisionTypes do not
gate this on-demand test. Collision results are not stored for the next frame.

| Requested type | Match condition |
| --- | --- |
| BOUNDING_BOX | The Sprite's world bounding box overlaps a flagged Tile cell. |
| ANY_COLOR | A nonzero Sprite pixel overlaps a nonzero pixel of a flagged cell. |
| COLLIDABLE_COLORS | Both pixels belong to their respective collidable-color masks, and the cell is flagged. |

Bounding-box tests need the cell flag but do not require a registered Tile bitmap.
Pixel tests require registered bitmap data on both sides. Index zero can match
COLLIDABLE_COLORS when both masks include it. Opacity and show-zero drawing rules
do not replace collision masks.

Rotated Tile maps are unsupported by these overlap queries, including the
bounding-box type. They return zero and print one diagnostic per layer index.
Tile point queries support rotation; use those when appropriate. Rotation
support for Interactions remains planned FUNDUS work.

Pixel tests walk integer canvas points over the Sprite bounds. Prepared Tile
axes and conservative Sprite row spans reduce repeated transform work. Numeric
guards reject nonfinite bounds and unrepresentable integer endpoints. Very large
representable walks still need a workload policy; zero does not distinguish an
unavailable query from a miss.

## Cell enumeration

`GetInteractionsSpriteTileCollisionCells` enumerates flagged cells overlapped by
the Sprite's box. It returns the total count, writing at most `capacity` entries
in row order. NULL output or nonpositive capacity counts without writing.
Invalid input and rotated maps return zero. Counts saturate at INT_MAX.

Wrapping reports wrapped coordinates, so a cell can appear more than once. The
enumerator samples the box at scaled tile-sized steps, with a 0.001 canvas-pixel
inset at its exclusive far edge. Adjacent samples in the same wrapped column are
collapsed. This preserves current behavior; it is not a precise enumeration of
nudged or rotated bitmap outlines. Nonfinite or nonadvancing steps stop traversal.

## Batches

`CheckInteractionsSpriteTileCollisionBatch` writes one result byte per supplied
Sprite index and returns how many results are nonzero. Invalid indices produce
zero entries. Nonpositive count or NULL arrays return zero without writing.
Input and result arrays must not overlap, and results must hold `count` bytes.

The caller prepares bounding-box caches before dispatch. Small estimated walks
run inline; larger ones use the shared synchronous worker pool. All work finishes
before return. Keep shared input state unchanged and give each result its own
storage. Worker dispatch does not make concurrent Sprite or Tile edits safe.
