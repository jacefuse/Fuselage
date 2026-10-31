# GDMF Vulkan implementation

## Internal interfaces

The Vulkan interfaces are engine implementation details:

| Header | Responsibility |
|---|---|
| `gdmf_vulkan.h` | Renderer lifecycle and acquire/prepare/submit operations |
| `gdmf_vulkan_instance.h` | Instance ownership and optional debug reporting |
| `gdmf_vulkan_physical_device.h` | Physical-device selection and result metadata |
| `gdmf_vulkan_device.h` | Logical device, queue handles, and pipeline-cache ownership |
| `gdmf_vulkan_swapchain.h` | Swapchain metadata, image arrays, and image-view ownership |
| `gdmf_vulkan_render_pass.h` | Presentation render pass and framebuffer collections |
| `gdmf_vulkan_commands.h` | Render/upload command pools and per-image synchronization |
| `gdmf_vulkan_palettes.h` | Per-image packed palette storage and update tracking |
| `gdmf_vulkan_memory.h` | Memory-type selection and host-visible allocation |
| `gdmf_vulkan_staging.h` | Per-image staging storage, reset, and suballocation |
| `gdmf_vulkan_uploads.h` | Serialized one-time commands and unresolved upload handles |
| `gdmf_vulkan_retirement.h` | Deferred resource ownership, countdown, and draining |
| `gdmf_vulkan_capture_ppm.h` | CPU conversion and binary PPM image output |
| `gdmf_vulkan_capture_requests.h` | Synchronized capture request and processing state |
| `gdmf_vulkan_capture_readback.h` | Owned-source readback and retained recovery storage |
| `gdmf_vulkan_mask_targets.h` | Lazy per-image offscreen mask color targets |
| `gdmf_vulkan_masks.h` | Mask compositor pass, sampler, descriptors, pipeline, and push layout |
| `gdmf_vulkan_geometry.h` | Aspect-ratio viewport fitting and mask rectangle conversion |
| `gdmf_vulkan_composition.h` | Mask snapshot, offscreen passes, and priority draw recording |
| `gdmf_vulkan_renderer_private.h` | Coordinator-only lifecycle/frame state, not a layer interface |
| `gdmf_vulkan_internal.h` | Memory, uploads, staging, resource retirement, state access, and compute recording |
| `gdmf_vulkan_surface.h` | Platform surface creation |
| `gdmf_vulkan_surface_linux.h` | Wayland/X11 surface adapter declarations |
| `gdmf_vulkan_layers.h` | Layer preparation, recording, masks, and swapchain invalidation |

The internal header includes the surface, layer, memory, staging, upload, and retirement
headers for existing consumers. These interfaces use Vulkan types. Separating their declarations
does not turn them into a backend-neutral rendering interface.

The build selects the platform surface implementation. Windows and macOS have
direct implementations; Linux dispatches to the surface matching the active
Wayland or X11 window backend. The native window must exist before renderer
initialization creates a surface.

## Instance creation and debug reporting

The instance module requests Vulkan API 1.0. It requires `VK_KHR_surface` and
the extension returned by the selected platform surface adapter. When offered,
it also enables portability enumeration and physical-device-properties2.
The latter supports the instance-level requirements of portability devices
selected later. Requesting API 1.0 alone does not verify compatibility of every
renderer feature or shader.

DEBUG builds enable `VK_EXT_debug_utils` and `VK_LAYER_KHRONOS_validation`
when found. After instance creation, they attempt to create a messenger for
warning and error messages. Failure to create that messenger does not fail
instance creation. Its callback prints the message and returns `VK_FALSE`.
The messenger is created after the instance, so it does not collect messages
emitted during instance creation.

`gdmf_vulkan_instance_create` returns `0` on successful instance creation or
`-1` on failure. Call it once per renderer lifecycle on the main/render thread.
The getter returns a borrowed handle. Destroy dependent devices and surfaces
before calling instance destruction, which releases the debug messenger first.
The handle is reset after destruction; repeated destruction is harmless.

## Physical-device selection

Selection runs after the instance and native presentation surface exist. A
candidate must provide graphics and presentation queues, `VK_KHR_swapchain`,
a successful surface-capabilities query, and nonempty format and present-mode
lists. Selection prefers the first queue family supporting both graphics and
presentation; otherwise it uses the first suitable family for each separately.
These checks are an initial suitability filter, not proof that later device
creation or every renderer operation will succeed.

Suitable devices receive the following score:

- 100,000 points for a discrete GPU.
- The maximum 2D image dimension divided by 16, rounded down.
- 100 points per whole GiB of reported device-local heaps, capped at 64 GiB
  for scoring.

The highest score wins; the first enumerated candidate wins a tie. The heap
cap limits the influence of unusually large driver-reported values. Device-local
heaps may represent shared memory, so the diagnostic labels their sum as
device-local heaps rather than dedicated VRAM. This score is a selection
heuristic, not a performance measurement.

`gdmf_vulkan_select_physical_device` returns `0` and writes the selection on
success, or returns `-1` without writing it on failure. The caller owns the
copied properties, queue-family indices, and score. The physical-device handle
is borrowed from the instance; selection creates no logical device or queues.
The caller must retain the result for later renderer setup and device-info
queries. Candidate diagnostics identify rejection reasons and scores.

## Logical-device creation

Logical-device creation consumes a successful physical-device selection. It
requests one queue at priority 1.0 from the selected graphics family and,
when presentation uses another family, one queue from that family as well.
When both families are the same, the two returned queue handles identify the
same queue. No optional device features are requested.

The device enables `VK_KHR_swapchain`. It also enables
`VK_KHR_portability_subset` when found during extension enumeration. Instance
setup and device selection must supply the matching prerequisites.

`gdmf_vulkan_device_create` returns `0` on success or `-1` on device-creation
failure. It writes its caller-owned result only on success. That result owns
the device and optional pipeline cache; queue handles belong to the device.
The cache starts empty, is not saved to disk, and may be `VK_NULL_HANDLE` if
cache creation fails. Rendering must support that fallback.

Creating queue handles does not establish synchronization. The renderer must
initialize its queue, upload, and resource-retirement locks before concurrent
work begins. Shared graphics/presentation queues must use the same queue lock.
Where upload and queue locks are both needed, acquire the upload lock first.

Before destroying the result, stop its users, finish outstanding GPU work,
and release all dependent resources. `gdmf_vulkan_device_destroy` releases the
pipeline cache, destroys the device, and clears all four handles. It does not
wait for idle or clean up other device-owned resources. Repeated destruction
of a cleared result does nothing. Do not overwrite a live result with another
creation call or copy it to create a second owner.

## Swapchain creation

The coordinator supplies the selected physical device, logical device,
presentation surface, current window pixel dimensions as a pair, and VSync
setting. The swapchain module does not read mutable window or renderer globals.
It uses the surface's fixed extent when provided; otherwise it clamps the
supplied dimensions to the surface's minimum and maximum image extents.

Format selection prefers `VK_FORMAT_B8G8R8A8_UNORM` with
`VK_COLOR_SPACE_SRGB_NONLINEAR_KHR`, falling back to the first offered format.
The UNORM preference reflects the existing rendering path's use of encoded
palette values. It is not a general color-management or HDR policy.

With VSync enabled, creation requests FIFO presentation. With VSync disabled,
it prefers IMMEDIATE, then MAILBOX, then FIFO. If allocation of the optional
present-mode list fails, it also falls back to FIFO. Disabling VSync therefore
does not promise a particular frame rate or presentation mode; IMMEDIATE may
tear. The result records the selected mode.

Creation requests one more image than the surface minimum, limited by a nonzero
maximum. The returned image count comes from the created swapchain and can
differ from the requested count. Separate graphics/presentation queue families
use concurrent sharing; a shared family uses exclusive sharing.

Images request color-attachment usage and transfer-source usage when advertised.
The result records these flags. Capture must not assume transfer-source support
when that flag is absent. Creation currently requests opaque composite alpha
and uses the surface's current transform.

`gdmf_vulkan_swapchain_create` returns `0` on success or `-1` on failure. It owns
the swapchain handle, image views, and two allocated arrays; the images themselves
belong to the swapchain. Failed creation releases partial resources and leaves
the caller's output unchanged. Empty format/image lists fail initialization.
Query failures, including `VK_INCOMPLETE`, fail this attempt rather than using
partial data. Allocation failures and empty lists use corresponding error
values in diagnostics; the function still returns only `-1`.

Stop use, complete GPU/presentation work, and destroy dependent framebuffers
before calling destruction. Destruction releases views before the swapchain,
frees both arrays, and clears the result. It does not wait for outstanding work.
Recreation and invalidation of other renderer resources remain coordinated by
the renderer; this module does not replace a live swapchain in place.

## Render pass and framebuffers

The presentation render pass has one color attachment using the swapchain
format and one sample per pixel. It clears that attachment at the start and
stores the rendered result. Its initial layout is undefined and its final
layout is `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`. There is one graphics subpass,
with the color attachment in color-attachment-optimal layout, and one external
dependency targeting color-attachment output writes. These are the existing
renderer settings; this module does not implement depth or multisampling.

The clear color is supplied when recording the render pass, not during its
creation. The render pass does not retain a swapchain handle or choose when
images may be reused.

Each swapchain image view gets one framebuffer at the swapchain's full pixel
extent, with one attachment and one layer. The caller owns the framebuffer
collection, including its array and stored count. The collection borrows the
render pass and image views. Its destruction uses its own count, independent
of later changes to swapchain metadata.

Both creation functions return `0` on success or `-1` on failure, writing
outputs only on success. A failed framebuffer creation destroys only the
framebuffers already created and frees the array. Destruction clears owned
handles and can be repeated on a cleared result. None of these helpers waits
for GPU work.

During recreation, finish the relevant work and destroy old framebuffers
before releasing their image views. Recreate framebuffers for the new image
views and extent. The existing renderer rebuilds the render pass when the
swapchain format changes and invalidates dependent layer resources during
recreation. That coordination remains outside this module. During teardown,
release framebuffers before the borrowed render pass or image views, and
release all owned objects before their logical device.

## Commands and frame synchronization

Two command pools use the selected graphics queue family. The render pool
allows individual command-buffer resets and belongs to render-thread frame
work. The transient upload pool serves serialized one-time uploads. These
separate pools preserve the distinction between frame recording and uploads
that may originate on other threads. Pool creation does not provide locking;
the upload service must serialize its pool use, and queue access must follow
the renderer's shared queue-lock contract.

The frame-command collection owns one primary command buffer, one acquire
semaphore, one render-finished semaphore, and one initially signaled fence per
swapchain image, plus a spare acquire semaphore. The coordinator rotates the
spare with the acquired image's semaphore during acquisition. Initial fence
signaling allows the first use of each slot without waiting for a prior frame.
Subsequent reuse still depends on the acquire/wait/reset/submit/present sequence.
Creating these objects alone does not establish correct frame synchronization.

Pool and frame-command creation return `0` on success or `-1` on failure.
Each writes its caller-owned output only on success and cleans up partial
resources on failure. Frame-command creation rejects zero images. Stored
image and command-buffer counts make cleanup independent of mutable swapchain
metadata; the command-buffer count is set only after successful allocation.

The frame-command collection borrows its render pool and owns neither staging
storage nor swapchain images. The coordinator must create and release per-image
staging alongside the matching frame resources. Stop users and complete GPU
and presentation work before releasing synchronization objects. Release frame
commands before their render pool and both pools before the logical device.
Destruction clears results, can be repeated on cleared results, and does not
wait for outstanding work. Do not copy results to create additional owners or
overwrite live collections with new creation calls.

## Shared palette buffers

Each swapchain image has one shared storage buffer for the 256 palettes of
16 colors. Entries are packed through `PackColorsRGBA8` in palette-major order,
occupying 16 KiB per image. Sprites and tiles borrow these buffers rather than
maintaining separate copies. Deep palettes are separate storage and are not
uploaded by this module.

The caller owns a collection of per-image entries, each grouping its buffer,
allocated memory, persistent mapping, uploaded generation, and first-upload
flag. Creation uses the renderer's host-visible, coherent allocator with a
device-local preference. The caller supplies the logical device and matching
physical-device memory properties explicitly. Mapping persists until memory is freed; coherent
memory avoids explicit flush calls but does not remove synchronization needs.

Prepare an image's palette buffer only after its previous GPU use completes
and while holding the lock shared with palette setters. The first preparation
uploads the full table, even if the generation is zero. Later preparation
skips the copy when the stored generation matches `GetColorsGeneration`.
Invalid or unmapped image slots are ignored. Each image updates independently.
Use palette setters for changes; direct writes bypass generation tracking.

The first-upload flag separates uninitialized storage from a valid generation
zero. It does not detect a full counter cycle between observations that ends
at the same stored value. Palette and renderer access still require a shared
synchronization contract.

Creation returns `0` on success or `-1` on failure, including zero images. Failed
creation releases partial buffers and memory and leaves caller output unchanged.
Buffer binding is checked before mapping. Buffer lookup returns a borrowed
handle or `VK_NULL_HANDLE` for unavailable or invalid slots.

Before destruction, stop users and complete GPU work. Retire descriptor users
before releasing their buffers. Destruction releases buffers before their
memory, frees the collection, and clears it. It does not wait. The coordinator
must recreate the collection with the swapchain image count and make dependent
descriptors refer to the new buffers.

## Memory selection and allocation

Memory helpers use the physical-device memory properties retained by selection;
they do not query or read a global renderer device. Pass the matching logical
device, memory properties, and resource requirements to allocation.

`gdmf_find_memory_type` returns the first type allowed by the requirements'
type mask that contains all requested property flags, or `UINT32_MAX` if no
type matches. It uses enumeration order, not a performance ranking.

`gdmf_allocate_host_visible_prefer_device_local` first looks for a type that
is device-local, host-visible, and coherent. It also finds the first allowed
host-visible, coherent fallback. If these are different types, it tries the
preferred type first and falls back after allocation failure. If they are the
same type, it makes only one attempt. No matching fallback returns
`VK_ERROR_OUT_OF_DEVICE_MEMORY`; otherwise the final allocation result is
returned. A failed preferred attempt is not logged by this helper.

The requested allocation size comes from the resource's memory requirements.
The caller's output changes only on successful allocation. The caller owns
the returned memory and must bind, map, synchronize, and free it as required
by the resource. These helpers do none of those operations, maintain no
mutable allocation state, and do not synchronize resource producers or device
destruction. Device-local and host-visible flags describe the selected memory
type; they do not establish a particular hardware layout or performance gain.

## One-time upload commands

The caller-owned upload context borrows a logical device, graphics queue,
transient upload pool, upload lock, and the renderer's shared queue lock.
Both locks must be initialized before use and remain valid until users stop
and retained handles are released. This service uses the existing internal
`BASE/fuselage_sync.h` interface.

`gdmf_execute_one_time_commands` takes that context, a recording callback, and
callback data. It allocates one primary command buffer, begins it for one-time
submission, calls the recorder, ends recording, creates an unsignaled fence,
submits, and waits for that fence. It returns `GDMF_VULKAN_UPLOAD_COMPLETE` (`0`)
after a successful wait, `GDMF_VULKAN_UPLOAD_FAILED` (`-1`) for setup/recording
failure before submission, or `GDMF_VULKAN_UPLOAD_UNRESOLVED` (`-2`) after
submit/wait errors or when earlier unresolved work blocks the context.
A null callback is rejected. The void callback cannot report
recording failure through this interface.

The upload lock covers recording, submission, waiting, and command cleanup.
The shared queue lock covers submission only and is acquired after the upload
lock. Waiting does not hold the queue lock. A callback must not reenter this
service or acquire either lock, and callers must not enter holding those locks.
Use frame staging for recurring uploads where possible; this path can block
until all queue work ahead of the upload has completed as well as the upload.

Failures before submission release successfully created command/fence handles.
After a submit or wait error, the context retains both handles and rejects new
uploads. This prevents freeing those objects while completion remains unresolved.
It does not protect the buffers, images, or other resources referenced by the
recorded commands: callers must keep those alive until recovery establishes
completion. Inspect or modify retained state only with the upload lock held or
after stopping its users.

After stopping producers and establishing completion externally,
`gdmf_vulkan_uploads_release_completed` releases retained handles under the upload
lock and clears them. It neither waits nor determines whether release is safe.
The coordinator must handle device loss and failed waits explicitly; a failed
idle wait alone is not confirmation of normal completion. Keep the borrowed
pool and device alive through this cleanup. Normal successful calls release
their own handles before returning.

## Capture request handling

The caller-owned request slot borrows an initialized mutex. Initialize it
before request producers start, and stop its users before releasing the mutex
or reinitializing the slot. It holds one copied path of up to 511 characters
plus its terminator, with idle, queued, and processing states. These operations
perform no filesystem access and do not select a frame or capture an image.

Submit and pending queries can run on any thread with a valid live context.
Submit returns `true` only when it copies a nonempty path into an idle slot.
It rejects null, oversized, or unterminated-within-capacity paths and any
second request while queued or processing. The caller must keep the source
path stable during submission. A mutex protects both the copied path and
state; a volatile flag is not used for publication.

The render-thread consumer takes a queued request by copying it to local
storage and marking it processing under the mutex. The helper returns with
the mutex released, so readback and file writing do not hold it. Pending stays
true until that same consumer finishes a safely resolved attempt. Taking with
no queued request leaves output unchanged; finishing with no processing
request returns false and does not discard a queued request.

Finishing clears the slot whether the resolved capture attempt saved a file
or failed. It is not a saved-file status. If GPU work remains unresolved, the
consumer must retain processing state and protect referenced resources through
recovery. The public `GDMF_CaptureFrame` and `GDMF_CapturePending` wrappers use
this slot. Submission rejects uninitialized or stopped renderers.

## Capture readback

The render-thread readback helper takes a caller-owned context with a borrowed
logical device and upload service, copied memory properties, and retained
readback handles for recovery. The caller supplies a source image, format,
extent, image-usage flags, viewport, and canvas dimensions as one metadata set.

The source must be application-owned in `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`,
with no active presentation or competing use. A device-idle wait does not
reacquire ownership of an image already passed to presentation. The coordinator
calls readback after frame submission and before presentation, while it still
holds the acquired image. The helper waits for rendering before copying; native
validation is still needed for the complete dependency chain.

Readback rejects absent transfer-source usage, unsupported four-channel format,
invalid dimensions, and unrepresentable byte sizes. It checks a device-idle
wait under the shared queue lock, then creates a transfer-destination buffer
with host-visible, coherent memory and checks binding. A one-time command
transitions the source to transfer-source layout, copies tightly packed pixels,
and restores presentation layout. The existing barrier settings are preserved;
their suitability still depends on the coordinator's actual frame dependencies
and queue ownership.

After completed submission, the helper maps the buffer and passes it to the
PPM writer. It unmaps, releases buffer/memory, and returns
`GDMF_CAPTURE_READBACK_SAVED` (`0`) only when file output succeeds. Resolved
setup/readback/write failures return `GDMF_CAPTURE_READBACK_FAILED` (`-1`).
No new readback work is submitted after an unsuccessful initial idle wait;
that failure does not establish completion of other renderer work.

An unresolved upload returns `GDMF_CAPTURE_READBACK_UNRESOLVED` (`-2`) and
retains the readback buffer/memory. Repeated calls remain blocked while those
handles are retained. Keep the source image, its swapchain, and other referenced
resources alive too. Keep the request in processing state during recovery.
After stopping producers and independently establishing completion, release
retained upload commands first, then call the readback completed-release helper.
That helper does not wait or establish safe release. Device-loss recovery must
be handled by the coordinator before normal teardown proceeds.

The request module, readback helper, and writer remain separate owners/services,
connected through that coordinator boundary.

## Capture image output

The PPM writer consumes completed, CPU-readable readback data. Its borrowed
input includes the available byte count, tightly packed image dimensions,
Vulkan pixel format, viewport rectangle, and logical canvas dimensions. It
makes no Vulkan calls and does not establish GPU completion or image ownership.

Output is binary PPM (P6) with three 8-bit RGB channels; alpha is discarded.
RGBA8 and BGRA8 UNORM/SRGB formats are supported. BGRA channels are reordered;
other formats fail rather than being interpreted as four 8-bit channels.
The writer does not apply gamma conversion or tone mapping.

The output uses canvas dimensions, or image dimensions if either canvas
dimension is zero. Each output pixel samples the center of its footprint in
the supplied viewport, clamped to the source image bounds. This is a sampling
policy, not a guarantee of reconstructing the original canvas after filtering,
scaling, or compositing. The comment in the PPM header records canvas size,
source size, and viewport rectangle. Source rows retain their supplied order.

`gdmf_vulkan_capture_write_ppm` returns `true` only after input validation,
row allocation, opening, header/row writes, and file close succeed. It checks
buffer sizes and address arithmetic before reading pixels. Row allocation
happens before opening, so allocation failure does not truncate an existing
file. Opening for output replaces an existing file; later write/close failure
can leave a partial file. This interface does not provide atomic publication
or report capture-request completion by itself.

## Mask offscreen targets

Mask composition renders one priority's content to an offscreen color target
before combining it with mask coverage. Target storage is separate from the
compositor pipeline, descriptors, and priority traversal.

The caller-owned target collection copies a configuration containing its
borrowed device/render pass, memory properties, format, extent, image count,
and mask slots per image. Setup checks nonzero geometry/counts and the count
product, then allocates zeroed bookkeeping. It returns `0` on success or `-1`
on invalid configuration or allocation failure, leaving output unchanged on
failure. No GPU images are created during this setup.

Ensure takes an image index and mask-slot index, checks each independently,
and creates that target only when needed. Each target has a single-sample,
single-layer 2D image with optimal tiling, color-attachment/sampled usage,
exclusive sharing, and undefined initial layout. It uses a matching
device-local memory type, one color view, and one framebuffer for the borrowed
offscreen render pass at the configured extent.

Ensure returns a borrowed pointer to a completed target, or `NULL` on invalid
indices or failed creation. Failed binding or creation releases all partial
resources and leaves the slot empty for retry. Existing targets are reused;
the collection does not resize them in place or choose a mask-capacity policy.

The render thread must complete an image's prior work before using its targets
again. Before collection destruction, stop users, complete GPU work, and retire
descriptor references. Destruction releases framebuffer, view, image, and memory
in that order, frees bookkeeping, and clears the collection. It does not wait
or release the borrowed device/render pass. Recreate the collection when its
format, extent, image count, or capacity configuration changes.

## Mask compositor setup

The compositor setup owns its offscreen render pass, sampler, descriptor-set
layout and pool, descriptor-set array, pipeline layout, and graphics pipeline.
It borrows the device, presentation render pass, optional pipeline cache, and
shader bytecode during creation. The coordinator chooses when to create it,
normally on first mask use. Offscreen image targets remain a separate owner
that borrows this setup's offscreen pass.

The offscreen pass retains a single-sample color attachment with clear/store
operations, undefined initial layout, and shader-read-only final layout. Its
subpass and external color-output dependency settings match the existing
presentation-pass setup. That matching structure does not by itself verify
every pipeline compatibility or inter-pass synchronization requirement.

The shared sampler uses linear minification/magnification, clamp-to-edge
addressing, nearest mip mode, and zero maximum LOD. Each image/mask-slot pair
has one descriptor set with two fragment-stage combined-image samplers:
binding 0 is offscreen color, and binding 1 is mask coverage. Descriptor updates
require completed prior use and already valid shader-readable views. The update
helper rejects invalid indices or null views; it performs no layout transition.

The pipeline draws a fullscreen triangle without vertex buffers into subpass
zero of the supplied presentation render pass. It uses no culling, one sample,
dynamic viewport/scissor, and the existing source-alpha color blending with
one-minus-source-alpha destination blending. Its fragment push constants are
four floats for the normalized mask rectangle followed by a 32-bit inversion
flag at byte 16. The C declaration checks that offset and the 20-byte range.
Shader bytecode must match that interface and the descriptor bindings.

Creation returns `0` on success or `-1` on invalid input or allocation/creation
failure, leaving caller output unchanged on failure. It checks slot/descriptor
count arithmetic and shader byte-count multiples of four. These checks do not
validate SPIR-V contents or prove shader/device compatibility. Shader modules
are temporary and released after pipeline creation; descriptor sets are reclaimed
with their pool. Partial failures release all owned setup resources.

Before teardown, stop use, complete GPU work, and release the separate targets
before their borrowed offscreen pass. Destruction releases the owned pipeline,
layout, descriptor resources, sampler, and offscreen pass and clears the result.
It does not wait or destroy borrowed device/cache/presentation resources.
Priority selection, mask-capacity policy, and draw recording are separate
coordinator responsibilities.

## Viewport and composition recording

Viewport fitting takes the swapchain extent and a positive aspect-ratio pair.
It centers the largest fitted rectangle using integer dimensions and 64-bit
intermediate products. Zero inputs or a fitted dimension rounded to zero fail
without changing output. Mask conversion maps canvas normalized coordinates
through that viewport into swapchain normalized coordinates, preserving
letterbox offsets. It rejects zero geometry and nonfinite coordinates/results
and does not clamp or reorder the rectangle.

Composition preparation runs on the render thread under the application-state
lock. It snapshots mask selection, views, bounds, and inversion for priorities
255 down to 0, using the existing Pixies selection hooks. A capacity from zero
to 256 limits selected mask slots; additional masked priorities are counted as
unmasked fallbacks. Invalid selected masks retain their slot but fall back
unmasked, preserving the existing slot-selection order.

The resulting plan owns CPU metadata only. Mask views, their images, and all
prepared layer resources remain borrowed and must outlive recorded GPU use.
The coordinator must retain them through submission/completion and provide the
same viewport snapshot to layer preparation. Copying metadata is not a resource
lifetime guarantee.

Recording consumes the plan outside the application-state lock, after the
image's prior GPU work and layer preparation complete. It does not query live
mask selection or bounds. For each usable mask it ensures a target, updates the
matching descriptor, clears the offscreen target to transparent black, records
Pixies then Tiles then Sprites at that priority, and records the existing
color-write-to-fragment-sample barrier.

The main pass clears with the supplied frame color and walks priorities 255
down to 0. A successfully masked priority draws one fullscreen composite
triangle with its snapshot push constants. Other priorities record Pixies,
Tiles, and Sprites directly in that order. Text records after all priorities.
Composite draws set the full-extent viewport and scissor; subsequent layer
hooks must establish their own required graphics state.

Missing, mismatched, invalid, or failed mask resources retain the existing
unmasked fallback. The returned counts identify masks recorded and mask
fallbacks, including capacity limits; callers should surface those appropriately.
The recorder returns false without issuing commands for invalid frame/plan
geometry. A true result reports issued recording only. It ends its render
passes but leaves command-buffer ending, submission, presentation, and recovery
to the coordinator. Compatibility, barrier correctness, and prepared-data-only
behavior of every layer hook still require full integration validation.

## Mask composite shaders

`mask_composite.vert` generates one oversized triangle from `gl_VertexIndex`.
Its location-zero texture coordinates cover the framebuffer without a vertex
buffer. `mask_composite.frag` samples one priority's offscreen color at binding
0 and mask coverage from the red channel at binding 1, both in descriptor set 0.

The fragment push constants are a normalized rectangle followed by a 32-bit
inversion flag at byte 16, matching `gdmf_vulkan_mask_push_constants`. The
rectangle uses swapchain coordinates from -1 to 1; the CPU converts canvas
bounds through the drawing viewport before recording the push constants.

Inside the rectangle, the fragment shader maps that position to mask texture
coordinates. Outside it, mask value is zero. Normal polarity multiplies color
alpha by one minus mask value; inverted polarity multiplies it by mask value.
RGB is unchanged. The pipeline applies the existing source-alpha blend settings.
This preserves the current treatment of translucent content; it is not a
guarantee of ideal compositing for every alpha combination. Zero-sized mask
rectangles are not guarded in the shader and must be reviewed with mask inputs.

Shader names are descriptive lowercase words; bindings, locations, push offsets,
and operations remain the same. Compile with `glslc --target-env=vulkan1.0`,
using repository-relative source paths to avoid embedding machine paths in
SPIR-V debug records. The matching `.spv` and generated header files carry
vertex and fragment stages in their filenames.

## Embedded shader headers

The host tool `shaders/spv2h.c` converts little-endian SPIR-V into guarded
`static const uint32_t` arrays with a byte count derived from `sizeof`.
The type supplies word alignment for Vulkan shader-module inputs and the
static linkage permits inclusion from separate C files. Include each shader
only where needed to avoid unnecessary private copies.

Usage is `spv2h input.spv output.h [symbol]`. Without an explicit symbol,
the tool prefixes the input basename with `gdmf_` and replaces nonidentifier
characters with underscores. An explicit symbol must be a valid identifier
of at most 255 characters. For the mask shaders use:

```text
spv2h mask_composite_vert.spv mask_composite_vert.h gdmf_mask_composite_vertex_spirv
spv2h mask_composite_frag.spv mask_composite_frag.h gdmf_mask_composite_fragment_spirv
```

Each byte-count symbol appends `_byte_count`. Pass the word array directly to
shader setup; no cast from a byte array is needed. The generator checks input
length/magic and file operations and returns nonzero on failure. It is not a
SPIR-V validator: validate compiled modules with `spirv-val --target-env vulkan1.0`.
Writing replaces the output header directly and can leave a partial header
after a write failure. Build rules must treat a nonzero exit as failure.

### Regenerating a shader

Ordinary Make builds use the checked-in headers. Editing GLSL does not regenerate
them. For shader work, install `glslc` and `spirv-val` from a Vulkan SDK and a host
C compiler. Create `fuselage/build/shaders`, then run these commands from the
repository root (use `.exe` for the host tool on Windows):

```sh
clang -std=c11 fuselage/source/GDMF/shaders/spv2h.c -o fuselage/build/shaders/spv2h
glslc --target-env=vulkan1.0 fuselage/source/GDMF/shaders/sprite.vert -o fuselage/build/shaders/sprite_vert.spv
spirv-val --target-env vulkan1.0 fuselage/build/shaders/sprite_vert.spv
fuselage/build/shaders/spv2h fuselage/build/shaders/sprite_vert.spv fuselage/build/shaders/sprite_vert.h gdmf_sprite_vertex_spirv
```

Substitute the shader's source, output names and existing array symbol. The array
symbol is declared in its current header; retain it so renderer consumers still
compile. Vertex, fragment and compute sources use `.vert`, `.frag` and `.comp`;
intermediate/header basenames use `_vert`, `_frag` and `_comp`. Keep the Vulkan
1.0 target unless the renderer's capability contract is deliberately changed.

Inspect and validate generated output in the build directory first. Only after
successful generation, replace the matching `.spv` and `.h` under
`fuselage/source/GDMF/shaders` together, keeping the edited GLSL alongside them.
Rebuild the affected renderer and test the shader on a suitable device. SPIR-V
validation and successful compilation do not prove visual or GPU correctness.
Compiler versions can change generated bytes; record the tools used when
reviewing such differences. Regenerate embedded headers with explicit symbols
even when compiling the GLSL produces unchanged SPIR-V.

## Renderer coordination

`gdmf_vulkan.c` owns the renderer context and lifecycle. `gdmf_vulkan_frame.c`
implements acquire, prepare, submission, capture, and presentation using that
context. Their private header is shared only between those coordinator files;
layers borrow handles or explicit service contexts through the internal API.
The resource modules keep their own allocation and teardown responsibilities.

Initialization establishes queue, upload, retirement, and capture locks, then
creates the instance, surface, selected/logical device, command pools, and
swapchain-dependent resources. Memory properties come from the retained
physical-device selection. Staging, commands, palettes, and framebuffers all
use the actual swapchain image count. Mask setup/targets remain lazy and use
the checked-in mask shader word arrays. Existing 16-slot mask capacity is
retained through the tile-layer constant.

Acquisition rotates the spare semaphore and checks the prior image fence.
It leaves that fence signaled until submission is ready, avoiding an unsignaled
fence after early preparation failure. Preparation runs under the shared
application-state lock, resets/begins frame commands, prepares staging,
palettes, compute and layers, and snapshots masks, viewport, clear color, and
canvas size. Repeated preparation of an already pending frame does not record
another one. Retirement advances only after observing a previously submitted
image's fence, not its initial signaled state. The countdown policy still
requires verification against complete resource lifetimes.

Submission records composition, checks command-buffer ending and fence reset,
then submits under the shared queue lock. Capture, when requested, runs after
submission and before presentation with that lock released. Presentation takes
the queue lock separately. The internal submit result is true only for
`VK_SUCCESS` or `VK_SUBOPTIMAL_KHR` presentation results. `GDMF_GetCurrentFPS`
counts those accepted requests over its sampling interval, excluding empty
and failed submission calls. It does not measure which images reached a display.

Resize/VSync recreation remains in preparation under the application-state
lock. Sprite, Tile and Text resources are released after quiescence and before
replacing borrowed palettes or the render pass. Their atlases survive ordinary
swapchain recreation, and preparation rebuilds image-dependent resources against
the new renderer objects. Text captures its viewport and descriptor during
preparation and consumes that draw snapshot after the other layers.

Zero window dimensions leave recreation pending. Quiescing serializes
against one-time uploads, drains an acquired-but-unconsumed semaphore through
an empty queue submission, waits for idle, and resets the render pool to discard
unsubmitted frame references. It releases resolved retained upload/readback
handles before retirement flush. Producers must be excluded during this whole
operation and subsequent resource replacement; locks alone do not stop every
possible producer. Successful rebuilding invalidates the remaining layer/compute resources
and updates retirement delay for the new image count. Failed rebuilding leaves
frame resources unavailable and the recreation request pending for retry.

Shutdown requires stopped frame/resource producers. GDMF quiesces before
renderer teardown; the renderer checks Text, Pixie, Tile and Sprite cleanup
and retirement draining before releasing
frame resources, pools, render pass, device, surface, instance, and locks.
Initialization failures use the same owned-resource cleanup. Clear-color and
VSync configuration survive completed teardown; device selection storage does
not. Public capture calls require the initialized lifetime and no concurrent
lifecycle teardown.

Failed idle/reset/drain operations stop normal rendering and retain resources.
An unresolved frame submission also blocks normal teardown because its queue
and semaphore state has not been recovered. Failed capture completion keeps its
request processing and its readback/source resources alive. GDMF does not destroy
the window after failed renderer cleanup; it logs the condition and `Tick`
reports the stopped renderer. This is retention for explicit recovery, not a
complete device-loss recovery implementation. Normal teardown must not be
forced merely because a previous cleanup call failed.

## Linux surface dispatch

Window creation selects the Linux backend before Vulkan initialization asks
which surface extension it needs. The surface dispatcher checks that selection
and forwards extension lookup and surface creation to the matching Wayland or
X11 adapter. The connection and native window must remain valid through surface
creation and until the Vulkan surface has been destroyed.

With no recognized backend, surface creation reports an error and returns `-1`.
Extension lookup currently returns `VK_KHR_surface_missing_backend`, a fabricated
name used to make later initialization fail. It is a failure sentinel, not an
extension callers should attempt to support. Replacing that mechanism with an
explicit initialization error remains outstanding work.

## Frame staging

Each swapchain image has its own staging slot. Preparation reuses that slot
after waiting for its previous GPU work, then records transfers into the
current frame's command buffer. Application updates mark CPU data dirty;
render-thread preparation stages those changes.

The caller-owned collection stores its borrowed logical device, a copy of
physical-device memory properties, and per-image entries grouping buffer,
memory, mapping, capacity, used bytes, and requested demand. Creation allocates
only these entries and returns `0` on success or `-1` on failure, including zero
images. GPU storage is allocated lazily and starts at a minimum of 64 KiB.
It uses host-visible, coherent memory and transfer-source/storage-buffer usage.

`gdmf_frame_staging_alloc` takes that collection and returns mapped memory and the corresponding buffer
and offset. A first request may allocate the slot immediately. If an existing
slot lacks capacity, demand is recorded for growth before a later use. Callers
retain dirty work and retry after a failed allocation. Growth or allocation
can fail, so the interface promises neither a successful retry on the next
frame nor a universally stall-free path. Suballocations use 16-byte alignment.
Requests that do not fit add to a conservative demand total without invalidating
storage already handed out. Arithmetic overflow is rejected, and output
arguments change only for a successful allocation.

Call reset after completing that image's prior GPU work and before its next
frame allocations. Reset attempts growth from recorded demand, then clears
the used/demand counters. Replacement storage is created and mapped before
old storage is released. If replacement fails, the previous buffer remains
available for smaller requests; pending uploads must still retry. This can
temporarily require memory for both allocations. Ordinary allocations never
grow an existing buffer during frame recording.

Destroy staging only after stopping users and completing GPU work. Destruction
unmaps storage, releases buffers and memory, frees entries, and clears the
collection. It does not wait or release the borrowed device. The coordinator
must keep staging image counts and lifetime aligned with the frame resources.

Successful returned storage and offsets must remain valid until their recorded
transfers complete. Sizes, alignment arithmetic, object lifetime, and recording
order need verification when changing this allocator.

## Deferred destruction

Layer operations can queue buffers, images, descriptor resources, and samplers
for retirement. The caller-owned queue borrows its logical device and an
initialized mutex. Initialize it before producers start with a nonzero image
count whose increment is representable. Initialization returns `0` on success
or `-1` on invalid image count and leaves output unchanged on failure.

Each accepted entry starts with an image-count-plus-one countdown. The
coordinator advances it at the completed-frame boundary. Reaching zero attempts
destruction, but the countdown itself is not a verified GPU completion token.
Its adequacy depends on the actual acquisition, recording, submission, and
resource-reference lifetimes. Do not introduce new references after retirement.

Enqueue functions return `true` when ownership is transferred, or `false` on
queue-node allocation failure. Null resources are successful no-ops. On
failure the caller retains ownership and must preserve resources until it can
retire them safely. There is no immediate-destruction fallback. Do not transfer
the same resource twice or assume that ignoring the return value is safe.

Image views are released before images, and buffers/images before their memory.
A failed descriptor-set free remains queued for retry. Retiring a whole
descriptor pool removes queued individual-set frees for that pool; subsequent
set-retirement requests while that pool is pending are absorbed by its eventual
destruction. Individual sets in other pools remain independent. Before directly
destroying a pool, remove its queued individual-set frees through the forget
helper. That helper does not cancel a queued whole-pool destruction, so the
caller must still respect ownership.

Before flushing, stop producers and establish GPU completion with no remaining
recorded but unsubmitted references. An idle wait alone does not cover those
recorded references. Flush ignores countdowns and returns `true` only when the
queue drains; failed descriptor-set frees remain queued and return `false`.
Neither tick nor flush waits for GPU work. Keep the device and mutex valid
until the queue is drained. Drain before changing the image count and creating
a replacement queue. This interface does not establish a general retirement
safety guarantee.

## Error handling

`GDMF_VULKAN_CHECK` evaluates a result once, logs a non-success value, and jumps
to the caller's `fail` label. The caller owns cleanup and return semantics.
`GDMF_VULKAN_LOG_IF_FAILED` only logs and continues. It does not make continuing
GPU work safe or guarantee that another operation will detect the failure.

Memory-type lookup returns `UINT32_MAX` if no type matches. Host-visible
allocation prefers device-local coherent memory and falls back to any matching
host-visible coherent type. The final allocation result is returned to the
caller; allocation failure remains a normal error path to handle.

The one-time command helper distinguishes completed work (`0`), pre-submission
failure (`-1`), and unresolved work (`-2`). The coordinator checks its own
command-buffer and fence results before submitting. Layer preparation still
needs its own error/availability contract. Prepared work does not guarantee GPU
execution, and an accepted presentation request does not prove screen display.
