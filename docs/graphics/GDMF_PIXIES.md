# Pixies

A pixie is a persistent drawing surface or collection of geometry. Its contents
remain until changed or released. Include `pixies/gdmf_pixies.h` for the public
interface; that header also includes the command and format definitions.

## Modes

| Mode | Retained content | How it appears |
| --- | --- | --- |
| `PIXIE_MODE_TEXTURE` (0) | RGBA pixels | A textured rectangle |
| `PIXIE_MODE_LIVE` (1) | Generated vertices | Geometry drawn each frame |
| `PIXIE_MODE_MASK` (2) | Eight-bit coverage | Controls items at the same priority |

Texture and mask edits update host output buffers, which the renderer uploads
when needed. Texture decoding can also queue supported compute work. After GPU
decoding is selected for a texture, its host output is stale; host pixel edits
and formats requiring a host decode are refused. Reinitialization restores a
host-writable pixie and discards the previous contents. Compute availability
and queue capacity affect which path is taken; command acceptance is not GPU
completion.

Live drawing appends geometry. Moving a figure by drawing it again leaves the
old figure in place. Clear and redraw moving content; unchanged content needs
no resubmission. `CLEAR` discards accumulated geometry and can then add a
full-area color rectangle. The current implementation stores at most 4096
six-vertex primitives per live pixie; reaching that capacity is not reliably
reported as a command refusal.

## Lifecycle

There are sixteen IDs, 0 through `MAX_PIXIES - 1`. `InitPixie` requires a known
mode and positive logical output dimensions. Every initialized pixie receives
4 MiB of zeroed RAM. Texture mode also allocates a zeroed RGBA buffer, mask mode
a zeroed coverage buffer, and live mode a vertex array.

The output dimensions define local drawing coordinates. Display dimensions
initially match them and can change independently. Initialization leaves both
enabled and shown false. Set the display attributes or enable it, and call
`ShowPixie`, to make it eligible for rendering.

Initialization returns false for invalid arguments, oversized output, allocation
failure, or failure to release an existing pixie's backend ownership.
Reinitializing an existing ID first releases its contents; a subsequent
allocation failure does not restore that pixie. `ReleasePixie` returns false
for invalid or uninitialized IDs or an unsuccessful backend handoff. Failed
handoff retains the logical state so release can be retried. `ShutdownPixies`
releases the collection at a renderer-quiesced lifecycle boundary.
Lifecycle operations and command submission require the engine's serialized
state/lifetime boundary; these calls do not supply a concurrent ownership API.

## Host state

Logical state and Vulkan ownership have separate interfaces. The host state owns
RAM, host output, live vertices, attributes, transform slots, and diagnostic text.
The backend owns its GPU resources and pending decode requests. Before logical
release succeeds, the backend must transfer or release ownership and prevent
queued work from affecting a replacement pixie at the same ID.

Output pixel counts are limited to `INT_MAX` because current host drawing uses
signed integer pixel indices. Allocation sizes must also fit `size_t` for their
element type. Rejected size checks happen before an existing pixie is released.
After successful backend handoff, a reinitialization allocation failure still
leaves the old pixie released, as described above.

If a release handoff fails during `ShutdownPixies`, that logical state is retained,
an error is printed, and shared backend shutdown is not called. Other pixies
can already have been released; shutdown is not an atomic collection operation.
If shared backend shutdown fails, its remaining resources stay owned by that
backend. The public function returns void, so callers cannot infer successful
cleanup from a return code. The lifetime boundary must remain closed while
retained resources are resolved.

## Placement and visibility

Display position and size use reference-canvas coordinates. Display extents
must be positive. The output buffer dimensions remain the same when display
size changes. Lower numeric priorities are drawn later by the current renderer.
Enabled and shown are separate state: hiding a pixie preserves its contents and
does not change its enabled flag.

Most setters return false for invalid or uninitialized IDs. Command-backed
setters can also refuse malformed arguments. Some setters write state directly;
the command packet is not the only mutation route. There is no general guarantee
that all setters reject nonfinite numeric input.

## Drawing and patterns

Commands draw points, lines, line lists, or polygons. Filled polygons use the
even-odd rule and support concave or self-intersecting outlines. Live mode
supports polygon outlines but refuses filled and patterned polygon fills.
Live mode also refuses image unpacking.

`SetPixieDrawPattern` supplies a persistent 16-bit line pattern. Bits are tested
in order and repeat every sixteen raster steps. `0xFFFF` is solid; zero draws
nothing. Texture and mask lines test steps on the host; live lines emit geometry
for visible pattern runs.

Patterned texture fills use a tiled RGBA8 bitmap from pixie RAM when one is set.
The pattern stays in the pixie's local space; moving the polygon does not move
the pattern. Alpha-zero bitmap pixels are skipped. Without a bitmap, fills test
draw-pattern bit `(x + y) & 15` to form a stipple. Mask patterned fills use the
stipple rather than a color bitmap. Clearing a pattern bitmap restores stipple.

Packet layouts, coordinate encodings, and transform behavior are described in
GDMF_PIXIES_COMMANDS.md. Packed image formats are in GDMF_PIXIES_FORMATS.md.

## Masks

A mask draws no visible color of its own. Items at its priority are composed
through its coverage; items at other priorities keep their usual behavior.
If multiple masks select the same priority, the lowest initialized eligible
ID wins. The renderer has a finite mask-target capacity and can fall back to
unmasked drawing when it is exhausted; see GDMF_VULKAN.md.

Default polarity hides that priority's items at coverage 255 and shows them at
coverage 0. `SetPixieMaskInvert(true)` reverses this. Outside the mask display
rectangle, coverage is zero before inversion, so an inverted mask hides its
priority outside the rectangle. Intermediate coverage scales transparency.

Mask `CLEAR`, `PLOT`, and drawing commands take coverage from the low byte of
their color argument. `UNPACK` uses the separate `PixieMaskFormat` namespace.
`ReadPixieMask` copies the host coverage buffer, including changes not yet
uploaded. It is not a GPU readback. It copies at most the supplied capacity
and returns the byte count, or zero for invalid/non-mask IDs or unusable output.
The bytes are binary coverage values and have no string terminator.

## Rotation and filtering

Display rotation uses clockwise radians in screen coordinates, where y grows
downward. Transform-slot rotation instead takes degrees. These are different
operations: one turns the displayed result, the other changes coordinates as
commands generate content.

Hotspots are measured in unscaled local pixels and anchor display placement.
Rotation normally uses the display center; `SetPixieRotateAroundHotspot` selects
the hotspot as pivot. Mask rotation is refused because current mask composition
uses an axis-aligned rectangle.

Nearest filtering is the default. Enabling linear filtering returns false if
the shared linear sampler has not been created or is unavailable, and leaves
linear filtering disabled. This can happen before renderer preparation even
on hardware that supports it; false is not solely a device-capability verdict.

## RAM and diagnostics

`WritePixieRAM` copies caller bytes into the pixie's RAM and permits overlapping
source and destination ranges. It retains no caller buffer. The complete range
must fit; NULL data is refused even for a zero-byte write. `GetPixieRAMSize`
returns 4 MiB for an initialized pixie, otherwise zero.

`GetPixieRAMReadPtr` returns a borrowed const view, or NULL for an unavailable
pixie. Release or reinitialization invalidates it. The memory remains mutable
through engine operations; const does not make it an immutable snapshot. Hold
the appropriate state/lifetime boundary while reading. No writable RAM pointer
is supplied by this interface.

`ReadPixieString` copies diagnostic text, returns its byte count excluding NUL,
and terminates the result whenever capacity is nonzero. NULL output or zero
capacity returns zero. For invalid or uninitialized IDs, it reads the shared
last diagnostic for an unavailable ID, which may concern another ID. A successful
command does not necessarily clear a previous message. Direct setters do not
all write diagnostic reasons when they fail.

## Getter defaults

Invalid or uninitialized IDs return zero or false for numeric and Boolean
getters, except `GetPixieMode` returns `PIXIE_MODE_TEXTURE` and
`GetPixieDrawPattern` returns `0xFFFF`. These defaults are not evidence that
a pixie exists; use `GetPixieInitialized` to distinguish that case.

## Vulkan images

The Vulkan image helpers keep shared samplers, descriptor layout and descriptor
pool in a caller-owned context. Each texture or mask owns its image, memory,
view, descriptors and upload slots. The Pixies backend owns these results and
connects them to the renderer's prepare, record and lifecycle hooks.

Textures use RGBA8 with sampled, transfer-destination and storage usage. Masks
use R8 with sampled and transfer-destination usage. Creation checks the device's
support for that combination and its image dimensions. Unsupported texture
storage usage currently prevents creation; a portable host-only fallback still
needs backend policy.

Nearest and linear filtering use separate descriptor sets, so changing selection
does not rewrite a set referenced by an earlier frame. Linear sampler or set
allocation failure leaves nearest sampling available. A linear descriptor also
requires linear filtering support for the image's format. Descriptor pool
capacity remains two sets per pixie.

Image creation records no commands and leaves the layout undefined. The first
successful full upload initializes every pixel and ends in
`SHADER_READ_ONLY_OPTIMAL`. Later uploads start and finish in that layout.
Compute recording performs its own transition to `GENERAL` and restores the
sampling layout afterward. Images must not be sampled or decoded before their
initial upload has been recorded in the proper order.

Upload buffers are allocated and mapped lazily, one slot per swapchain image.
The renderer must finish an image index's prior work before overwriting its
slot. Creation, allocation, binding and mapping failures retain pending dirty
contents for retry. A quiesced staging-ring reset changes the slot count without
discarding image pixels or descriptors.

Immediate destruction requires completed GPU work, discarded recorded references
and excluded users. If freeing descriptors fails, the image and upload resources
remain owned for retry. Shared context destruction comes after all images and
any deferred references have been released. These helpers do not wait or prove
completion; deferred ownership transfer remains part of backend integration.

Upload layout bookkeeping follows command recording. Recovering that state and
dirty contents when a recorded frame is abandoned remains outstanding work.

## Frame preparation

The graphics helpers share one pipeline builder and one vertex-buffer allocator
between texture and live mode. Texture vertices contain position and UV; live
vertices contain position and color and require no texture descriptor. Both
pipelines retain triangle lists, dynamic viewport/scissor, no culling and the
existing color blending. Alpha currently replaces the destination alpha rather
than accumulating it; changing that requires a separate composition review.

Each swapchain image needs its own frame object. Its persistently mapped,
coherent vertex buffers may be reused or grown only after prior GPU work and
recorded references for that slot are finished. Growth creates a replacement
before releasing the old allocation, so allocation, binding or mapping failure
retains the previous buffer for retry.

Preparation runs under the state lock. It creates missing images, records dirty
uploads, then drains queued decodes. A decode cannot pass a pending dirty upload
for its target. An initialized image can keep showing its previous contents
while an upload retries. A new image is excluded until its first full upload
has initialized it.

Preparation freezes draw priorities, descriptors, vertex ranges, scissors and
mask information into the frame. Texture quads use current display attributes;
live vertices retain their previously generated positions. Live geometry is not
cleared during preparation, so another render without a logic tick presents the
same retained content. The lowest-numbered ready, enabled and shown mask wins
at each priority.

Priority recording reads only that snapshot. It draws IDs in ascending order
within the requested priority, switches pipelines when the mode changes and
restores the viewport scissor afterward. The main compositor still selects
priorities from 255 down to zero and applies the separate mask composition pass.
Scissors enclose rotated display bounds, round outward and clamp to the viewport
before integer conversion. Nonfinite or entirely outside bounds omit the draw.

A successful preparation publishes a snapshot, even if unavailable resources
omit some items. `unavailable_images` reports affected image IDs as a bit mask;
the snapshot's drawn flags identify which vertex-backed items are ready. Invalid
preparation input invalidates the snapshot. Callers must retain its borrowed
image, descriptor and pipeline handles through recording and GPU use.

The backend supplies these helpers through the renderer hooks. Deferred-release
completion and replay of uploads/decodes from abandoned frames still need
further verification and recovery work.

## Backend lifecycle

The backend owns shared image resources, pipelines, per-ID images and per-image
frame slots. Preparation initializes them lazily and retries ordinary allocation
failures. Live drawing can still prepare if shared image setup is unavailable.
Recording and mask queries use the most recently prepared frame snapshot.

During ordinary release, descriptors, images and upload slots transfer to the
renderer retirement queue. Each handle is cleared only after its transfer
succeeds. If a transfer fails, logical Pixie state and remaining handles stay
owned, and further acquisition/preparation stops. Release can retry without
transferring the same handle twice. A renderer already stopped may still require
shutdown/reinitialization before rendering resumes.

The retirement queue uses a frame countdown. That countdown alone does not prove
completion for every recorded or submitted reference; completion-based retirement
still needs hardening.

The coordinator notifies Pixies of quiescence only after a successful GPU wait
and render-command-pool reset. That invalidates published snapshots and permits
immediate resource cleanup. Failed quiescence leaves resources retained. The
notification requires producers to be excluded; it performs no wait itself.

Renderer shutdown explicitly calls `ShutdownPixies` and checks backend cleanup
before destroying the device or locks. Retained state/resources stop teardown
and prevent a new renderer lifecycle. Public `ShutdownPixies` remains void;
failure is reported through diagnostics and the renderer's internal result.

After quiesced swapchain recreation, Pixies releases only pipelines, frame vertex
buffers and upload rings. Images, descriptors, GPU-resident pixels and logical
state survive. New frame resources use the new render pass and image count.
Changing those inputs without the lifecycle hook stops preparation rather than
reusing incompatible resources.
