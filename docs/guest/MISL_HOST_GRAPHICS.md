# MISL diagnostic text and graphics

The native MISL host adapters forward guest calls to GDMF. They own address
translation and argument marshalling; GDMF remains independent of MISL.
Numeric opcodes, register assignments and wire records retain their existing ABI.

## Memory and results

Inputs require a complete readable span. Outputs require writable storage;
read-only assets, ROM, save spans and Pixie banks cannot serve as outputs.
Typed records pass through aligned native storage rather than casts from guest
bytes. Byte counts are checked before narrowing or allocation. Device-window
permission limits are described in [Native MISL host boundary](MISL_HOST.md).

Scratch allocation failure refuses the operation using its existing result
convention. A zero result can therefore mean refusal or an ordinary empty result;
these interfaces do not provide a separate allocation-error code. Native geometry,
numeric-input and backend acceptance rules still apply.

## Text

Diagnostic printing copies at most 511 bytes into a terminated local buffer.
A truncated or partially unmapped string prints its safely copied prefix.
This differs from capture filenames, which require a complete valid string.
Text cursor, cell and return-value behavior belongs to the diagnostic text layer.

## Sprites

Bitmap input must cover the complete mapped sprite stride. Collision reports
retain their eight-byte wire records and maximum of 32 reports per request.
Batch input counts are limited to 1,048,576. Batch outputs require ordinary
guest RAM, with aligned virtual input addresses and disjoint input/output ranges.
Native input storage is copied when host alignment cannot be assumed.

Pair searches retain count-only queries and the native minus-one refusal result
(`UINT32_MAX` in the guest register). Output capacity limits stored pairs, not
the reported total. Input may overlap output because inputs are copied first.
Pair and type writes retain their established per-pair order when outputs alias.

## Tiles

Viewport, map-offset and cell-offset getters use native local values followed by
byte copies into guest outputs. This permits valid unaligned guest destinations.
Initialization and release refresh mapped tile banks immediately, including
removing mappings when storage is no longer available.

## Pixies

Command arguments, command lists and transform matrices use aligned native copies.
Submission clamps lists to 4096 commands. Status retains executed/rejected counts,
the first-rejection sentinel and submission counters. A scratch allocation failure
executes nothing, reports zero executed/rejected, retains the sentinel and advances
the submission counter; cumulative execution totals remain unchanged. Acceptance
does not imply GPU completion.

Initialization and release refresh Pixie banks before returning. `WritePixieRAM`
permits overlapping source and destination, including input from a mapped bank;
the native implementation uses `memmove` and retains no caller buffer.

Guest coordinate packers now clamp before conversion and guard line-helper
inputs. Their established rounding difference from native packing is documented
in [guest Pixies](MISL_GUEST.md#pixies-and-command-lists); the bridge forwards
encoded words without applying another coordinate conversion.

## Colors

Palette inputs are copied into aligned native arrays of 16 or 256 colors.
RGBA packing uses the canonical native packer. Palette setters and generation
queries retain the established native behavior; this adapter does not redefine
color names or palette entries.

## Interactions

World-position getters use native float temporaries. Sprite/tile cell queries
validate the full output capacity with wide byte-count arithmetic before calling
the native implementation. A null output remains a count-only query. The total
can exceed capacity; only the available number of cells is stored. Invalid spans,
unrepresentable capacities and scratch failures return zero without cell writes.
Collision batches use the same RAM and range restrictions as sprite batches.

## Banks and dirty uploads

The host owns bank refreshes and the VIVIAN store watch. Compile-time checks pin
mapped dimensions and native record layouts to the existing wire definitions.
The scheduler must install the watch after committing mappings and before the
first instruction, then flush dirty ranges after each slice. The combined
`FuselageRunGuest` tick now supplies those call sites; see
[execution and pumps](MISL_HOST_RUNTIME.md).

The watch marks affected bitmap slots and tile-map layers without uploading
synchronously. Output admission may conservatively mark an entire capacity.
Flush attempts native uploads and clears the dirty flags even if an upload is
refused. Retry and failure reporting remain FUNDUS work; clearing a flag does
not prove successful upload or presentation.

## Numeric conventions

Sprite rotation uses degrees; tile-map rotation and primary Pixie rotation use
radians; Pixie transform-slot rotation uses degrees. This migration preserves
those conventions. The planned author-facing degrees policy is post-publication
FUNDUS work. The adapters do not establish a universal nonfinite-input policy.
