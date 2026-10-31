# Sprites

Sprites place indexed bitmaps on the GDMF canvas. Include
`sprites/gdmf_sprites.h`; optional additive helpers are declared in
`sprites/gdmf_sprites_help.h`.

## Bitmaps

There are 1024 shared bitmap slots and 4096 sprite instances. Each bitmap is
64 by 64 pixels, with two four-bit palette indices per byte: the first pixel
uses the high nibble. A complete source bitmap occupies 2048 bytes.

`UploadSpriteBitmap` copies the entire bitmap into the host mirror and marks
the slot for a later GPU upload. The caller can reuse its source after return.
The function has no byte-count parameter: provide all 2048 readable bytes.
Success means host acceptance, not completion or even recording of GPU work.
Overwriting a slot changes the shared shape for every sprite assigned to it.

`AssignSprite` changes a sprite's bitmap reference without changing its other
attributes. A valid slot number need not have been uploaded yet; such a sprite
is omitted from the draw list until the bitmap is registered.
`SPRITE_BITMAP_NONE` removes the reference. `AssignSpriteBitmapFromSprite`
copies both the reference and the source sprite's `showzero` setting.

Initialization generates a checkerboard in slot zero. `AssignSpriteTestPattern`
assigns that slot without enabling or showing the sprite. Slot zero can be
explicitly overwritten like any other valid bitmap slot.

## Lifecycle

`InitSprites` resets the host instance attributes and bitmap registration,
generates the test pattern, and attempts atlas and pipeline setup. Its return
value is the number of initialized sprite slots, currently 4096. It does not
report whether GPU setup succeeded. Preparation can retry pipeline setup.

Instances start disabled and invisible, with unit scales, zero position,
rotation, shear and hotspot, palette and priority zero, opacity 255, and no
collision reporting. Palette index zero is hidden. The collidable-color mask
starts at `0xFFFE`, selecting indices 1 through 15. Sprite zero alone starts
assigned to the test-pattern bitmap; the others have no bitmap assignment.
Using sprite zero as a mouse cursor is an application convention.

Initialization also discards pending uploads for invalidated bitmap registrations
and marks the new test pattern for upload. It does not preserve prior bitmap
assignments or registration. Existing collision reports are refreshed by the
next automatic pass, not by initialization itself.

Repeated initialization reuses existing GPU resources rather than destroying
resources that a recorded or submitted frame may still reference. Host state
must be serialized with rendering and workers during the reset.

`ShutdownSprites` releases renderer resources. Call it through the engine's
lifecycle with rendering stopped and GPU references resolved. It has no result
value; do not interpret return from this function as a separate GPU completion
signal. Standalone or repeated lifecycle use requires the same coordination.
Shutdown does not reset host attributes or erase the host bitmap mirror.

## Attributes

An enabled sprite participates in automatic collision processing. It is drawn
only when also visible and assigned a registered bitmap. Visibility affects
rendering alone. Disabling a sprite preserves its visibility and other
attributes; setters can configure a disabled sprite.

`ClearSprite` disables the instance and removes its bitmap reference. It leaves
other attributes and shared bitmap contents unchanged.

The selected palette is one of Colors' 256 palettes of 16 colors. Palette index
zero is discarded unless `SetSpriteShowZero` enables it. That rendering choice
does not change collision masks. Despite its name, `SetSpriteTransparency`
sets opacity: zero is invisible and 255 leaves the palette color's alpha
unchanged. Opacity does not disable collision tests.

Lower priority values draw later, nearer the front. At equal priority, later
sprite indices draw over earlier ones. GDMF interleaves Pixies, Tiles and Sprites
at each priority, with Sprites last among those three layers.

`GetSpriteRenderedCount` reports the currently published prepared Sprite count.
It is zero before preparation, after failed preparation, and when lifecycle
cleanup invalidates the snapshot. It does not establish that commands were
submitted or a frame was presented.

The `Sprite` type describes the attributes, but live instances are held in a
private array. Use the API to change them so dependent geometry caches are
invalidated. No function returns a mutable pointer to live sprite state.

## Transforms

Positions use canvas coordinates. The hotspot is an anchor in unscaled local
bitmap coordinates, measured from the top-left; placing a sprite places this
anchor at the requested position. Hotspots may lie outside the bitmap.

Uniform scale multiplies the per-axis scales. The effective scales are
`scale * scaleX` and `scale * scaleY`. Shear follows scaling and precedes
rotation. Rotation is in degrees and uses the sprite center by default;
`SetSpriteRotateAroundHotspot` selects the hotspot as pivot. Flipping changes
bitmap sampling without changing the transformed footprint.

Scale setters reject values less than or equal to zero. This check alone
does not reject NaN or positive infinity. Other transform setters also do not
provide comprehensive nonfinite or extreme-value validation. Supply finite
coordinates and positive finite scales; acceptance is not a numeric safety
guarantee for rendering or pixel walks. Degenerate shear can prevent inverse
point sampling.

## Return values

Boolean setters and toggles report success, not the resulting value. An invalid
sprite index returns false without changing the instance. The bitmap assignment
and upload calls also reject out-of-range bitmap IDs; upload rejects NULL data.
Diagnostics are not uniform across these calls, so check returned values.

For invalid sprite indices:

| Query | Result |
| --- | --- |
| Boolean attribute or collision-presence getter | false |
| Uniform scale getter | 0 |
| Per-axis scale getter | 1 |
| Other float or integer attribute getter | 0 |
| Bitmap ID | `SPRITE_BITMAP_NONE` |
| Collision count | 0 |
| Collision array | NULL; optional count receives 0 |
| World-point containment | false |
| Bounding box | false; outputs unchanged |
| Bitmap pixel at a world point | -1 |

## Collision results

The renderer's sprite preparation runs the automatic CPU collision pass before
checking graphics pipeline readiness. It tests enabled sprites, including
invisible ones. Each sprite's reporting mask controls which types it records;
the other sprite need not request collision reporting itself.

| Type | Test |
| --- | --- |
| `COLLISION_TYPE_BOUNDING_BOX` | Overlap of axis-aligned boxes enclosing transformed sprites |
| `COLLISION_TYPE_ANY_COLOR` | Overlapping sampled pixels with nonzero indices in both bitmaps |
| `COLLISION_TYPE_COLLIDABLE_COLORS` | Overlapping sampled pixels selected by each sprite's color mask |

Pixel tests use host bitmap data and account for transforms and flipping. They
do not use palette alpha, sprite opacity or visibility as collision coverage.
Bounding-box overlap can be reported without registered bitmap contents.

Nonfinite bounding boxes produce no reported match. Pixel walks whose overlap
endpoints cannot be represented as `int` skip pixel types; a requested bounding
box match can still be returned. The bitmask interface has no separate error
code for these numeric limits. Such results must not be used to infer reliable
pixel coverage for extreme or invalid transforms.

`HasSpriteCollision`, `GetSpriteCollisionCount` and `GetSpriteCollisions` read
the last automatic pass. Changes made afterward do not recompute those results.
Each entry identifies one other sprite and one matched type. Multiple types
against the same sprite occupy multiple entries. Each sprite stores at most
32 entries; additional matches are silently dropped. A missing entry therefore
does not prove the absence of a collision when that limit is reached.

`GetSpriteCollisions` returns borrowed storage, including a valid array with
count zero when no match was recorded. Do not modify or free it. Its results
are overwritten by the next automatic pass; copy them before retaining them.

`CheckSpritePairCollision` performs an explicit query using the requested type
bits. It ignores enabled/visible flags and the automatic reporting masks, and
does not update automatic results. It returns zero for an invalid index,
self-pair or no matching requested type.

`IsSpriteWorldPointInside` tests transformed local pixel bounds independently of
bitmap contents, using the sampling rules below. `GetSpriteBoundingBox` supplies
the forward quad's enclosing axis-aligned box; each output pointer is optional.
`GetSpritePixelAtWorldPoint` returns a
palette index from 0 through 15, or -1 for an invalid index, missing bitmap or
point outside the bitmap. These geometry queries ignore enabled/visible flags.

## Point sampling

The inverse transform converts a canvas point into local bitmap coordinates,
then truncates toward zero to select a pixel. The accepted local interval is
`-1 < coordinate < 64` on each axis. Consequently, a point less than one local
pixel beyond the top or left edge can still select edge pixel zero. Right and
bottom coordinates at 64 are rejected. This is existing sampling behavior;
containment is not an exact continuous point-in-quad test.

Flipping remaps accepted pixel coordinates after the bounds check. It changes
the selected bitmap pixel without changing whether the point is accepted.
Packed bytes use the high nibble for even x and the low nibble for odd x.

Nonfinite queries and local coordinates outside the accepted interval are
rejected before integer conversion. A nonfinite prepared transform, a
nonpositive/overflowed effective scale, or a shear determinant with magnitude
below `1e-6` makes the sampling probe noninvertible. Such queries return false
for containment and -1 for pixel sampling. Attribute setters still retain their
existing acceptance rules; these sampling checks do not validate rendered quads
or establish a work bound for every collision query.

Prepared probes are internal snapshots of transform values with borrowed bitmap
data. Use them within one serialized query. Their row-span helper estimates a
candidate x interval, widened for float rounding, rather than exact coverage.
It retains the existing half-int-range limits and broad fallback for very small
scale products. Extreme-coordinate conservativeness and collision work budgets
still require separate validation.

## Batch queries

`CheckSpritePairCollisionBatch` writes one result byte per supplied pair and
returns how many pairs matched at least one requested type. Each result uses
the same requested-bit semantics as the individual pair query. NULL arrays
or a nonpositive count return zero without writing results. Output storage
must hold at least `count` bytes.

`FindSpriteCollisionPairs` searches a supplied set of sprite indices. It skips
invalid indices and duplicates, and orders matches by the indices' first
positions in the supplied list. Each output pair names the corresponding
sprites; an optional parallel array receives matched type bits.

The return value is the total number of matching pairs, even when output
capacity is smaller. Only the first `capacity` matches are written. Output
arrays are independently optional; nonpositive capacity performs a count-only
query. NULL input or a nonpositive input count returns zero. A scratch-memory
allocation failure returns -1 without writing output arrays; that result must
not be interpreted as an empty set of collisions.

Batch calls may use worker threads and wait for them before returning. Call
them from the normal simulation context with sprite state held unchanged until
return. They do not make concurrent sprite mutation safe.
The internal pool and its lifecycle are described in `GDMF_WORKERS.md`.

Search scratch is retained for reuse and released on coordinated successful
Sprite backend shutdown. Failed scratch growth preserves prior allocated
storage and does not write partial public results. Invalid/nonfinite bounding
boxes are excluded consistently with the individual pair test.

## Relative helpers

The helper functions read an attribute, add an offset, and call its absolute
setter. `OffsetSpriteScale` adds to scale rather than multiplying it. Rotation
offsets use degrees. Invalid indices return false; a resulting scale less than
or equal to zero is rejected. These read/modify/write operations require the
same state serialization as ordinary sprite calls.

## Atlas view

The diagnostic preview is separate from the bitmap atlas used for rendering.

`ToggleSpriteAtlasView` is currently a no-op and `GetSpriteAtlasViewActive`
always returns false. Applications should not rely on an atlas preview being
available through these functions.

## Vulkan atlas

The Vulkan implementation stores one unpacked palette index per texel in an
`R8_UINT` array image, with one 64-by-64 layer per bitmap slot. It uses nearest
sampling; interpolation between palette indices would not represent a color
blend. Atlas capacity remains 1024 slots.

Resource creation checks sampled-image support, image usage/extent/array limits,
memory selection and binding. It creates the image, memory, array view and
sampler without submitting commands. Layout initialization is recorded during
frame preparation, outside the render pass, across every layer in the view.
Registered bitmaps are expanded from packed nibbles into the shared frame arena
and copied into their individual layers, returning each to the sampled layout.

A new layer is unavailable for drawing until its first complete bitmap copy has
been recorded. If staging cannot fit a new bitmap, it remains pending. An update
to a previously populated layer can leave the previous GPU contents drawable
while the new host contents wait for upload. Collision sampling uses the current
host mirror, so those two views may temporarily differ.

A fresh atlas refills every registered bitmap from the host mirror, including
slots whose dirty flags were cleared for an earlier atlas. Destroying an atlas
does not unregister or erase host bitmaps. Atlas destruction requires completed
GPU use, discarded recorded references, released descriptor users and excluded
producers; the resource helper does not wait or retire resources itself.

Layout/readiness flags and cleared dirty flags describe recording, not successful
submission or completion. Recovery after an abandoned frame still needs explicit
coordination before those flags can be trusted again.

## Vulkan graphics resources

Each swapchain image has a descriptor set binding the shared atlas at binding
zero and that image's shared Colors buffer at binding one. Sprite descriptors
do not own the atlas or palette buffers. The palette range covers 256 palettes
of 16 packed RGBA8 colors. The whole descriptor collection is published only
after every per-image set has been allocated and written; failed construction
reclaims the pool and all sets allocated from it.

The graphics pipeline uses triangle lists, dynamic viewport/scissor, no culling,
and one sample per pixel. Vertex data carries transformed position, UV, bitmap
layer, palette, opacity and index-zero visibility, plus a retained grayscale
shader mode that the current atlas preview API does not enable.

Color blending uses source alpha and one-minus-source-alpha. The existing alpha
blend replaces destination alpha with source alpha; it does not accumulate it
the same way as RGB. This behavior is preserved for the composition review.

Each image's vertex storage is independently mapped and host coherent. Reuse
requires completion of that image's previous GPU work. Growth allocates, binds
and maps replacement storage before releasing the old buffer, keeping the old
allocation and contents if any step fails. Successful growth adds 256 vertices
of capacity; the caller writes the complete new frame data before drawing.

These helpers take caller-owned results and borrowed device/render-pass/cache
inputs. They do not submit work or wait for idle. Cleanup and replacement require
resolved GPU use and discarded recorded references. Release pipelines and their
users before descriptor layouts/pools, then release descriptor users before
their borrowed atlas and palette resources.

## Prepared frames

Each swapchain image has its own prepared Sprite frame. Preparation selects
enabled, visible sprites with registered bitmaps and initialized atlas layers.
Sprites with nonfinite projected corners are omitted. Canvas dimensions must be
finite and positive, and the viewport must have a nonzero extent.

Vertices are grouped by priority, retaining sprite-index order within each
group. The compositor records priorities from 255 down to zero, so lower
priorities draw later. Within a priority, later sprite indices draw over earlier
ones. Flipping changes texture coordinates, leaving the world quad unchanged.

Preparation captures the pipeline, descriptor set, viewport and priority slices.
Recording uses that snapshot rather than reading live Sprite attributes or
window dimensions. Host state must remain unchanged during preparation. Edits
afterward take effect in a later preparation pass.

A failed preparation clears the frame's published count and draw slices while
retaining its vertex allocation for retry. A successful empty frame records no
draws. Preparing an image slot again requires completion of its previous GPU use
and discarded command references; invalidating the snapshot does not establish
either condition. Captured graphics handles remain borrowed and must outlive
recording and execution.

The prepared count describes selected sprites, not confirmed presentation.
Collision processing and bitmap transfers are separate steps that run before
draw-list preparation, including frames that cannot produce a draw list.

## Backend lifecycle

The Sprite backend owns the atlas, descriptor collection, pipeline and image-slot
frames. It borrows the renderer's device, render pass and palette buffers.
Ordinary allocation failures leave partial ownership available for cleanup or
retry; they do not erase host attributes or bitmaps. Automatic collision reports
are refreshed before setup and draw-list preparation. Bitmap transfers can be
recorded even when descriptor or pipeline setup is still unavailable.

The renderer marks the backend quiesced only after successful GPU completion and
discarding recorded command references. Before replacing shared palette buffers
or the render pass, it releases Sprite frames, pipeline and descriptors. The
atlas remains available across swapchain recreation. Later preparation rebuilds
the image-dependent resources against the replacement renderer objects.

Shutdown follows the same completion rules, then releases the atlas. Calling
Sprite shutdown with unresolved references retains resources and stops further
Sprite preparation and recording. An unannounced change to the borrowed device,
render pass, image count or bound palette buffers also stops preparation. The
renderer checks this status before acquisition, preparation and submission.
Recovery requires coordinated completion and checked cleanup before a fresh
lifecycle; the backend does not wait for GPU completion on its own.

Renderer access, host state and lifecycle operations still require the engine's
serialization. Atomic stop-status polling does not make resource access safe
from arbitrary threads. Recorded bitmap flags also retain the abandoned-frame
limitation described under Vulkan atlas.
