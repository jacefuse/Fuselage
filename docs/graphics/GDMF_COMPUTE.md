# GDMF compute

## Kernel table

Compute is an internal service for engine-owned image decoders. Applications do
not supply shaders through this interface. The shared header describes kernels,
resource bindings, and parameter layouts without including a graphics API.
Vulkan is the current implementation.

| Kernel | Work per invocation | Workgroup size |
| --- | --- | --- |
| `DECODE_RAW` | One packed-image pixel | 16 by 16 |
| `DECODE_RLE` | One indexed run-length row | 64 by 1 |
| `DECODE_MASKTILE` | One dirty mask tile | 64 by 1 |

`gdmf_kernel_desc` returns an immutable entry or NULL for an unknown ID.
`gdmf_kernel_groups` rounds invocation counts up to whole workgroups without
overflowing the input width or height. It requires a valid description with
nonzero group dimensions and valid output pointers. For row and tile kernels,
pass the row or dirty-tile count as width and 1 as height. Zero in either
dimension produces no dispatch work. Workgroup counts still need to fit the
device's limits.

Push structures contain consecutive 32-bit values copied directly into the
shader's push block. Field order and signedness are part of this internal binary
contract, even when C names change. Destination coordinates may be negative;
the kernels clip against the target dimensions.

Raw formats are RGBA8 (0), palette 4 bpp (1), and palette 8 bpp (5). Raw source
offsets are bytes. Palette bases count uint32 entries packed by `PackColorsRGBA8`.
The RLE row table contains one uint32 offset per row, relative to `data_offset`;
the table starts at a four-byte-aligned byte offset. The RLE decoder uses
`data_length` as its payload end, subject to the input limits below.

The mask tile table contains pairs of uint16 values: tile index and stream byte
length. Stream starts are computed from preceding lengths. `tile_count` counts
dirty tiles, `tiles_x` describes the frame grid, and `tile_size` is at most 128.
`probability_base` counts uint32 entries; `context_mask` is
`(1 << context_bits) - 1`. The destination must already retain unchanged tiles.
These parameter descriptions do not establish validation of arbitrary encoded
input; resource and stream bounds remain the caller's responsibility.

## Dispatch contract

Buffer handles, image handles, and views are borrowed opaque uint64 values.
Their native interpretation belongs to the backend. Buffer offsets and sizes
are bytes; offsets must satisfy `gdmf_compute_buffer_alignment`. Before device
initialization that function returns a conservative 256, not a queried limit.

| Binding kind | 0 | 1 | 2 | 3 |
| --- | --- | --- | --- | --- |
| Bytes/palette/image | Input storage buffer | Packed palette buffer | Written image | Unused |
| Bytes/probabilities/previous/image | Input storage buffer | Probability buffer | Previous image | Written image |

The dispatch field `palette` supplies binding 1 for either kind. Even a raw
RGBA8 decode needs a valid descriptor there although that kernel path does not
read a palette. Mask decoding requires distinct previous and destination images
of matching dimensions. Reading a separate previous image keeps neighboring
tile results independent of dispatch execution order.

The renderer calls prepare before layer preparation, after waiting for that
swapchain image's prior submission. Successful preparation resets its descriptor
pool. `gdmf_compute_ready` reports success for the latest frame preparation;
a failed reset leaves compute unavailable for that frame. Initialization failure
is remembered until compute shutdown, including across swapchain recreation.
Callers decide whether to retain work for retry or use a host implementation.

Vulkan records into the frame command buffer outside render passes, on the
graphics queue. Both image roles enter and leave in
`VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`; barriers move them to GENERAL for
compute and restore them afterward. Resources must remain alive through GPU
completion. Buffer contents must already be available to the compute stage.
This service does not submit commands, transfer queue ownership, or validate
every possible alias, buffer bound, format, parameter value, or device limit.

`gdmf_compute_record` returns false for unavailable preparation, invalid checked
arguments, or descriptor allocation failure. It returns true after recording,
or for zero workgroups without recording. True does not report GPU completion
or decoded-image correctness. Each frame has room for up to 64 descriptor sets,
with two storage-buffer and two storage-image descriptors budgeted per set.

## Vulkan resources

`gdmf_compute_vulkan.c` owns layouts, pipelines, and per-image descriptor pools
in one private context. `gdmf_compute_vulkan_record.c` borrows that context to
record dispatches. The private header joins those two implementation files;
layer code uses the existing shared internal interfaces.

Initialization creates the complete kernel table or releases its partial
resources. Shader modules are temporary. Generated read-only uint32 arrays
provide aligned word storage for shader-module creation. Pool-reset errors
are reported and prevent dispatch recording for that frame.

Shutdown and swapchain recreation must exclude producers, complete submitted
work, and discard recorded but unsubmitted references before destroying compute
resources. The renderer supplies that boundary. Compute teardown does not issue
an unchecked device-idle wait of its own. The service is serialized by renderer
lifecycle and frame preparation; it is not a general concurrent compute API.

## Compute shaders

The three GLSL sources live beside their compiled SPIR-V and generated headers
in fuselage/source/GDMF/shaders. Workgroup sizes, resource bindings, and push
member order match the shared compute interface.

Raw decoding uses one invocation per pixel. Four-bit palette indices form one
continuous stream across rows, high nibble first; odd-width rows can start in
the low nibble. Eight-bit indices select a palette entry directly. The remaining
format branch reads four RGBA bytes, so callers must reject unsupported formats
rather than expecting an error from the shader.

RLE decoding walks one row's count/index pairs in each invocation. It stops on
an incomplete pair or a zero count, leaving any pixels already stored in place.
It clamps each run to the row width. Clipped destination pixels are skipped.

Mask decoding runs one invocation per dirty tile. Each tile has its own range
stream and uses a static probability table. Context combines five previous-frame
samples with four causal samples from the current tile; causal samples outside
the tile fall back to the previous frame. Two private rows retain current-tile
history. Coverage levels 0, 1, 2, and 3 become stored values 0, 85, 170, and 255.
Each context's three probabilities occupy two uint32 entries: the first holds
the upper-bit probability and the low-bit probability for upper bit zero in its
low/high halves; the second holds the low-bit probability for upper bit one.

## Compute shader input limits

These kernels require caller-validated input. Raw decoding has no source-length
parameter. RLE uses a payload-end check, but offset additions and row-table
access are not protected against overflow or out-of-range tables. Byte helpers
load whole uint32 words, so buffer storage must cover the final word even when
the logical byte stream ends partway through it. Palette and probability indices
must also lie within the supplied buffers.

The mask shader retains `data_length` in its push block but does not check it
when reading the tile table or range streams. Tile lengths locate subsequent
streams; they do not bound the current decoder. Probability values, stream
lengths, dimensions, and offsets must be valid before dispatch. The shader does
not return a decode error or undo partial stores.

Mask tiles must be nonzero and no wider than 128. The current row-history
lookup does not reproduce clamped causal context at a partial right-edge tile;
widths that are not multiples of tile size need correction before general use.
Previous-frame sampling also uses untranslated image coordinates before applying
`destination_x/y` to stores. Translated mask decoding needs separate validation.
These limits apply to the retained implementation, including its compiled assets.

## Regenerating compute shaders

Build `spv2h` from the reviewed source as described in GDMF_VULKAN.md, then run
these commands from the repository root with the shader tools on PATH:

```sh
glslc --target-env=vulkan1.0 -fshader-stage=compute fuselage/source/GDMF/shaders/decode_raw.comp -o fuselage/source/GDMF/shaders/decode_raw_comp.spv
spirv-val --target-env vulkan1.0 fuselage/source/GDMF/shaders/decode_raw_comp.spv
spv2h fuselage/source/GDMF/shaders/decode_raw_comp.spv fuselage/source/GDMF/shaders/decode_raw_comp.h gdmf_decode_raw_spirv
```

Repeat for `decode_rle` and `decode_masktile`, with symbols
`gdmf_decode_rle_spirv` and `gdmf_decode_masktile_spirv`. Generated headers contain
static const uint32 arrays and `_byte_count` constants. Keep source, binary,
and header together; build dependencies must regenerate the header after its
binary changes. SPIR-V validation checks the module, not device feature
selection, input safety, decoding correctness, or runtime synchronization.
