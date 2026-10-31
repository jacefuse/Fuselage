// Software Handled Audio Rendering Pipeline. SEE: (SHARP.md, Overview)
#ifndef SHARP_H
#define SHARP_H
#include <stdint.h>
#include <stdbool.h>

#define SHARP_VERSION "0.5.2026102901 ENTERON"

// Init returns whether audio started. SEE: (SHARP.md, Lifecycle)
bool SHARP_Init(void);
void SHARP_Shutdown(void);
void SHARP_SetPoolMB(int pool_mib); // Before init; nonpositive selects the default.
bool SHARP_IsAvailable(void);
uint32_t SHARP_GetSampleRate(void); // Frames per second; zero without a backend.

// Rendered frames, ahead of device playback. SEE: (SHARP.md, Timing and writes)
uint64_t SHARP_GetRenderPosition(void);

// Shared sample bytes. SEE: (SHARP.md, Sample pool)
uint32_t SHARP_GetPoolSize(void);
bool SHARP_WritePool(uint32_t offset, const void* data, uint32_t bytes);

typedef enum {
    SHARP_CHIP_ADORA = 0,
    SHARP_CHIP_SWEENY,
    SHARP_CHIP_AMY,
    SHARP_CHIP_JENNY,
} SharpChipKind;

// Create returns a chip ID or -1. SEE: (SHARP.md, Chips)
int SHARP_CreateChip(SharpChipKind kind);
int SHARP_GetChipCount(void);
const char* SHARP_GetChipName(int chip);

// One producer; timestamps are absolute render frames.
// SEE: (SHARP.md, Timing and writes)
bool SHARP_Write(int chip, uint16_t reg, uint16_t value);
bool SHARP_WriteAt(int chip, uint16_t reg, uint16_t value, uint64_t position);
uint32_t SHARP_GetDroppedEventCount(void);
uint32_t SHARP_GetLateEventCount(void);
uint32_t SHARP_GetDisorderedEventCount(void);

#define SHARP_BUSES 16
#define SHARP_CAPTURE_MASTER (-1)
#define SHARP_FMT_S16 0
#define SHARP_FMT_F32 1

// Capture reads return frames copied, or zero. SEE: (SHARP.md, Capture)
uint32_t SHARP_GetCaptureMaxFrames(void);
bool SHARP_SetCapture(int bus, uint32_t frames, bool enabled);
uint32_t SHARP_ReadCapture(int bus, void* destination, uint32_t max_frames, int format, int channels);

// Stereo balance attenuates one side. SEE: (SHARP.md, Mixer)
bool SHARP_SetChipBus(int chip, int bus);
int SHARP_GetChipBus(int chip);
bool SHARP_EnableBus(int bus, bool enabled);
bool SHARP_SetBusVolume(int bus, float volume);
bool SHARP_SetBusBalance(int bus, float balance);
void SHARP_SetMasterVolume(float gain);
#endif
