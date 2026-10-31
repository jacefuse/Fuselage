/* Native dispatch contracts; scheduling and persistence integrate separately. */
#ifndef MISL_HOST_SERVICES_INTERNAL_H
#define MISL_HOST_SERVICES_INTERNAL_H
#include <stdint.h>

int fuselage_guest_service_ecall(void);
int fuselage_guest_gate_abi(uint32_t call);
int fuselage_guest_declare_abi(void);
int fuselage_guest_unknown_ecall(uint32_t call);
void fuselage_guest_report_fault(const char* reason);
void fuselage_guest_report_abi_mismatch(uint32_t requested_version);

int fuselage_guest_service_system(uint32_t call);
int fuselage_guest_service_text(uint32_t call);
int fuselage_guest_service_sprites(uint32_t call);
int fuselage_guest_service_tiles(uint32_t call);
int fuselage_guest_service_pixies(uint32_t call);
int fuselage_guest_service_colors(uint32_t call);
int fuselage_guest_service_interactions(uint32_t call);
int fuselage_guest_service_input(uint32_t call);
int fuselage_guest_service_audio(uint32_t call);
int fuselage_guest_service_dice(uint32_t call);
int fuselage_guest_service_rom(uint32_t call);
#endif
