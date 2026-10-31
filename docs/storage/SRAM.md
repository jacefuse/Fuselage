# SRAM save storage

SRAM keeps a program's declared save bytes in memory and stores them between runs.
The application owns the layout, slots and version of those bytes. Fuselage opens
the region before execution and requests a disk save at clean exit. Writing the
working copy marks it dirty; it does not immediately write a file.

## Lifecycle

Include `SRAM/sram.h`. `SRAM_Configure` declares an ID of 1..16 bytes, a version
and a byte count. Zero bytes selects no save region. Configuration, open and close
retire any existing working copy without saving it; detach borrowed mappings first.
A failed nonzero declaration cannot subsequently open as a save-less program.

`SRAM_Open` loads an existing file or creates a complete zero-filled file for a
first run. It returns true when storage is ready, or when no region was declared.
It returns false for invalid configuration, unreadable or incompatible files,
allocation failure or failed creation. `SRAM_GetFailureReason` supplies a short
diagnostic; the orchestrator decides how to present it.

`SRAM_GetSize` reports open storage bytes, otherwise zero. `SRAM_IsEnabled` means
a region is open. `SRAM_Read` and `SRAM_Write` copy complete admitted ranges;
zero length, null pointers and out-of-range requests return false without changing
bytes. Writes permit overlapping input from a host-mapped save view and mark the
working copy dirty. A successful write marks it dirty even if the bytes match.

`SRAM_Save` publishes dirty storage and clears the flag only on success. It also
returns true when there is no work: unopened, clean or abandoned storage. False
leaves the working copy dirty so the host can report or retry the failure.
`SRAM_Abandon` suppresses disk writes until the next close/open/configure cycle.
`SRAM_Close` frees storage without saving. Calls must be serialized on the host
thread; the singleton implementation does not provide concurrent access.

## Paths

By default, `example.roap` or `example.exe` uses `example.save` beside the program.
`SRAM_SetSaveDirectory` selects another existing directory; null or empty restores
the default. It returns false for an overlong directory and retains the previous
setting. Check that result. It does not create directories.

`SRAM_GetPath` returns the most recently constructed save path. `SRAM_GetProgramPath`
queries the executable's path, using a supplied nonempty `argv0` as fallback. A
launcher should supply the ROAP's actual path instead of its own executable path.
The implementation retains narrow Windows paths; it does not yet promise UTF-8
or arbitrary Unicode filenames across platforms.

## File format

FSAV format 1 is unchanged. Integers are little-endian and read/written byte-wise.

| Offset | Bytes | Field |
|---|---|---|
| 0 | 4 | `FSAV` magic |
| 4 | 4 | File format, 1 |
| 8 | 16 | Program ID, NUL-padded |
| 24 | 4 | Application save version |
| 28 | 4 | Declared payload size |
| 32 | 4 | Reflected IEEE CRC-32 of payload |
| 36 | 4 | Flags, written as zero |
| 40 | 8 | Creation UTC seconds |
| 48 | 8 | Last-save UTC seconds |
| 56 | 8 | Reserved, written as zero |
| 64 | Declared size | Payload |

Loading checks magic, format, ID, application version, declared size, complete
payload and checksum. A refused file is left unchanged. Trailing bytes and
nonzero flags/reserved fields retain their existing acceptance behavior; they are
not currently rejected. CRC detects accidental damage, not malicious modification.
UTC timestamps use the C runtime clock, with zero on clock failure. SRAM has no
DICE, graphics, input or Fuselage dependency.

## Disk writes

Only a missing main file permits first-run creation. Other read-open failures
refuse loading. If the main file is missing but `.prev` exists, loading also
refuses and reports the recovery copy; it does not silently initialize a new save.

Writes first create an exclusive staging file named with a process ID and counter
beside the destination. Header, payload, flush and close must succeed before
publication. Existing legacy `.save.tmp` files are ignored and left untouched.
For an update, the current save becomes `.prev`, then the staged file takes the
main name. If publication fails, restoration is attempted without replacing an
existing main path. A failed restoration leaves `.prev` available for recovery.
The host must report a failed save rather than assuming the primary path is valid.

This is not a power-loss durability guarantee. There is no complete file/directory
sync protocol or cross-process writer lock. Paths and their parent directory must
be host-controlled; symbolic links, reparse points and concurrent path replacement
are not yet a hardened trust boundary. Publication is tested on Windows; POSIX
uses a same-directory hard link that refuses an existing destination and still
needs native Linux/macOS verification.

## MISL integration

The host prepares guest memory, opens SRAM, then commits its borrowed read-only
save span and descriptor. BOOTSPUR provides the admitted write path. An open/save
failure does not publish a partially mapped guest. The host refuses to close save
storage while the guest still borrows it; detach CPU mappings first.

The finish helper accepts a host clean-exit decision. Faults and failed save loads
force abandonment; a normal guest finish can remain eligible for a later clean
exit. Finishing does not free mapped storage. The scheduler and failure views are
implemented; launcher/root exit call sites remain part of orchestrator integration.

## Building

Run `make -C fuselage/source/SRAM CC=clang` from the project root. Override `CC`,
`AR`, `CPPFLAGS`, `CFLAGS` or `BUILD_DIR` for the local toolchain. The archive uses
the C runtime and standard platform facilities, with no other Fuselage systems.
Internal tests and tracking notes are separate from the library and public API.
