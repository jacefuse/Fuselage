# Tiles

Tiles place indexed bitmaps on independent maps. Include `tiles/gdmf_tiles.h`.
The optional relative scrolling helper is in `tiles/gdmf_tiles_help.h`.

## Layers and cells

There are 16 layer indices, numbered 0 through 15. Each initialized layer has
its own map, bitmap atlas, viewport, scroll and transform state. Maps can contain
up to 512 by 512 cells. Each layer supports up to 1024 bitmap slots, with a fixed
tile size selected at initialization, up to 32 by 32 pixels.

`TileLocation` holds a bitmap ID, orientation/collision flags, signed pixel
offsets, palette, opacity, index-zero visibility and an application-defined
32-bit tag. The tag has no engine-defined meaning. Public structure member
names and layouts are retained for bridge consumers; native applications change
live state through the API.

Cells are stored as `location[map_x][map_y]`. The second dimension has a fixed
stride of `MAX_MAP_HEIGHT`, regardless of the initialized map height. A compact
`width * height` array has a different layout.

## Lifecycle

`InitTileLayer` initializes one layer. Map dimensions must be in 1..512, tile
dimensions in 1..32, and bitmap-slot count in 1..1024. Out-of-range values are
rejected rather than capped. Scale values less than or equal to zero are
rejected; nonfinite validation is incomplete. The function returns false for
invalid arguments, atlas creation failure or an unresolved backend lifecycle.
Initialization requires renderer
services; it is not an independent host-only operation in ENTERON.

The layer starts enabled and visible, unwrapped, at scroll position zero.
Its viewport covers the current canvas, its rotation is zero, and its pivot
is the viewport center. Priority defaults to the layer index. Cells start with
bitmap ID and palette zero, opacity 255, hidden palette index zero, zero offsets
and tag, and no flags. The layer's collidable-color mask starts at zero.

Initialization prepares a built-in test pattern but does not register all
bitmap slots. Register bitmaps before using their IDs. Reinitializing an active
layer releases its previous resources first; failure does not promise to retain
the previous layer.

`ReleaseTileLayer` releases one initialized layer for reuse and returns false
for an invalid or uninitialized layer, or when resource retirement fails.
A failed release can have transferred some resources to the renderer's queue;
the layer is unavailable for rendering or reinitialization until checked recovery.
`ShutdownTiles` has no success result and retains resources when renderer
quiescence has not been established. See Backend lifecycle for coordination.

## Return values and gating

Most action functions return true on success and false on invalid input or a
disabled layer. The visibility and enablement setters and toggles are exceptions:
they return the resulting boolean state. Successfully hiding or disabling a
layer therefore returns false, which is also the invalid-layer result.

While a layer is disabled, placement, fills, appearance, orientation, viewport,
scale, priority, wrapping, scroll and pivot changes fail. Getters still read
the stored state. Visibility and enablement changes remain available. Bitmap
registration, collision flags/masks, tags and dirty marking also remain available
on initialized layers while disabled. Hidden layers continue to accept the same
operations as visible layers.

Scalar getters return zero or false for invalid layers or cells. A returned
zero can also be valid data. Viewport, scroll, pivot and cell-offset getters
accept optional output pointers and write zero to supplied outputs when invalid.
Point-query and mapped-storage results follow their own rules below.

## Bitmap registration

`UploadTileBitmap` copies four-bit palette indices into the layer's host mirror
and marks the slot for later transfer. Pixels are packed high nibble first,
two per byte. Provide enough readable bytes for the tile's pixel count rounded
up to whole bytes. There is no source-length parameter. The caller can reuse
the source after return. True means host acceptance, not completed GPU work.
Bitmap IDs must be inside the layer's configured slot count.

A NULL source selects the built-in test pattern; `UploadTileTestPattern` is the
same operation. `UploadTileBoxPattern` generates an index-one border two pixels
wide around an index-zero interior. Both generators handle every supported tile
size, including odd widths and one-pixel dimensions. Small boxes whose borders
overlap contain only index one. Packed rows continue across byte boundaries;
odd-width rows have no extra padding byte. An unused final low nibble is ignored.

Register every bitmap before placing it. The current renderer and point sampler
have separate readiness requirements. Point queries reject unregistered bitmap
samples; drawing also requires an initial recorded GPU copy.
Unuploaded GPU contents must not be treated as a defined blank tile. Host
sampling and GPU rendering can differ while a registered update awaits transfer.

## Cells

`PlaceTile`, `FillTileLayer`, `FillTileRow`, `FillTileColumn` and `FillTileRegion`
change only bitmap IDs. They preserve palette, opacity, index-zero visibility,
flags, offsets and tags. Placement accepts IDs outside the configured atlas;
such IDs are skipped by rendering. `GetTile` returns the stored ID, or zero for
an invalid layer/cell.

A fill region must be nonempty and fit completely inside the map. Requests
crossing an edge fail without partially filling the map. Row and column fills
use the same rules.

Each cell selects one of Colors' 256 palettes of 16 colors. Despite the
`Transparency` function names, the value is opacity: zero is invisible and
255 preserves the palette color's alpha. `showzero` controls whether palette
index zero is rendered. These appearance settings do not disable collision.

## Orientation and transforms

Cell flips mirror the bitmap before an optional clockwise quarter turn.
`SetTileCellRotation` controls that quarter turn; `GetTileCellRotation` reads
the flag. For nonsquare tiles, a quarter turn swaps the drawn width and height
about the cell's center, allowing gaps and overlap with neighboring cells.
Signed cell offsets are canvas-pixel nudges in the range -128..127.

The viewport uses signed canvas coordinates and can lie partly or wholly off
canvas. Negative viewport extents are stored as zero. Layer scale enlarges map
pixels; scroll offsets remain in unscaled source pixels. Wrapping normalizes
scroll into the map span. Nonwrapping scroll is clamped to the available range,
with zero range when the map is smaller than the visible footprint.

`SetTileMapRotation` turns the map clockwise in **radians**. Sprite rotation uses
degrees. The viewport does not rotate: it continues clipping the turned map.
The pivot is measured in canvas pixels from the viewport's top-left corner.
Moving the viewport moves that pivot; resizing does not recenter it. Rotation
expands the footprint used when a scroll offset is clamped. Changing transform
settings does not automatically reapply an earlier scroll request.

Layer priority shares the 0..255 range with Pixies and Sprites. Lower values
draw later. At equal priority, Pixies draw first, then Tiles, then Sprites.
Among Tile layers at the same priority, lower layer indices draw later.

## Point queries

`GetTilePointInfo` returns true when the canvas point resolves to a map cell.
The output reports the cell coordinates, its collision flag, whether the point
lies on the cell's drawn tile, and the sampled palette index when it does.
A valid cell can return true with `onTile` false, for example after a nudge
moves its bitmap away from the query point.

A NULL output fails. Other failures clear the result. Queries account for
viewport clipping, scale, scroll, wrapping, layer rotation, and the selected
cell's nudge and orientation. They inspect the grid cell under the point;
they do not search neighboring cells for overhanging bitmaps. They do not use
visibility, enablement, opacity or `showzero` as collision gates.

An unregistered or out-of-range bitmap leaves the resolved cell's `onMap` and
collision flag intact, but reports `onTile` false and palette index zero. It
cannot produce a pixel collision. Registered samples use the current host copy,
including updates whose GPU transfer is still pending.

Nonfinite coordinates, invalid prepared geometry and grid coordinates outside
the internal integer range fail with a cleared result. Extreme finite geometry
can also leave a cell without a usable pixel sample. These checks prevent invalid
conversions; they do not establish exact rendering agreement for every extreme
transform or change the attribute setters' existing acceptance rules.

`CheckTilePointCollision` also requires the cell's collision flag and a sampled
palette index selected by the layer's collidable-color mask. A zero mask matches
nothing. Sprite-shaped queries belong to Interactions, whose supported transform
cases are a separate contract.

## Guest cell mapping

`GetTileLayerCellsPtr` returns borrowed writable cell storage for an initialized
layer, or NULL otherwise. It supports the guest bridge; native application code
uses setters. Mapped writers must preserve the fixed array stride, stay within
the exposed storage, and arrange serialization with engine readers.

After mapped writes, call `MarkTileLayerDirty` so geometry is rebuilt. Raw writes
do not pass through setter validation or enablement gates. Layer release makes
the mapping logically invalid even though the backing array has static storage;
the bridge must refresh or revoke mappings accordingly. The pointer interface
does not itself provide guest bounds checking or a thread-safety guarantee.

## Scrolling helper

`ScrollTileMap` reads the current source-pixel scroll, adds the supplied offsets
and calls `SetTileMapOffset`. It preserves that setter's wrapping, clamping and
success result. It does not scale the added offsets. The read and write need the
same serialization as other state changes.

## Host state ownership

Native cell and layer operations share the same host state as mapped cell
storage. Geometry changes notify the renderer to rebuild its image-dependent
view; collision flags and application tags do not require vertex changes.
Priority, visibility and enablement are read when preparing a frame rather
than baked into cell vertices.

State access must be serialized with rendering, collision queries and mapped
writers. The internal host reset does not release GPU resources. The lifecycle
owner must resolve old resource references and coordinate replacement before
publishing reset state. A host-only test setup does not establish that a layer's
atlas or rendering resources are ready.

## Bitmap ownership

Each layer has a private host bitmap bank with its configured dimensions and
slot count. Renderer and collision helpers borrow read-only unpacked pixels;
an invalid or unregistered slot has no readable bitmap result. Resetting a bank
clears registration and pending transfers without changing cell attributes or
releasing GPU resources. The lifecycle owner coordinates bank and layer reset.

First registration requests a geometry rebuild, since the bitmap may previously
have been omitted. Replacing a registered bitmap only marks its pixels pending.
Writes remain available while the layer is disabled. A fresh GPU atlas can mark
all registered bitmaps pending again so it refills from the host mirror.

Pending flags are cleared after recording complete bitmap copies, which does not
establish submission or completion. Frame-abandonment recovery and consistent
GPU readiness checks in rendering require the corresponding backend coordination.
Host access remains serialized.

## Prepared probes

Internal probes cache a layer's inverse rotation and other repeated transform
values for point walks. They borrow live layer and bitmap state; they are not
detached snapshots. Keep that state unchanged from probe creation through the
last query, and discard the probe before layer edits, reset or release.

For unturned layers, independent column and row lookups can be combined into a
cell query. Those lookups return unavailable axes for turned layers, where both
coordinates are needed to undo the rotation. The full point query handles either
case, testing clipping in canvas space and sampling in unrotated layer space.

## Vulkan atlas and uploads

Each Tile layer has an `R8_UINT` array image containing one unpacked palette
index per texel. Its dimensions and array-layer count match the layer's configured
tile size and bitmap slots. Nearest sampling preserves palette indices; the
shared Colors buffer supplies their final colors.

Atlas creation checks sampled-format support, image limits, device-local memory
selection and binding, view creation and sampler creation. A failed construction
releases its partial resources and leaves the caller's result unchanged. It
does not submit commands or wait for the device. Initial layout setup is recorded
outside the render pass during the first frame upload pass.

Uploads copy registered host bitmaps through the shared frame staging arena.
Every pending slot requests space, including after another request fails, so
arena growth can account for the full demand. Failed requests remain pending.
Each copy transitions its array layer to transfer destination and back to sampled
layout. Atlas and host dimensions/count must agree; mismatches record no work.

A bitmap is drawable only after its first full copy has been recorded. That
first copy requests a geometry rebuild if previous preparation omitted it.
An update to an already populated bitmap may leave its previous GPU contents
drawable while the current host copy waits for staging. Point queries read the
current host copy during that interval.

A fresh atlas refills every registered slot, even if an earlier atlas cleared
its host dirty flag. Atlas destruction preserves host registration and pixels.
It requires completed GPU use, discarded command references, released descriptor
users and excluded producers. The resource helper does not wait or arrange
deferred destruction; the backend lifecycle owner must establish those conditions.

Readiness/layout flags and cleared dirty flags describe recording, not submission
or completion. Recovery after an abandoned frame still needs coordination; merely
discarding commands does not prove that their atlas transitions or copies ran.

## Vulkan graphics resources

Tile layers share one descriptor layout. Each layer owns a descriptor collection
with one set per swapchain image: binding zero samples that layer's atlas and
binding one reads the image's shared Colors buffer. The collection borrows the
layout, atlas and palette buffers. Construction requires non-null handles for
every palette buffer and publishes only a complete collection, reclaiming partial sets through
their pool on failure. Layers can create or release their collections independently.

The shared pipeline uses triangle lists, no culling, one sample, and dynamic
viewport/scissor state. Each 32-byte vertex carries position, texture coordinates,
bitmap ID, palette, opacity and index-zero visibility. Discrete selectors remain
flat across a triangle. Tile opacity is passed in 0..255 units and normalized
in the fragment shader. Palette indices resolve through the shared packed Colors
buffer; index zero is discarded unless the cell enables it.

RGB blending uses source alpha and one-minus-source-alpha. As in ENTERON, the
alpha blend replaces destination alpha with source alpha. It does not accumulate
alpha like RGB. This behavior remains subject to the shared mask-composition
review.

Each layer/image slot has independent, persistently mapped coherent vertex
storage. Allocation prefers device-local host-visible coherent memory and can
fall back to ordinary host-visible coherent memory. Initial capacity is at least
4096 vertices. Growth creates, binds and maps replacement storage before releasing
the old allocation. Failure preserves the old allocation, mapping and contents;
successful replacement requires the caller to write the new frame's vertices.

Reuse or growth requires completed prior GPU use and discarded recorded
references for that slot. Cleanup releases pipeline and descriptor users before
the shared layout, then descriptor users before borrowed atlas/palette resources.
These helpers take explicit device/resource inputs, do not submit or wait, and
do not establish safe runtime retirement on their own.

## Prepared frames

Each layer/image slot owns its vertex storage, cached vertex count and geometry
validity. Dirty geometry is rebuilt for that slot; a failure invalidates the
published draw and keeps the allocation for retry. Counts are not shared between
different images. Canvas changes and replacement atlas images also force rebuilds.
The backend marks all affected slots dirty after geometry or bitmap-readiness
changes. Priority and visibility are captured without requiring new vertices.

Axis-aligned and rotated maps retain separate geometry paths. Both use the same
mirror/quarter-turn texture-coordinate rules. The rotated path advances regular
grid origins by precomputed edge vectors and rotates cell nudges with the map.
Candidate coverage retains ENTERON's one-cell margin; unusually large nudges or
overhangs require further coverage review.

Finite-coordinate checks use CPU-local quad data before copying into mapped
vertex storage. CPU reads from device-local host-visible memory can be expensive;
the mapped output is not scanned back for validation.

Preparation checks finite geometry, integer cell ranges, count multiplication
and vertex byte sizes before allocation or conversion. Invalid or unavailable
geometry produces no published draw. These checks do not impose a memory/work
budget on every representable scene.

Only registered bitmaps with an initial recorded atlas copy contribute vertices.
A later successful first copy invalidates geometry that previously omitted its
bitmap. Host collision sampling may see a new registered bitmap before it is
ready for GPU drawing.

The draw snapshot contains handles, vertex count, priority, image index, render
viewport and the clipped layer scissor. Scissor conversion preserves ordinary
integer truncation and checks coordinate ranges before casting. Fully clipped,
hidden, disabled or empty layers record nothing. A failed preparation cannot
reuse the prior published draw.

Recording reads these copied values without consulting live layer state or
canvas/window getters. Within a priority it walks layer indices from high to low,
putting lower indices in front. The compositor visits priorities 255 down to zero.
Wrong image indices or priorities do not record a draw. Resource owners must
retain captured handles through execution, including a layer released between
preparation and submission. Copying a snapshot does not itself establish that
GPU lifetime; the backend transfers released resources to the renderer's queue.

## Backend lifecycle

The Vulkan backend owns each layer's atlas, descriptor collection and per-image
frames. One pipeline and descriptor layout are shared by all layers. Dirty flags
are allocated for the actual image count, without an eight-image limit. Host
changes and initial bitmap readiness invalidate every affected slot.

Preparation runs under the drawing-state lock and copies each draw into a
separate batch. Recording uses only that batch. Releasing or reinitializing a
layer does not rewrite an already prepared draw. A successful runtime release
transfers its buffers, descriptor pool, atlas image and sampler to the renderer's
retirement queue before clearing host state. Shared pipeline resources remain
owned until a quiesced renderer boundary. Mapped vertex memory stays mapped
until its queued memory allocation is freed.

If retirement fails, the backend retains every resource not already transferred
and stops further frame work. Reinitialization checks release success before
replacing a layer. Shutdown can recover retained owners only after the renderer
confirms device completion and discards command references. Calling shutdown
without that confirmation retains the owners, including when the public call
cannot return a failure value.

Resize releases Tile frames, descriptor pools and the pipeline before borrowed
palette buffers or the render pass are replaced. Atlas images and host bitmaps
survive. The next preparation rebuilds against the new image count and palettes.
Unexpected device, render-pass, image-count or palette replacement stops work
and retains resources rather than silently rebinding existing owners.

Runtime retirement uses the renderer's existing image-count countdown. It is
not an independently verified completion token. Atlas flags describe recorded
uploads, so recovery after discarding an unsubmitted upload also remains open.
These limitations require further FUNDUS hardening and native validation.

## Atlas preview

`ToggleTileAtlasView` toggles a diagnostic flag, and `GetTileAtlasViewActive`
reads it. Any layer index in 0..15 can hold this flag, including an uninitialized
layer. Invalid indices are ignored by the toggle and read as false. The flag
does not currently render an atlas preview.
