#pragma once

// Shared MISL memory contracts; no engine implementation headers are required.
// Asset file records belong to ROAP, independently of their guest mapping.
// SEE: (MISL_ABI_MMIO.md, Header organization)
#include "abi/misl_mmio_map.h"
#include "abi/misl_mmio_audio.h"
#include "abi/misl_mmio_graphics.h"
#include "abi/misl_mmio_input.h"
#include "../ROAP/roap_asset_rom.h"
