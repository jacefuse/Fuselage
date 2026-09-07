#pragma once

// MISLlauncher -- the standalone shipping mode.
//
// A generic Fuselage host that plays a ROAP loaded from disk at runtime, as
// opposed to the default mode where a blob is baked into the executable (see
// roap_embedded.h). One MISLlauncher.exe runs any MISL game:
//
//     MISLlauncher.exe            -- plays GAME.ROAP from its own directory
//     MISLlauncher.exe other.roap -- plays the blob you name
//
// This mode supplies its own game() -- it plays whatever blob it was handed,
// so there is no game.c to author. It is built by `make launcher` and is the
// only configuration that reads argv.

// Resolve and load the blob, before the engine starts. argv[1] if given, else
// GAME.ROAP beside the executable. Returns 0 and reports to stderr if the
// blob is missing, unreadable, empty, or too large for VPU memory -- in which
// case main() should exit rather than open a window on nothing to play.
int RoapLauncherInit(int argc, char **argv);

// Release the loaded blob. Safe to call whether or not init succeeded.
void RoapLauncherShutdown(void);
