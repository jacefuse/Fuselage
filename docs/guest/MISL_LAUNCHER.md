# Native ROAP launch modes

The disk launcher loads a ROAP into owned memory. Embedded mode compiles generated
ROAP bytes into a native executable. Both use the same VIVIAN/MISL host tick and
ABI; neither mode changes the ROAP file format.

## Lifecycle

Include `MISL/host/misl_launcher.h` for disk-launcher entry points.
`FuselageInitLauncher` returns:

| Result | Meaning |
|---|---|
| `FUSELAGE_LAUNCH_FAILED` (0) | Options, file loading or host image preflight failed |
| `FUSELAGE_LAUNCH_RUN` (1) | Run a loaded file, or show the missing-default-program screen |
| `FUSELAGE_LAUNCH_DONE` (2) | Help, version or information request completed |

Initialize before engine startup. Informational paths do not apply native engine
configuration or open saves. `--info` uses the same host image preflight as execution;
a recognized malformed manifest is refused rather than described as a legacy image.
Preflight acceptance does not verify all assets, the guest's runtime ABI declaration,
save-file readiness or device availability.

For RUN, apply the requested display mode and register `FuselageTickLauncher` as
the engine tick. It calls the combined guest tick and halt view; do not add separate
input/audio pumps around it. Stop callbacks before shutdown or reinitialization.
`FuselageShutdownLauncher` detaches owned guest mappings before freeing file bytes.
It does not save: the root lifecycle must first choose clean publication or
abandonment, then finish and release storage in order.

Launcher state resets between module initializations. This does not promise that
the full engine supports restart or live program switching. The
[root lifecycle](../engine/FUSELAGE.md), [display pacing](../engine/FUSELAGE_PACING.md) and
[native build modes](../engine/FUSELAGE_BUILD.md) provide the integrated process lifecycle.

## Command line

Options and one path may appear in either order. `--` ends option parsing, allowing
a filename beginning with a dash. With no path, the launcher locates `GAME.ROAP`
beside its executable; failure to locate that directory is reported.

## No program supplied

When no ROAP path was supplied and `GAME.ROAP` does not exist beside the
executable, the launcher opens its default window. Diagnostic Text shows the
Fuselage version, explains that no program was found and prompts for any key to
quit. No guest or guest save is opened. Closing this screen returns failure status
1. An explicit missing path, an invalid existing file and informational requests
retain their command-line failure behavior.

## Options

| Option | Effect |
|---|---|
| `--fullscreen` | Borderless fullscreen |
| `--fullscreen-exclusive` | Exclusive fullscreen |
| `--windowed` | Windowed; the last display option wins |
| `--save-dir <dir>` | Existing save directory; empty selects beside-program storage |
| `--capture-dir <dir>` | Allow captures there; empty disables guest captures |
| `--max-memory <MiB>` | Positive decimal guest RAM cap, up to `UINT32_MAX` |
| `--exit-on-halt` | Print a halt verdict and request quit |
| `--exit-after <seconds>` | Finite positive run duration; an earlier halt reports its outcome |
| `--info` | Print accepted file configuration and exit |
| `--version`, `-v` | Print available component identities and contract versions |
| `--help`, `-h` | Print help and exit successfully |

The memory cap covers program and extra-pool RAM, not file storage, mapped save
bytes or all native allocations. The host's address-space bounds still apply.
Numeric overflow, trailing junk and nonfinite durations are refused. Directory
and program paths are bounded and rejected instead of silently truncated. Relative
paths retain their meaning only while the process working directory stays stable.
Windows paths retain the current narrow-character limitations.

File loading checks the host size limit before allocation, uses 64-bit file sizing,
and checks full reads and close status. Accepted bytes and the manifest title remain
owned by launcher state until shutdown; command-line argument storage is not retained.

## Reports

Help/version/info use standard output; failures and run verdicts use standard error.
The Windows console helper preserves redirected files and pipes instead of reopening
those streams on the console. A debug build (`DEBUG`) also appends one halt record
per loaded run to `roap_halt.log` in the working directory. Release builds do not
write that log. The log is a launcher diagnostic, separate from SRAM persistence.

Read `FuselageGetLauncherExitCode` after the run:

| Code | Outcome |
|---|---|
| 0 | Informational completion, deliberate guest finish, or elapsed run duration |
| 1 | Failure before execution |
| 3 | Guest execution fault |
| 4 | Save-load refusal |

A requested timed run that halts early reports the halt rather than claiming a
successful elapsed run. Verdicts are issued once and no further guest work is
performed while quit is pending. Quoted halt reasons escape control characters.
Normal interactive runs still use the halt panel and Escape unless an automatic
run option was selected.

## Timing reports

Duration begins at the launcher's first execution tick. Samples after a one-second
warm-up report the mean/minimum/maximum of GDMF's rolling FPS readings. These are
sampled rates, not an exact count of presentations over that interval. Simulation
frequency uses intervals between samples; guest throughput uses instruction
attempts, including faulting attempts.

The pacing status comes from the root controller under its state lock, as the tick
callback requires. Launcher tests supply that controller's report through a fixture;
the root controller has separate trace/worker tests, while actual display lock
behavior still needs hardware checks.

## Embedded programs

Generate `game_roap_bin.h` with the supplied `bin2h` tool using array name
`game_roap_code`. Only `misl_embedded.c` includes the generated bytes, avoiding a
copy in every including translation unit. Register `FuselageTickEmbeddedGuest`,
or include `misl_embedded.h` and call `FuselageRunGuest(FuselageGetEmbeddedGuest())`
from a deliberately mixed native/guest host.

The descriptor has stable lifetime and uses the tool's reported length. Empty
input therefore remains empty, despite the C array placeholder, and is refused by
host preflight. Entry zero derives the manifest entry or selects legacy offset zero.
Native build rules must compile this source only when its generated header exists.
The embedded native mode is selected by the [host Makefile](../engine/FUSELAGE_BUILD.md).
A guest SDK application does not need to build this native host.
