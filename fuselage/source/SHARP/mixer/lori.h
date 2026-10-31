// LORI stereo bus mixer. SEE: (SHARP.md, LORI composition interface)
#ifndef LORI_H
#define LORI_H
#include <stdint.h>

#define LORI_BUSES 16
#define LORI_BLOCK 256

typedef struct {
    float volume;
    float balance;
    int enabled;
} LoriBus;

typedef struct {
    float master_gain;
    LoriBus bus[LORI_BUSES];
    float scratch[LORI_BUSES][LORI_BLOCK * 2];
} Lori;

// Init clears storage and disables every bus; no allocation or device is needed.
void InitLori(Lori* mixer, float master_gain);
void SetLoriMasterGain(Lori* mixer, float gain);
int EnableLoriBus(Lori* mixer, int bus, int enabled);
int SetLoriBusVolume(Lori* mixer, int bus, float volume);
int SetLoriBusBalance(Lori* mixer, int bus, float balance);
int IsLoriBusEnabled(const Lori* mixer, int bus);

// Populate interleaved stereo scratch for each chunk. SEE: (SHARP.md, LORI composition interface)
typedef void (*LoriFillFn)(void* user, float* const* buses, int bus_count, uint32_t frames);

// Overwrite the master output with the mixed, gain-adjusted and clipped frames.
void RenderLori(Lori* mixer, LoriFillFn fill, void* user, float* master, uint32_t frames);
// Apply master gain and clipping in place, without bus mixing.
void ProcessLori(Lori* mixer, float* stereo, uint32_t frames);
#endif
