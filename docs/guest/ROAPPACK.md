# roappack

roappack is a standalone host tool for building and inspecting ROAP asset data
and flattening linked guest executables. It needs no Fuselage library, VIVIAN,
launcher, graphics/audio SDK or running engine. The file contracts are described
in ROAP_FORMAT.md.

## Commands

```sh
roappack -o game.rom 7=sprites.bin 91=level.bin
roappack -o game_full.roap --image game.roap 7=sprites.bin
roappack --list game_full.roap
roappack --flatten game.elf -o game.roap
```

Use `--help` for command syntax. Exactly one operation is selected. Section IDs
are unsigned 32-bit values in decimal, hexadecimal or octal notation. Duplicate
IDs, incomplete numbers, missing paths and incompatible options are rejected.
Packing accepts up to 256 sections and writes raw data; it does not compress it.

`--max-bytes N` bounds each input and the complete output. Its default is the
32-bit byte-count limit, 4,294,967,295, rather than a particular engine's memory
map. A produced ROAP must still fit the target runtime's allocation and placement
policy. The option limits file/image size, not total process memory use.

Exit status is zero on success, one for a file/data/option-value/operational
error, or two for an invalid command shape or missing option argument.
Listing a file without an asset trailer succeeds and
reports that fact, including for a small legacy program image.

## Packing assets

Section data is four-byte aligned relative to the payload start. The directory
follows the data; the trailer is last and counts itself in totalBytes. When
`--image` is supplied, the original program image is prepended unchanged. An
image already ending in the asset-trailer magic is refused to prevent attaching
a second payload. This signature check does not validate that existing payload.

All inputs are read and aggregate sizes checked before output staging begins.
This also supports an output path matching an input path. Inputs are retained
in memory during preflight; large asset collections can require substantial RAM.
The writer encodes integers explicitly as little-endian bytes and preserves
existing valid version-1 asset output.

## Listing assets

Listing checks trailer/directory versions, directory size, section extents,
duplicate IDs and raw stored/decoded-size consistency before printing entries.
Sections cannot extend into the trailer or reference directory bytes. Empty
sections are allowed, as are shared data bytes referenced by distinct IDs.
Non-raw codec flags are listed as requiring a guest decoder; listing does not
decode or validate compressed contents.

## Flattening ELF

The input must be a linked little-endian RISC-V ELF32 executable with a valid
program-header table. File-backed load segments must start collectively at
address zero, fit their file/memory extents and not overlap each other. This tool
rejects a nonzero origin instead of silently rebasing addresses.

File bytes are placed at their physical load addresses, with gaps zero-filled.
Memory-only tails and pure BSS segments do not extend the output. No relocation
or executable-code validation is performed. ELF table and segment calculations
are checked before image allocation and output replacement.

## Output replacement

Output is written to an exclusively created sibling staging file, then closed
and replaced with the platform's replacement operation. An existing destination
is no longer deleted before replacement. Handled failures attempt to remove only
the staging file created by that invocation and report cleanup failures;
unrelated temporary files are retained.

The Windows build uses MoveFileEx and POSIX builds use rename. Native Windows
tests cover replacement failure with a locked destination and concurrent writers.
Concurrent replacement can be refused by the OS; the last successful replacement
wins, and a refused invocation reports an error rather than deleting the target.
Power-loss durability, hostile-directory behavior and native macOS/Linux cases
remain validation work. Windows path handling currently follows the native narrow
character APIs; a complete Unicode/long-path policy is future work.

## Building the tool

Prebuilt SDK packaging is planned. Rebuilding the tool requires a host C11
compiler and its platform headers, with no engine library. From
`fuselage/source/ROAP/tools`, either use the Makefile:

```sh
make CC=clang
```

or compile the five files directly:

```sh
cc -std=c11 -O2 -I.. roappack.c roappack_io.c roappack_pack.c \
   roappack_list.c roappack_elf.c -o roappack
```

On native Windows use `mingw32-make`; the executable defaults to
`fuselage/build/tools/roappack.exe`. Other hosts use `roappack` without the suffix.
The only project header outside the tool directory is `roap_asset_rom.h`.
RISC-V tools are needed to build guest code, not to compile this host tool.

The Makefile supports `CC`, `CPPFLAGS`, `CFLAGS`, `LDFLAGS`, `LDLIBS`, `BUILD_DIR`
and `FORMAT_INCLUDE_DIR`. Checkouts may contain spaces; a custom BUILD_DIR must
be nonempty and have no whitespace. Use a relative output path from a spaced
checkout. Clean or change output directories when changing build options.
`clean` removes only the listed tool outputs and retains unrelated files.

## Source organization

| File | Responsibility |
| --- | --- |
| roappack.c | Command parsing and dispatch |
| roappack_io.c | Bounded input, exclusive staging and replacement |
| roappack_pack.c | Raw payload layout and little-endian writing |
| roappack_list.c | Directory validation and listing |
| roappack_elf.c | Checked ELF segment extraction |
| roappack_internal.h | Private shared helpers and declarations |

Native Windows strict builds, ordinary/sanitized file tests and an isolated
SDK-like source build pass. Native macOS/Linux builds and runtime validation are
still pending. The header-only format checks and tool tests are internal material
and are not prerequisites for using a prebuilt tool.
