#include "fuselage_misl.h"

MISL_PROJECT(.title = "Minimal ROAP");

__attribute__((section(".text.init"), noreturn)) void _start(void) {
    MISL_PROGRAM_BEGIN();
    for (;;) {
        MISL_Yield();
    }
}
