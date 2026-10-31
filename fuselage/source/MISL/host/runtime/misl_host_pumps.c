/* Shared pre-slice order for launcher and embedded hosts. */
#include "misl_host_scheduler_internal.h"
#include "misl_host_pages_internal.h"
void fuselage_guest_pump_devices(void) {
    fuselage_guest_pump_input();
    fuselage_guest_fill_clock(fuselage_guest_get_clock_page());
    fuselage_guest_pump_audio();
    fuselage_guest_refresh_pixie_banks();
    fuselage_guest_refresh_tile_banks();
}
