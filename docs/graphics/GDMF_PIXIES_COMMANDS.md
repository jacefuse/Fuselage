# Pixie commands

## Command packets

`IssuePixieCommand` takes a target ID, opcode, 16-bit flags, and four 32-bit
argument words. Most commands require the argument array. `SHOW`, `HIDE`, and
`CLEAR` without a requested fill do not read it. Pass zero for unused flags
and reserved argument words.

A `PixieCommand` adds the target ID to the packet for a list. Its layout is
twenty bytes: uint8 target, uint8 opcode, uint16 flags, then four uint32
arguments. `args` is the argument array's field name. Numeric IDs and field
order are part of the native/guest binary contract.

| Opcode | Flags | Arguments 0, 1, 2, 3 |
| --- | --- | --- |
| `SET_ATTR` (0) | Priority in bits 0..7; enabled in bit 8 | Float bits for x, y, width, height |
| `UNPACK` (1) | Color or mask format, depending on mode | RAM offset; signed whole-pixel destination pair; palette index; reserved |
| `DRAW` (2) | Unused | Packed 13.3 start; packed 13.3 end; color; pixel width |
| `PLOT` (3) | Unused | Float bits for x; float bits for y; color; reserved |
| `CLEAR` (4) | Bit 0 selects explicit fill | Fill color or coverage; remaining words reserved |
| `SHOW` (5) | Unused | Unused |
| `HIDE` (6) | Unused | Unused |
| `EXECUTE` (7) | Reserved | RAM list offset; count; transform slot; reserved |
| `SET_DRAW_PATTERN` (8) | Unused | Low sixteen bits are the pattern; remaining words reserved |
| `DRAW_POLY` (9) | Polygon mode, optionally `PIXIE_PTS_FLOAT` | RAM points offset; point count; color; outline width or reserved |
| `SET_TRANSFORM` (10) | Reserved | Transform slot; remaining words reserved |
| `DRAW_LINES` (11) | Line mode, optionally `PIXIE_PTS_FLOAT` | RAM points offset; vertex count; color; pixel width |

Colors use `PackColorsRGBA8` layout: red in the low byte, then green, blue,
alpha. Mask drawing uses the color word's low byte as coverage. Width zero
defaults to one; current line widths are capped at 256 pixels.

Commands return false for a refused checked requirement. True means the command
was accepted, not that pixels have appeared or every requested primitive fit
the live geometry capacity. Pixel-editing commands are refused on GPU-resident
textures. There is no general malformed-input or completion guarantee.

## Coordinate packing

`PackPixieFloatArg` copies a binary32 float's bits into one argument word.
It does not numerically convert the value to an integer. Use it for `SET_ATTR`
and `PLOT` arguments.

`PackPixieDrawXY` stores x in the high sixteen bits and y in the low sixteen,
as signed fixed-point with three fractional bits. Resolution is one eighth;
the range is -4096 through 4095.875. Each coordinate
saturates to the encoding range before integer conversion, then rounds using
the current floating-point rounding mode. Under the normal nearest mode,
halfway values follow the platform's `lrintf` nearest-even behavior.
Infinities saturate to the corresponding bound; each NaN coordinate becomes zero.
Guest packing preserves its existing add-half-and-truncate rounding, so halfway
values can differ. See [guest Pixies](../guest/MISL_GUEST.md#pixies-and-command-lists).

For lines with endpoints outside this range, use `PackPixieDrawLine`. It clips
the segment to the encoding box before packing; independently clamping both
endpoints can change the line's direction. It returns false for NULL output,
nonfinite coordinates, an overflowing endpoint difference, or a wholly outside
segment, leaving output words unchanged. A true result supplies two packed
endpoints; it does not test the pixie viewport. Floating-point precision still
limits clipping of extreme coordinates.

`UNPACK` uses a different pair encoding: signed whole-pixel x in the high half
and y in the low half, without fractional bits. Do not use `PackPixieDrawXY`
for that destination argument. Mask unpacking ignores this destination word;
it updates the full mask or listed tiles in the mask's own frame coordinates.

## Point arrays and polygons

`DRAW_LINES` and `DRAW_POLY` read four-byte-aligned arrays from pixie RAM.
With `PIXIE_PTS_FLOAT` clear, each point is one packed 13.3 word. With it set,
each point is two binary32 floats, x then y, occupying eight bytes. The whole
array must fit in RAM. The float form avoids quantization before transforms.

`PIXIE_LINES_STRIP` joins successive points and leaves the path open.
`PIXIE_LINES_SEGMENTS` draws disjoint pairs; an unmatched final vertex is
ignored. At least two vertices are required. The array is streamed rather
than limited by the polygon point cap.

Polygons accept three through `PIXIE_POLY_MAX_POINTS` (256) points. Modes
`FILLED`, `LINES`, and `PATTERN` occupy the low nibble of flags. The line form
closes the outline automatically and is supported in all three pixie modes.
Filled and patterned forms use even-odd filling in texture or mask mode and
are refused in live mode. Unknown modes and undefined point-array flag bits
are refused.

## CPU drawing and live geometry

Texture and mask endpoints round to whole pixels using the current `lrintf`
rounding mode. `PLOT` checks the rounded cell, so a coordinate such as -0.4
can address pixel zero. Lines skip writes outside the output area while walking
the complete segment. Drawing inputs and transformed results must remain finite
and within the rasterizer's integer arithmetic range. Extreme coordinates are
not generally validated or clipped before conversion.

Texture lines follow a repeating sixteen-bit draw pattern, starting at bit zero
for each segment. Thick lines stamp a square of the requested width at each
enabled step. Mask lines currently ignore the draw pattern. Their stamp extends
from `-width/2` through `+width/2`, inclusive, so an even width covers one extra
pixel in each dimension. Texture and mask line stepping also use different
tie rules. These are existing differences; applications should not assume
identical coverage between modes.

Polygon fills sample row centers and use half-open edges with the even-odd rule.
Pattern bitmaps tile in output coordinates; texels with zero alpha leave the
destination unchanged, while other texels replace it without alpha blending.
Without a bitmap, patterned texture fills use bit `(x + y) & 15` of the draw
pattern. Patterned mask fills always use that stipple, ignoring color bitmaps.

Live drawing retains vertices in normalized device coordinates. Display placement,
scale, rotation and canvas dimensions are applied when each primitive is issued.
Changing display attributes afterward does not recalculate already retained
vertices. A solid line normally uses one rectangle; a patterned line uses a
rectangle for each enabled run at approximately one-unit intervals. A zero-length
line makes a square regardless of the line pattern. Line width remains unscaled
by command-list transforms.

Live geometry has capacity for 4096 six-vertex primitives. Primitives that exceed
the capacity are dropped without making the enclosing command return false.
`CLEAR` discards retained vertices and can add one full-area fill rectangle.
Capacity therefore persists across frames until cleared or reinitialized.

## Lists and results

`RunPixieCommandList` reads a caller-owned host array without copying it. Keep
the array valid and stable throughout execution. NULL commands or zero count
return zero with an empty result when supplied. The host entry point does not
enforce `PIXIE_LIST_MAX_COMMANDS`; callers must provide a valid array of the
requested count. The 65536-record limit applies to RAM-based `EXECUTE`.

`EXECUTE` reads four-byte-aligned records from the named pixie's RAM. Each
record can target a different pixie. Its count must be 1..65536 and the complete
array must fit. Uploading a list retains its bytes; commands execute only when
the list is invoked. This is separate from live mode retaining generated
vertices between frames. Both list entry paths are implemented.

The return count and optional `PixieListResult` concern top-level records.
`executed` counts visited records, including refusals; `rejected` counts refused
records. Processing continues after a refusal. `firstRejectIndex` is
`PIXIE_LIST_NO_REJECT` if none, otherwise the first refused record's index;
the opcode and target fields identify that record. The first rejection's text
is restored to its target's diagnostic buffer after the list ends.

Nesting is limited to eight active lists. Depth rejection visits no records,
sets `executed` to zero and `rejected` to the requested count, and sets the
first index to zero; opcode and target fields remain zero. A valid `EXECUTE`
record currently returns true even if commands inside its child list fail
or hit the nesting limit. Parent totals therefore do not summarize nested
failures.

## Transforms

Each pixie has sixteen affine transform slots. All start at identity. Slot
zero is readable but cannot be written or reset; setters accept slots 1..15.
`SetPixieTransform` applies scale, clockwise rotation in degrees, then
translation. Matrix setters/getters copy the same register directly.

The matrix field names retain the documented mathematical notation:

```text
x' = a*x + b*y + tx
y' = c*x + d*y + ty
```

An executing list composes its selected local transform inside the inherited
transform: the local/model transform acts first, followed by the parent/scene
transform. `SET_TRANSFORM` replaces the current local selection relative to
the transform that the enclosing list started under; successive selections
do not accumulate. Slot zero returns to that starting transform. Returning
from a list restores the caller's transform state.

Outside a list, `SetPixieCurrentTransform` changes the standing interpreter
transform until changed again. This state is shared across command targets;
it is not a separate current-transform register per pixie. Commands and list
execution must be serialized.

Transforms affect point and line geometry before rasterization. They do not
scale line width, move a pixie's display attributes, change `UNPACK` buffer
indices, or move pattern tiling in local output space. Compact points are
quantized before transforming, so large model scaling magnifies their steps
of one eighth of a unit; use float point arrays when that matters.

## Packing checks

The packing helpers can be checked without Vulkan or the rest of the engine:

```sh
cmake -S fuselage/tests/GDMF/pixies -B fuselage/build/pixie-packing-test -DCMAKE_BUILD_TYPE=Release
cmake --build fuselage/build/pixie-packing-test --config Release
ctest --test-dir fuselage/build/pixie-packing-test -C Release --output-on-failure
```

The packing test checks packet layouts, float-bit preservation, fixed-point limits,
rounding modes, segment clipping, and unchanged output on rejected lines.
The host-state test additionally exercises real logical state and transform
registers with controlled backend and command-dispatch boundaries. It checks
allocation failures, ownership-handoff retention, RAM/diagnostic boundaries,
and wrapper packet construction; it does not substitute a working command
interpreter or validate rendering. Assertions remain enabled in Release builds.
No Vulkan SDK is required.

The command test links the real interpreter, rasterizer and live geometry code.
It checks representative pixels and vertices, patterns, widths, list results,
nested transforms across targets, diagnostics and command bounds. Backend and
packed-decoder services remain controlled test boundaries. UNPACK validation
and raw mask copying are exercised; packed decoding and GPU execution are not
verified by this test.
