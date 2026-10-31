# Pixie image formats

`PIXIE_OP_UNPACK` interprets its flags according to pixie mode: texture mode
uses `PixieUnpackFormat`, mask mode uses `PixieMaskFormat`, and live mode refuses
unpacking. These are separate numeric namespaces. The format is supplied by the
command; it is not a field in the color image header.

## Color image formats

| ID | Format | Palette |
| --- | --- | --- |
| 0 | `RGBA8`: four raw bytes per pixel | None |
| 1 | `PALETTE4BPP`: two indices per byte, high nibble first | One shared sixteen-entry Colors palette |
| 2 | `RLE_PALETTE_SHARED`: compressed indices | One shared sixteen-entry Colors palette |
| 3 | `RLE_PALETTE_OWN`: compressed indices | 256 embedded RGBA8 entries |
| 4 | `RLE_RGBA8`: four separately compressed channel planes | None |
| 5 | `PALETTE8BPP`: one raw index per pixel | One 256-entry deep Colors palette |

The command's palette argument selects a shared palette for formats 1 and 2,
or a deep palette, 0..15, for format 5. It is unused by other formats. Deep
palettes are separate from the shared sixteen-entry palettes. Indexed colors
are resolved during decoding; changing a palette later does not by itself
redecode an existing pixie's pixels.

Four-bit indices form a continuous stream across rows. An odd-width row can
leave the next row starting in the low nibble of the same byte. RGBA byte order
is red, green, blue, alpha. Pixels outside the destination buffer are clipped.

## Color headers and payloads

Color data at the RAM offset begins with:

| Field | Size |
| --- | --- |
| Width | uint16 |
| Height | uint16 |
| Pass count | uint8 |
| Pass IDs, in application order | One uint8 per pass |

Multibyte packed fields use little-endian layout. The current decoder requires
positive dimensions no greater than 4096 and at most 16777216 pixels. The full
encoded data must fit in pixie RAM; those limits do not guarantee that any
particular image fits.

An owned palette follows the pass list for format 3: 256 RGBA8 entries, including
entries not used by that image. A row table, when declared, follows that palette
or directly follows the pass list for format 2. The payload follows these fields.

Raw formats require zero passes. Their header is followed by a uint32 length
field and raw bytes. The current raw paths skip that length field and derive
the required bytes from width, height, and format; they do not check that the
stored length agrees. Store the actual length when constructing an image.

Without Huffman, compressed payloads have a uint32 encoded-byte count followed
by those bytes. With Huffman, the payload wrapper contains 256 uint8 code
lengths, a uint32 decoded length, a uint32 encoded length, and encoded bytes.
The decoder first removes Huffman and then decodes the primary compression.
True-color format 4 contains four uint32 plane lengths followed by the four
channel streams in RGBA order. Delta, when present, is undone per decoded plane.

## Compression passes

| ID | Pass | Meaning |
| --- | --- | --- |
| 0 | `RLE` | Count/value byte pairs |
| 1 | `HUFFMAN` | Canonical Huffman coding around the primary encoded bytes |
| 2 | `DELTA` | Per-channel byte prediction before primary compression |
| 3 | `LZ` | Sliding-window compression, used instead of RLE |
| 4 | `ROWTABLE` | Row entry points for independently decodable indexed RLE rows |
| 5 | `CTXTILE` | Separate mask tile encoding, not a color pass combination |

For compressed indexed formats, accepted pass lists are RLE or LZ, optionally
followed by Huffman, or exactly `[ROWTABLE, RLE]`. Row tables are supported only
by formats 2 and 3. They contain `height` uint32 byte offsets into the RLE
payload, and runs do not cross row boundaries. LZ, Huffman, and Delta cannot
be combined with this table. Row tables are indices, not a byte transformation.

True-color format 4 accepts RLE or LZ, optionally followed by Huffman, with
optional Delta before the primary pass. Delta is refused for indexed formats.
The implementation's pass parser also accepts zero passes for compressed-format
IDs and then takes its default RLE decoding path; use an explicit primary pass
rather than relying on that permissive case.

Raw images and indexed RLE with row tables can use the compute path when the
renderer has prepared it. Other paths decode on the host. Selecting GPU decode
makes a texture GPU-resident; host drawing and subsequent host-only decodes are
then refused until reinitialization. This behavior depends on renderer readiness
and resources; it is not a claim that every Vulkan device can run the kernels.

## Mask formats

Mask format 0, `RAW8`, copies exactly output-width times output-height coverage
bytes from the command's RAM offset. It has no image header or palette. It
replaces the whole host mask and ignores the command's destination word.

Mask format 1, `CTXTILE`, stores changed tiles using a static probability table
and previous-frame context. Coverage levels 0..3 become bytes 0, 85, 170, 255.
Unlisted tiles retain coverage at four-level precision when any tiles are
processed. An empty list leaves the original bytes unchanged. Its header is:

| Offset | Field |
| --- | --- |
| 0 | uint16 width |
| 2 | uint16 height |
| 4 | uint8 pass count, 1 |
| 5 | uint8 pass ID, `PIXIE_PASS_CTXTILE` |
| 6 | uint8 context bits |
| 7 | uint8 square tile size |
| 8 | uint16 listed tile count, which can be zero |
| 10 | uint32 probability table RAM offset |
| 14 | uint32 total coded payload bytes |
| 18 | Tile entries, then concatenated streams |

Each tile entry is a uint16 grid index and uint16 stream byte length. Width and
height must match the pixie's output dimensions. Tile size is 1..128. The
probability table must be four-byte aligned and fit entirely in RAM. It has
`1 << context_bits` pairs of uint32: first `upper | (lower_zero << 16)`, then
`lower_one`. Each is a twelve-bit zero-bit probability in the range 1..4095.

The header limit is twenty context bits, but a twenty-bit dense table requires
8 MiB and cannot fit in the current 4 MiB pixie RAM. Actual usable widths must
also satisfy the table and other data's storage requirements. The header limit
does not promise that every allowed context width can be stored.

The host decoder bounds the overall payload. A probability component of 4096
or greater in a visited context entry returns false without changing the mask.
Zero probability components retain the existing fallback of 2048. Individual
tile stream lengths, duplicate tiles, truncated streams and inconsistent lists
are not all strictly validated. Some malformed data can produce partial or
altered output while returning true. Treat successful unpacking as an operation
result rather than proof that arbitrary encoded input was valid.
The compute shader has additional documented input limits in GDMF_COMPUTE.md;
its presence does not mean this public mask command currently dispatches it.

## Decoder behavior and limits

Packed integer fields are read explicitly as little-endian bytes, including
unaligned fields. Pixel and allocation limits are checked before host decoding.
Shared palettes use the Colors getters; embedded palettes supply 256 RGBA entries.
The eight-bit raw format uses the separate deep palette set.

RLE clips runs to the required output size. LZ supports overlapping backward
matches and stops at invalid distances or incomplete match fields. A color
decode that produces fewer bytes than required returns false before blitting.
Extra encoded data and oversized runs are not generally rejected once enough
output has been produced.

The Huffman reader supplies zero bits after its encoded bytes end. This preserves
existing decoding behavior, but can allow truncated input to produce a complete
output. The canonical code tables also retain the existing 32-bit code arithmetic
with lengths up to 255; accepting a length does not establish that every such
codebook is representable or valid. Strict codebook and stream validation remain
necessary before treating this as a general-purpose untrusted-input decoder.

Host mask decoding holds separate previous and current level buffers so tile
order cannot replace temporal context with newly decoded neighboring pixels.
The mask is published after processing the tile list. A malformed tile index or
payload progression can still stop that list early and publish partial results.
Unlisted coverage is quantized to the four supported levels whenever a nonempty
tile list is processed; an empty list leaves the original bytes unchanged.

The standalone Pixies tests include 24 format/pass combinations, exact expected
colors, variable-length canonical codes, overlapping LZ matches, allocation
failures, range checks, mask probability rejection and GPU-request routing.
GPU requests are controlled test boundaries; these checks verify host decoding,
not complete Vulkan backend or engine integration.

## GPU decode requests

Raw images and indexed RLE images with row tables can enqueue GPU work after
compute preparation succeeds. Each request copies its image bytes, row table
and resolved palette at command time. Later RAM or palette changes do not
alter queued work. RLE table and payload bytes are copied directly into one
queue region, without an intermediate joined allocation. Byte data is padded
to four bytes with zeroes for word-based shader reads.

The queue holds at most 64 requests and 64 MiB of copied input, including
palettes and padding. Failure to queue permits host fallback only while the
pixie remains host-owned. Once GPU-resident, a failed enqueue refuses the
operation rather than writing into a stale host buffer. Residency changes
only after a request has been copied successfully.

Frame preparation records requests in order, after any dirty host pixels have
been uploaded and target images initialized. An unavailable target, staging
allocation or compute dispatch stops the drain; that request and all later
requests remain pending. Consumed or cancelled input storage is reclaimed
without waiting for the whole queue to empty.

The compute recorder expects targets in `SHADER_READ_ONLY_OPTIMAL`, transitions
them to `GENERAL` for dispatch and restores the sampling layout afterward.

Successful pixie release cancels that ID's pending requests before its logical
state is destroyed. Failed backend release retains the state and requests for
retry. Cancellation does not undo commands already recorded into a frame;
the backend must handle those references before allowing ID reuse.

Queue consumption currently follows successful command recording, rather than
submission or GPU completion. Recovery for a frame abandoned after recording
remains unfinished. The backend connects the recording adapter to initialized
target images and the renderer's frame staging, after successful dirty uploads.
