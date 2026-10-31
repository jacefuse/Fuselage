/* Private BOOTSPUR services; integer registers carry floating representations. */
#ifndef MISL_HOST_ROM_INTERNAL_H
#define MISL_HOST_ROM_INTERNAL_H
#include <stdint.h>
int fuselage_guest_rom_bad_pointer(uint32_t call, uint32_t address, uint32_t byte_count);
int fuselage_guest_rom_unimplemented(uint32_t call);
int fuselage_guest_rom_memory(uint32_t call);
int fuselage_guest_rom_trace(uint32_t call, uint32_t address);
int fuselage_guest_rom_write_save(uint32_t call);
#endif
