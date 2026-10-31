# BOOTSPUR system-ROM image

`misl_rom.S` defines BOOTSPUR's header, entry table and register adapters.
`misl_rom_bin.h` contains the linked image as a byte array for native hosts.
The image reaches native host services through ecalls; it does not contain
standalone implementations of the memory, formatting or math routines.

## Entry table and stub shapes

The header contains four little-endian 32-bit words: magic, compatibility
version, entry count and table offset. The table starts at byte 16. Its 47
entries contain byte offsets relative to the ROM base. See
[ROAP format](ROAP_FORMAT.md#bootspur-system-rom) for the shared constants.

Callers obtain the ROM base through the region descriptor and load the requested
offset from that image's table. Entry indices are stable; the stored byte offsets
can change when the image changes. Do not hard-code a stub's byte address.
`misl_rom_start` labels the ROM header, not a directly callable routine.

One assembly list emits both the offset table and the adapters. Assembly checks
enforce the declared header/table position, contiguous entry indices, entry count
and supported adapter kinds.

| Adapter kind | Register conversion |
|---|---|
| Integer registers | Arguments and results already use integer registers |
| Float unary/binary | Move `fa0`/`fa1` bits into `a0`/`a1`; return `a0` bits in `fa0` |
| Double unary | Move `fa0` through stack storage into `a0:a1`; return the pair in `fa0` |
| Double binary | Move `fa0`/`fa1` into `a0:a1`/`a2:a3`; return `a0:a1` in `fa0` |
| Integer to double | Leave integer arguments in place; return `a0:a1` in `fa0` |
| Double to integer | Move `fa0` to `a0:a1`; leave integer results in place |
| Integer to float | Leave integer arguments in place; return `a0` bits in `fa0` |
| Float to integer | Move `fa0` bits to `a0`; leave integer results in place |

Pairs are low word first. These adapters move representations rather than
performing the numerical conversion. Each issues `MISL_ROM_ECALL_BASE + index`
and returns to the guest's caller. The current one-double spill adapters reserve
eight stack bytes; the two-double adapter reserves sixteen. They restore the
stack pointer before returning and require writable guest stack storage.
Argument registers are caller-saved; in particular, binary-double adapters use
`a2:a3` for the second argument.

The guest runtime's thunks load the ROM base from the shared cache and tail-jump
through this table. Discovery must have completed before those calls. See
[Guest runtime](MISL_RUNTIME.md#rom-thunks) for symbol binding and floating ABI
requirements.

## Embedded image

The checked-in `misl_rom_bin.h` supplies `misl_rom` and `misl_rom_len`. It can be
included by a native host compiler without assembling RISC-V source. The current
image is 1,520 bytes and retains ROM compatibility version `2026091001`.
This value is a contract version, not a source modification date.

To regenerate an image, preprocess and assemble `misl_rom.S` for
RV32IMFD/ILP32D, link its text at zero using `misl_rom_start` as the linker entry,
then extract the linked `.text` section and embed those bytes. Linking is required:
the offset-table expressions can remain relocations in an object file. Extracting
an unlinked object is not a valid image-generation procedure.

Keep the embedding symbols and verify the embedded length, header, table and
bytes against the linked output. The linked image is position-independent:
mapping it at a different guest base does not require changing its table values.
Source comment or macro-name edits do not require a compatibility-version change
when the image and contract are unchanged.
