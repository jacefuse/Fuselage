# Fuselage applications

## Choosing a target

| Target | Application supplies | Execution and delivery |
|---|---|---|
| Native C application | `project_config.h` and source defining `tick()` | Compiles for the host platform and uses native Fuselage interfaces. |
| MISL guest | Guest source, configuration and assets | Compiles to a ROAP, executed by VIVIAN through MISL. A launcher loads the file. |
| Embedded MISL application | A built ROAP and host `project_config.h` | A platform-specific executable contains the same ROAP that a launcher can load. |

MISL is the guest interface, VIVIAN is the emulated CPU, and ROAP is the file
format. Embedding changes delivery; it does not convert guest code to native
code. Build an embedded executable separately for each host platform.

The engine supplies `main()` and the native application loop. Native project
code supplies `tick()`; embedded and launcher modes use engine-owned guest
callbacks. Applications using an independent subsystem directly can provide
their own entry point without using the full Fuselage loop.

See [native builds](FUSELAGE_BUILD.md), [building a ROAP](../guest/BUILDING_A_ROAP.md)
and [native launch modes](../guest/MISL_LAUNCHER.md) for commands and requirements.
Guest-only development uses the SDK and RISC-V tools, without building the native
launcher. Preparing an embedded executable additionally requires the host build
dependencies.

## Project boundaries

Keep application source, assets and configuration together. Declare external
dependencies and asset licences in the application's README. Avoid dependencies
on a developer's checkout locations, private test tools or another example's
unpublished files. Pass project paths to the build rather than assuming a fixed
directory depth.

Keep guest source in one place when shipping both a ROAP and its embedded form.
Embedding consumes the built ROAP, so it does not need a second guest source tree.
For applications with both native and guest implementations, sharing application
logic can prevent fixes from drifting. The guest and native interfaces differ in
ownership, memory access and available services; matching function names alone
do not make source portable. Document any target-specific adapters and behaviour.

Generated application objects and embedded headers belong in build directories.
Mark ROAP files and other binary assets as binary in version control so text
line-ending conversion cannot alter their contents. Checked-in engine data has
different purposes; see [source layout](SOURCE_LAYOUT.md#generated-data).

## Updating an application

Rebuild selected guest sources against the current SDK. Review changed function
names, return values and data layouts using the current headers and guides.
The runtime ABI declaration checks compatibility at the ordinary ECALL boundary;
it does not prove that every application assumption is correct.

Check semantics as well as compilation: drawing priority, coordinate types,
palette selection, timing, collision results and resource lifetimes can change
behaviour without producing compiler errors. Build native and guest targets
separately and test their visible behaviour before treating a port as complete.
Record deliberate behaviour differences and unresolved issues with the project.

Diagnostic text uses a byte-indexed glyph table rather than a UTF-8 text renderer.
Use characters supported by that table for captions. See
[Diagnostic Text](../graphics/GDMF_TEXT.md).
