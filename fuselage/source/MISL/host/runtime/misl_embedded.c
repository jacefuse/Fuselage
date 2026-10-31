/* Engine-owned embedded callback. SEE: (MISL_LAUNCHER.md, Embedded programs) */
#include "misl_embedded.h"
#include "game_roap_bin.h"
_Static_assert(sizeof game_roap_code <= FUSELAGE_GUEST_MAX_FILE_BYTES,
               "Embedded ROAP exceeds the host limit");
const MislGuestImage* FuselageGetEmbeddedGuest(void) {
    static MislGuestImage image = {game_roap_code, 0, 0};
    if (!image.byte_count) {
        image.byte_count = game_roap_code_len <= sizeof game_roap_code ? game_roap_code_len : 0;
    }
    return &image;
}
void FuselageTickEmbeddedGuest(void) {
    FuselageRunGuest(FuselageGetEmbeddedGuest());
    FuselageHandleGuestHalt();
}
