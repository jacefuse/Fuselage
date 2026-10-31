# ROAP format contracts

A ROAP carries a flat little-endian RV32 program image, optionally preceded by a
project manifest and followed by an asset payload. These file contracts do not
require the VIVIAN interpreter, launcher or any Fuselage subsystem headers.
See CONVENTIONS.md, Terminology for the shared ROAP naming rule.

The format headers are `source/ROAP/roap_manifest.h`,
`source/ROAP/roap_asset_rom.h` and `source/MISL/bootspur/misl_rom.h`, under fuselage.
The first two describe file records; the third describes the separate BOOTSPUR
system ROM. The standalone packer is documented in [ROAPPACK.md](ROAPPACK.md),
and the integrated host in [MISL_HOST.md](MISL_HOST.md). File formats, numeric
identifiers and record layouts retain their existing compatibility versions.

## Manifest

The optional `RoapManifest` occupies 128 bytes at the start of the program image.
Its magic is `MISL` and its format version remains 1. `codeOffset` is a 16-bit
byte offset to the program entry. The other fields hold project settings and
resource requests, not host pointers.

| Fields | Meaning |
| --- | --- |
| canvas/window dimensions, aspect ratio | Requested presentation settings |
| simRateMilli, targetFPSMilli | Rates in thousandths of a hertz |
| audioPoolMB, programPoolMB, poolMB | Memory requests in MiB (1,048,576 bytes), retaining their existing field spellings |
| flags | Hide-cursor and banner bits |
| title | 32-byte title, expected to contain a NUL terminator |
| romMinVersion | Minimum BOOTSPUR contract version; zero declares no minimum |
| sramId, sramVersion, sramBytes | 16-byte save identity, save-layout version and requested bytes |
| poolCount | Number of extra pools, up to eight |
| poolReserved, reserved | Reserved storage; write zero |

Zero-valued optional settings request defaults where the host supports them.
A zero extra-pool count requests no extra pools; a zero SRAM byte count requests
no save region. Each counted extra pool must have a nonzero size. Host allocation
limits and launcher policy are separate from the record definition.

The four inline readers use byte access, including for unaligned input. The caller
must provide a real readable buffer covering the supplied size. They are limited
field readers, not a complete file validator:

| Function | Return and output behavior |
| --- | --- |
| `ROAP_GetManifestCodeOffset` | Entry offset if magic/version match and the offset is at least 128 and inside the supplied image; otherwise zero |
| `ROAP_GetManifestRomMinimumVersion` | Minimum ROM version for a recognized version-1 manifest; otherwise zero |
| `ROAP_GetManifestSram` | Requested save bytes; writes the identity into a valid 17-byte output buffer and the save version through a valid pointer |
| `ROAP_GetManifestMemory` | Program-pool MiB; fills a valid count pointer and eight-element uint16_t output array with extra-pool requests |

Zero is deliberately ambiguous in these legacy-style readers: it can mean no
manifest, an unsupported/invalid manifest, or no request for that field. For entry
selection, zero also serves the legacy image whose code starts at zero. The entry
reader does not check instruction alignment or prove that executable code exists.

The SRAM reader clears id[0] and the output version before an early return; it
adds a terminator after copying a recognized 16-byte identity. It does not validate
save-path characters or save-layout compatibility. The memory reader returns a
zero count for an excessive count or a zero-sized declared pool, but may retain
earlier sizes in the array. Always ignore entries beyond the returned count.

The minimum-ROM reader now checks the manifest version, matching the other
readers. Previously it read that field from any full-size image with matching
magic, including unsupported versions. Serialized bytes are unchanged.

## Asset ROM

An optional read-only asset payload follows the program image. Its trailer is
the final 16 bytes of the file and is included in the payload's byte count.
An asset-only ROM file can contain the payload without a preceding program image.

| Record | Bytes | Purpose |
| --- | --- | --- |
| `RoapAssetTrailer` | 16 | `ROMT` magic, version, directory offset and total payload bytes |
| `RoapAssetDirectoryHeader` | 12 | `ROM1` magic, directory version and entry count |
| `RoapAssetEntry` | 20 | Application ID, payload-relative offset, stored size, decoded size and codec flag |

Directory version remains 1. The writer uses that version for the trailer too.
For a file of N bytes and trailer totalBytes T, the payload begins at N minus T;
the program image precedes it. Directory and section offsets are relative to
that payload start, including when attached to a ROAP.

Raw data has flag zero and equal stored/decoded sizes. Codec values 1 and 2 are
already allocated and must not be reassigned. Other codecs require an appropriate
guest decoder; defining a directory entry does not supply decompression.

The header defines records and constants, not a validated parser or memory map.
Consumers must validate versions, arithmetic, directory/entry extents and policy
before using untrusted offsets. The structs describe little-endian bytes; copying
a host struct is not a portable serialization method on a big-endian host.
Runtime memory ceilings do not belong in this format header.

## BOOTSPUR system ROM

BOOTSPUR is separate from application asset data. Its 16-byte `MislRomHeader`
contains `BOOT` magic, contract version, entry count and jump-table offset.
The current table has 47 entries and keeps its existing version 2026091001.
This is a compatibility version, not a source-file modification date.

The jump table stores offsets from the ROM base. Entry indices are stable and
must not be reused or reordered; services use ecall base 0x5000 plus the index.
The header remains usable by C and preprocessed assembly. Its region identifier
mirrors the MMIO contract and must be checked against that contract by consumers.
See [ROM services](MISL_ROM.md), [guest runtime](MISL_RUNTIME.md) and
[BOOTSPUR](BOOTSPUR.md) for the implementations and build contract.
