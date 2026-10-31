#ifndef GDMF_DISPLAY_MODE_H
#define GDMF_DISPLAY_MODE_H

// Shared by the public API and platform window interfaces.
typedef enum {
    GDMF_MODE_WINDOWED,
    GDMF_MODE_BORDERLESS,
    GDMF_MODE_FULLSCREEN_EXCLUSIVE,
} GDMF_DisplayMode;

#endif // GDMF_DISPLAY_MODE_H
