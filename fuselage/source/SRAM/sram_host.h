/* Native host borrowing; not part of guest SDK inputs. */
#ifndef SRAM_HOST_H
#define SRAM_HOST_H
#include "sram.h"
/* Read-only view, invalidated by Configure, Open and Close. Tick-thread use only. */
const uint8_t* sram_get_host_span(uint32_t* byte_count);
#endif
