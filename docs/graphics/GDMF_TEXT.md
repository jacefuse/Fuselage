# Diagnostic Text layer

Layer 0 is the engine's diagnostic overlay for errors, statistics and tools.
Application text belongs on its own Tile layer. Include `textlayer/gdmf_textlayer.h`.
The established lowercase `tl` prefix is a deliberate naming exception. Diagnostic
Layer 0 and Tile index zero are separate interfaces.

## Grid state

The grid has 80 columns and 45 rows. Characters are byte values selecting one of
256 built-in 16x16 glyphs; strings are not decoded as UTF-8. Each cell also stores
an RGBA8 color. Callers must serialize printing, reads and renderer preparation.

The initial overlay is inactive. The cursor starts at zero with white selected;
initial cells contain glyph zero and transparent black. `tlActivate`,
`tlDeactivate` and `tlToggle` return the resulting active state. A successful
deactivation therefore returns false. Printing changes host state even while
the overlay is inactive.

`tlCLS` fills cells with space glyphs and opaque black, selects black as the
print color and homes the cursor. Select a visible color before printing text
after a clear. `tlScrollUp` moves rows upward and fills the last row with glyph
zero and transparent black. It leaves the cursor unchanged and returns zero;
there is currently no stored scrollback history.

Cell getters return a space glyph or opaque black outside the grid. Their results
can also be valid cell contents, so they are not separate error indicators.

## Cursor coordinates

`tlSetCursor` clamps columns to 0..80 and rows to 0..44. Column 80 means the next
glyph will wrap to the next row. It is not a writable cell. The getters retain
this pending-wrap position. `tlGetCursor` computes `row * 80 + column`, which
can reach 3600; do not assume it always identifies a cell in the grid.

The positional printing forms reject columns outside 0..80 or rows outside
0..44 with -1, leaving the grid, cursor and selected color unchanged. This
prevents unchecked coordinates from addressing outside the cell arrays.

## Printing

The suffixes describe the arguments: no suffix uses the selected color and
cursor, `C` supplies a color, and `CP` also supplies a position. Successful calls
select their color and leave the cursor after the output. A positional call
followed by ordinary calls can therefore build one line in several colors.

String functions interpret newline as the next row at column zero, ignore
carriage return, and advance tabs to the next eight-column stop. Wrapping and
newlines scroll at the bottom of the grid. Tabs move the cursor without filling
skipped cells. Return values count glyphs placed, excluding those control bytes;
counts saturate at INT_MAX. NULL strings return zero without changing state.

Raw-character functions always place the supplied glyph byte, including values
zero, nine, ten and thirteen. They do not interpret control characters and return
one on success. Integer functions print signed decimal values, including INT_MIN.

Formatted functions use printf-style formatting, with separate explicit-color
and current-color entry points. They accept at most 255 output bytes. Truncation
or a formatting failure returns -1 before changing state; NULL format returns
zero. Supply a valid format and matching arguments, as with standard printf.

## Font data

The built-in font retains all 16,384 bytes of the EdiBiMa-generated ENTERON data.
It is stored as an immutable unsigned array in one source file, rather than a
writable definition in a broadly named header. Existing glyph indices and shapes
are unchanged.

Four pixels occupy each byte, with the first pixel in the lowest two bits. The
original shade mapping is preserved: zero is transparent black, one is opaque
white, two is opaque gray 85, and three is opaque gray 170. The CPU decoder writes
a 256x256 RGBA8 atlas into caller-owned storage. NULL or short output buffers
fail without writes. GPU upload and shader orientation are separate renderer
concerns; CPU byte equivalence does not by itself verify on-screen rendering.

## Vulkan atlas

The internal atlas helper owns the RGBA8 image, memory, view and nearest-filtered
clamp-to-edge sampler. It checks format/extent support and memory types before
publishing resources. Font pixels decode directly into mapped coherent staging
memory, avoiding a second heap copy. The upload transitions the full image from
undefined to transfer destination, copies it, then transitions it for sampling.

Creation uses the shared one-time upload service. Completed upload publishes a
ready atlas and releases staging. Failure before submission cleans up temporary
resources and leaves the output unchanged. An unresolved submission or wait
retains the image, view, sampler and staging owners without marking them ready.
An already-blocked uploader returns unresolved without allocating another atlas.

Unresolved resources must remain alive until the renderer establishes completion
and discards command references. Stop producers and release the uploader's
retained command/fence handles before atlas destruction. Cleanup refuses an
unconfirmed quiescence or a still-retained uploader reference. The caller must
also release descriptor users and keep the borrowed device, upload context,
command pool and locks alive throughout recovery. This helper does not perform
a hidden wait or infer completion from a failed submission.


## Vulkan graphics resources

Each swapchain image has a 28,800-byte storage buffer: 80x45 cells, each holding
one 32-bit glyph index and one packed RGBA8 value. Red occupies bits 0..7,
green 8..15, blue 16..23 and alpha 24..31. Packing reads host rows in reverse
order, preserving the existing combination of CPU row order and shader flips.
Compile-time checks guard the cell stride, color offset and fixed shader grid.

Cell storage uses host-visible coherent memory, preferring device-local memory
where available. It stays mapped until destruction. Complete an image slot's
previous GPU work and discard earlier command references before rewriting it;
a persistent mapping does not provide synchronization. A short packing output
is rejected before any writes. Serialize reads of host state with printing.

A shared descriptor layout exposes the atlas at fragment binding zero and the
cell storage buffer at fragment binding one. Each image receives its own set,
with a buffer range covering all 3,600 cells. The atlas must be ready. Supplied
buffer handles must belong to the same device and provide the full cell range.
The descriptor collection borrows its layout, atlas and buffers.

The vertex shader generates a six-vertex full-screen quad without vertex
buffers. The fragment shader selects the cell, locates its glyph in the 16x16
atlas grid, flips the local glyph Y coordinate and multiplies font shades by
the cell color. The pipeline uses dynamic viewport/scissor and no depth test.
RGB uses source-alpha blending; destination alpha is replaced by source alpha,
as in ENTERON.

Constructors take zeroed caller-owned outputs and publish them only on success.
Creation failures release temporary owners, including any partial graphics
pipeline. Destruction requires completed GPU use and discarded command references;
these helpers do not wait. Release pipeline and descriptor sets before their
layout, and descriptor users before the atlas or cell buffers. Keep the device
alive throughout cleanup. Renderer preparation and lifecycle integration are
separate from these resource helpers.


## Prepared frames and lifecycle

The renderer prepares Text on the render thread under the drawing-state lock,
after waiting for the acquired image slot's fence and resetting its previous
command buffer. Only then may Text rewrite that slot's mapped cell buffer.
Inactive Text, unavailable resources, invalid slots or an empty viewport produce
no prepared draw. Every prepare attempt clears the previous draw first.

Successful preparation captures the pipeline, layout, image-specific descriptor
and viewport. Recording uses these values without reading live host or renderer
state. It consumes the snapshot once and draws six vertices after the other
layers. A wrong image index or missing command buffer consumes the snapshot
without recording. Changes made after preparation take effect on the next
prepared frame.

The backend owns its atlas, descriptor layout, cell-buffer array, descriptor
pool/sets and pipeline. Ordinary allocation failures leave it available for
retry; partial new frame collections are rolled back. An unresolved atlas
upload or an unexpected device/render-pass/image-count change blocks further
Text work and retains resources for checked cleanup. The renderer checks this
health state before acquisition, preparation and submission.

Quiescence requires stopped producers, successful GPU completion and discarded
command references. The coordinator drains acquisition if needed, waits for
idle, resets the render command pool, releases retained upload handles and then
notifies Text. A failed wait/reset does not authorize resource destruction.
Text does not perform its own wait or infer completion from upload failure.

Before swapchain replacement, Text releases pipeline, descriptor sets and cell
buffers while the old render pass is still valid. The atlas and descriptor
layout survive ordinary recreation; preparation builds new image-dependent
resources lazily. Full shutdown is checked by the renderer before device and
service teardown. Unconfirmed cleanup keeps the backend blocked and retains its
owners. Completed shutdown clears backend ownership; host text state and public
tl signatures are unchanged.
