# Fuselage conventions

These conventions guide naming and application structure in Fuselage. Existing
code is checked against them as it is reviewed.

## System boundaries

GDMF, CAKE, DICE, SHARP, SRAM and VIVIAN must each build and operate without the other systems
or the Fuselage orchestrator. Fuselage initializes, coordinates and connects them.
Keep cross-system orchestration out of system implementations and public headers.

A direct dependency on another system requires a specific need and a recorded
reason before it is added. Prefer a narrow data or callback interface when the
orchestrator can supply the connection. Small shared platform utilities must
remain independent of system state and orchestrator lifecycle.

## Function names

Public function names identify the part of the engine that owns the operation.

| Level | Pattern | Examples |
|---|---|---|
| Fuselage orchestrator | `FuselageVerbNoun` | `FuselageSetTitle`, `FuselageWaitFrame` |
| Standalone system | `SYSTEM_VerbNoun` | `GDMF_SetVSync`, `DICE_InitTimer`, `CAKE_GetControllerName` |
| Subsystem within a system | `VerbSubsystemNoun` | `SetSpritePosition`, `SetTilePalette`, `InitColorsPalettes` |

The system pattern is **SVN**: system, verb, noun. The subsystem pattern is
**VSN**: verb, subsystem, noun. Parts that add no information may be omitted,
as in `GDMF_Init` and `InitSprites`.

Format helpers use their format's ownership prefix, such as
`ROAP_GetManifestCodeOffset`. ROAP still names the file format, not the execution
core or host session. These helpers must not depend on either implementation.

Native MISL session operations belong to Fuselage orchestration and use names
such as `FuselageRunGuest` and `FuselageGetGuestHaltInfo`. Their private helpers
use `fuselage_guest_` and live under `MISL/host`. This host integration is not an
additional independent system. Keep `MISL_` guest services, ROAP format helpers
and the independent `VIVIAN_` core distinct from those native host operations.

GDMF, CAKE, DICE, SHARP, SRAM and VIVIAN are standalone systems. Sprites, Tiles, Pixies,
Colors, and Interactions are subsystems within GDMF. Cross-subsystem helpers
owned by Interactions follow the same rule, such as
`GetInteractionsSpriteDistance` and `CheckInteractionsSpriteTileCollision`.

LORI, Adora, Sweeny, Amy and Jenny are named subsystems within SHARP. Their public component
functions also use VSN, such as `InitLori`, `SetLoriBusVolume`, `RenderAdora` and
`GetSweenySource`, even when an application uses a component independently.
General SHARP composition services use SVN, such as `SHARP_InitSequence`.
Internal helpers use lowercase ownership prefixes such as `sharp_lori_`,
`sharp_adora_`, `sharp_sweeny_`, `sharp_amy_` and `sharp_jenny_`.

Use PascalCase for the words following a system's underscore and for public
subsystem functions. Subsystem function names have no underscore.

### Lifecycle and scope

Initialization and shutdown functions follow the same naming patterns as other
functions. Use a plural name when an operation affects the whole collection
and a singular name when it affects one item:

- `InitSprites()` initializes the sprite subsystem.
- `InitPixie(id, ...)` initializes one pixie.
- `ShutdownPixies()` shuts down all initialized pixies.
- `SetSpritePosition(...)` changes one sprite's position.

Name the owning subsystem even when an operation concerns a specialized part
of it. For example, `InitColorsDeepPalettes` identifies deep palettes as part
of Colors.

### Internal functions

Internal functions use a lowercase system prefix and snake_case, such as
`gdmf_get_device` and `gdmf_vulkan_init`. Keep their declarations in internal
headers, or make them `static` when they are used only within one source file.
The naming convention describes visibility; the declarations and linkage
enforce it.

Internal C command-recording callback types use a lowercase system/backend
prefix and a descriptive `_callback` suffix, such as
`gdmf_vulkan_command_record_callback`. Engine-owned Vulkan helper macros use
`GDMF_VULKAN_`; keep `VK_` names for the Vulkan interface itself.

### Platform implementation names

Platform implementation files use lowercase names with underscores, including
the system, platform, and concern: `gdmf_window_macos_presentation.m`.
Their C helpers and file-level state use a lowercase system/platform prefix,
such as `gdmf_macos_restore_presentation`. Local names use descriptive lowercase
words separated by underscores in these files.

Objective-C classes use a system and platform prefix followed by their role,
such as `GDMFMacOSWindow` and `GDMFMacOSMetalView`. This named category follows
Objective-C class naming practice and avoids runtime name collisions; uppercase
class names do not make these implementation types part of the public C API.
Application-defined Objective-C selectors use lowerCamelCase verb phrases,
such as `requestQuit:`. Framework-required selectors keep their original names
so overrides and delegate callbacks continue to work.

Shared interfaces belong in a common header. An exception must record its
reason and scope alongside the declaration.

### Deliberate exceptions

`fuselage()` is the engine's main-loop function.

Native projects supply the application callback `tick()`. It is an application
entry point, rather than an engine-system function.

The diagnostic text layer uses the lowercase `tl` prefix, as in `tlPrint` and
`tlSetCursor`. It is intended for engine messages, debugging, and tools.
Application text belongs on a dedicated tile layer.

An additional naming exception must address a misleading or unpronounceable
name and apply consistently to a named category. If exceptions become common,
review the convention itself.

Compiler and C-runtime entry symbols retain their required spellings, including
`_sbrk`, `sbrk`, `memcpy` and `__divdi3`. This exception applies to names that
external code emits or expects. Engine-owned helpers still follow the usual
system naming rules.

## Variables and parameters

GLSL helper functions and variables use descriptive lowercase words separated
by underscores. Shader interface block and structure names use PascalCase;
framework names such as `gl_GlobalInvocationID` retain their required spelling.

Local variable names should usually be lowercase. Choose meaningful names
that describe the value's purpose. Replace shorthand and unclear abbreviations
with descriptive words.

For example, prefer `palette_index` to `pi` when the value selects a palette.
This example illustrates clarity; it does not establish a separator rule for
all variable names.

## Application structure

A standard native Fuselage application supplies two integration files:

| File | Purpose |
|---|---|
| `project_config.h` | Compile-time configuration using `PROJECT_*` definitions |
| `project.c` | The application's `tick()` function and application logic |

Applications may have additional source files, headers, and assets. The engine
supplies `main()` and declares `tick()` in `fuselage.h`. A standard application
defines `tick()` without registering a callback or writing a separate header
for its declaration.

Configuration belongs in `project_config.h`, rather than an application
configuration callback. Definitions may use compile-time expressions; they do
not perform runtime hardware detection. Omitted settings receive the engine's
defaults. The meaning of zero depends on the setting.

A ROAP guest supplies its guest source, conventionally `misl.c`, and its
guest-side assets. It runs under the VPU rather than defining a native `tick()`.
A self-embedded MISL application supplies `project_config.h` and build rules
that embed an already-built ROAP. The engine supplies its native entry point
and guest-driving tick.

Standalone programs that use an individual system, including subsystem test
harnesses, may supply their own `main()`. Applications with their own native
event loop require individual review of their integration.

## Terminology

Use names that identify the thing being discussed:

| Term | Meaning |
|---|---|
| ROAP | The file format, a file in that format, or the program it contains |
| VIVIAN | The virtual processing unit that executes guest programs |
| MISL | The Machine Independent System Layer used by guests |
| Binary | Compiled output, when that distinction matters |
| Guest | A program executing under VIVIAN |

Do not use the generic term "blob" in source, comments, documentation, commit
messages, or program output. Use the specific name for the data or artifact.

A ROAP is a specific file format, not an unnamed collection of bytes. Call it
a ROAP, ROAP file, ROAP image, or embedded ROAP as appropriate. Embedding it in
a C array does not change its identity. Apply this terminology in every
ROAP-facing guide, example, source comment and tool message. Documentation about
ROAPs should refer to this section when stating the terminology rule.

## Comments and documentation

Private `.inc` files may hold parts of a single C function when separating
concerns would otherwise introduce unwanted calls or duplicate shared control
flow. Name the owning context and document the reason. Include each fragment
only from its owning source file; it is neither a public header nor a separately
compiled source. These fragments are required source distribution material.
Keep their file dependencies in the build manifest. VIVIAN's instruction-family
fragments use this exception to share one decoder loop and fault epilogue.

Keep comments concise and explain purpose, constraints, or behavior that the
code does not make clear. Put longer explanations in documentation and refer
to them from the code where useful.

Use ordinary sentence punctuation in comments and program messages. Do not
use double dashes as prose punctuation. Preserve required syntax, such as
decrement operators, command-line options, and Markdown table separators.

When a comment needs supporting context, append `SEE: (filename, entry)`.
The entry names a section in that document. Keep the explanation there and
retain only the immediate constraint or purpose in the comment.

Public documentation describes shipping behavior. Keep task lists, review
notes, and development history in internal tracking documents. Before removing
a reference to an unimplemented capability, record the outstanding work there.

## Version dates

When reviewing or changing a file that already has a dated version string, update
its date to the day of that work. Keep the ENTERON release name throughout
publication preparation. A date update does not announce FUNDUS or change API
compatibility. Files without a version string do not need one added for migration.

ABI, file-format and ROM compatibility versions are contracts, including values
that look like dates. Preserve them unless the corresponding contract change is
reviewed; they are not source modification stamps.
