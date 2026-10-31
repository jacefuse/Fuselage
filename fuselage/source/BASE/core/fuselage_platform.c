/* Process setup before graphics initialization. SEE: (FUSELAGE.md, Windows DPI) */
#include "fuselage_internal.h"
void fuselage_prepare_process(void) {
#if defined(_WIN32)
    typedef BOOL(WINAPI * SetDpiAwareness)(DPI_AWARENESS_CONTEXT);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    SetDpiAwareness set_awareness =
        user32 ? (SetDpiAwareness)(void*)GetProcAddress(user32, "SetProcessDpiAwarenessContext") : NULL;
    if (set_awareness) {
        (void)set_awareness(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }
#endif
}
