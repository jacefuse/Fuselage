#ifndef GDMF_PIXIES_H
#define GDMF_PIXIES_H

// Persistent pixels, geometry, and priority masks. SEE: (GDMF_PIXIES.md, Modes)
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "gdmf_pixies_commands.h"
#include "gdmf_pixies_formats.h"

#define GDMF_PIXIES_VERSION "0.5.2026103001 ENTERON"
#define MAX_PIXIES 16
#define PIXIE_RAM_SIZE (4 * 1024 * 1024)
#define PIXIE_OUTPUT_STRING_LEN 256

typedef enum {
    PIXIE_MODE_TEXTURE = 0,
    PIXIE_MODE_LIVE = 1,
    PIXIE_MODE_MASK = 2,
} PixieMode;

// False on invalid ID/mode, invalid or oversized dimensions, failed backend
// handoff, or allocation failure. SEE: (GDMF_PIXIES.md, Host state)
// Reinitialization releases existing contents first; failure does not restore them.
// New pixies are disabled and hidden. SEE: (GDMF_PIXIES.md, Lifecycle)
bool InitPixie(int id, PixieMode mode, int output_width, int output_height);
// False for unavailable IDs or failed backend handoff; failure retains logical state.
bool ReleasePixie(int id);
// Requires a quiesced renderer; reports retained state/resources through diagnostics.
void ShutdownPixies(void);
// False when a checked command requirement is refused. Not GPU completion.
// SEE: (GDMF_PIXIES_COMMANDS.md, Command packets)
bool IssuePixieCommand(int id, PixieOpcode opcode, uint16_t flags, const uint32_t arguments[4]);

// Slots 1..15 are writable; slot 0 is readable identity. Rotation is in degrees.
// SEE: (GDMF_PIXIES_COMMANDS.md, Transforms)
bool SetPixieTransform(int id, int slot, float translation_x, float translation_y, float scale_x,
                       float scale_y, float rotation_degrees);
bool SetPixieTransformMatrix(int id, int slot, const PixieTransform* matrix);
bool GetPixieTransformMatrix(int id, int slot, PixieTransform* output);
bool ResetPixieTransform(int id, int slot);
bool SetPixieCurrentTransform(int id, int slot);
// Returns accepted top-level records, not nested-command totals. result may be NULL.
// SEE: (GDMF_PIXIES_COMMANDS.md, Lists and results)
uint32_t RunPixieCommandList(const PixieCommand* commands, uint32_t count, PixieListResult* result);

// RAM writes permit overlap and retain no caller buffer. False for NULL data,
// invalid IDs, or a range outside RAM. SEE: (GDMF_PIXIES.md, RAM and diagnostics)
bool WritePixieRAM(int id, size_t offset, const void* data, size_t size);
// RAM size for an initialized pixie, otherwise 0.
size_t GetPixieRAMSize(int id);
// Borrowed read-only RAM, or NULL. Invalidated by release/reinitialization.
const void* GetPixieRAMReadPtr(int id);
// Bytes copied excluding NUL; truncates and terminates when capacity is nonzero.
// Invalid IDs read the shared diagnostic buffer. NULL/zero capacity returns 0.
size_t ReadPixieString(int id, char* buffer, size_t capacity);
// Copies host mask bytes, not a GPU readback. Returns 0 for invalid/non-mask IDs
// or NULL/zero capacity. Copies at most capacity bytes, without a terminator.
size_t ReadPixieMask(int id, unsigned char* buffer, size_t capacity);

// Placement uses reference-canvas coordinates; dimensions must be positive.
// Setters below return false for invalid or uninitialized IDs.
bool SetPixiePosition(int id, float x, float y);
bool SetPixieDisplaySize(int id, float width, float height);
bool SetPixiePriority(int id, unsigned char priority);
bool SetPixieEnabled(int id, bool enabled);
bool ShowPixie(int id);
bool HidePixie(int id);

// Pattern bitmap is tiled RGBA8 in pixie RAM; clear returns to draw-pattern stipple.
// False for invalid IDs, zero dimensions, or a bitmap range outside RAM.
bool SetPixiePatternBitmap(int id, uint32_t ram_offset, uint16_t width, uint16_t height);
bool ClearPixiePatternBitmap(int id);
bool SetPixieDrawPattern(int id, uint16_t pattern);
uint16_t GetPixieDrawPattern(int id);
// Mask-only setters also return false on other modes. SEE: (GDMF_PIXIES.md, Masks)
bool SetPixieMaskInvert(int id, bool invert);
bool GetPixieMaskInvert(int id);
// Clockwise radians; refused for masks. Transform-slot rotation uses degrees.
bool SetPixieRotation(int id, float radians);
float GetPixieRotation(int id);
bool SetPixieHotspot(int id, float hotspot_x, float hotspot_y);
float GetPixieHotspotX(int id);
float GetPixieHotspotY(int id);
bool SetPixieRotateAroundHotspot(int id, bool enabled);
bool GetPixieRotateAroundHotspot(int id);
// Enabling fails unless the shared linear sampler is already available.
// SEE: (GDMF_PIXIES.md, Rotation and filtering)
bool SetPixieFilterLinear(int id, bool enabled);
bool GetPixieFilterLinear(int id);
// Invalid/uninitialized getter defaults: 0/false, TEXTURE mode, solid 0xFFFF
// draw pattern. SEE: (GDMF_PIXIES.md, Getter defaults)
bool GetPixieInitialized(int id);
PixieMode GetPixieMode(int id);
int GetPixieOutputWidth(int id);
int GetPixieOutputHeight(int id);
float GetPixieX(int id);
float GetPixieY(int id);
float GetPixieDisplayWidth(int id);
float GetPixieDisplayHeight(int id);
unsigned char GetPixiePriority(int id);
bool GetPixieEnabled(int id);
bool GetPixieShown(int id);

#endif // GDMF_PIXIES_H
