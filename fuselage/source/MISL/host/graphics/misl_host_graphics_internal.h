/* Native graphics bridge storage helpers. */
#ifndef MISL_HOST_GRAPHICS_INTERNAL_H
#define MISL_HOST_GRAPHICS_INTERNAL_H
#include <stdint.h>
void* fuselage_guest_copy_records(uint32_t address, uint32_t count, uint32_t record_bytes);
void* fuselage_guest_resolve_record_output(uint32_t address, uint32_t count, uint32_t record_bytes);
void fuselage_guest_install_graphics_watch(void);
void fuselage_guest_flush_graphics(void);
#endif
