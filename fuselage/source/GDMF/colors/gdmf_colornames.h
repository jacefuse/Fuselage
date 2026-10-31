// Named RGBA8 color constants. Define the four-component Color type before
// using these macros. This header declares no functions or storage.

#ifndef GDMF_COLORNAMES_H
#define GDMF_COLORNAMES_H

// BLANK is transparent black; the other named colors are fully opaque.
#define BLANK       (Color){ 0, 0, 0, 0 }
#define BLACK       (Color){ 0, 0, 0, 255}
#define WHITE       (Color){ 255, 255, 255, 255 }
#define RED         (Color){ 255, 0, 0, 255 }
#define GREEN       (Color){ 0, 255, 0, 255 }
#define BLUE        (Color){ 0, 0, 255, 255 }
#define YELLOW      (Color){ 255, 255, 0, 255 }
#define MAGENTA     (Color){ 255, 0, 255, 255 }
#define CYAN        (Color){ 0, 255, 255, 255 }
#define GRAY        (Color){ 128, 128, 128, 255 }
#define DARKGRAY    (Color){ 80, 80, 80, 255 }
#define LIGHTGRAY   (Color){ 200, 200, 200, 255 }
#define ORANGE      (Color){ 255, 165, 0, 255 }
#define PINK        (Color){ 255, 192, 203, 255 }
#define PURPLE      (Color){ 128, 0, 128, 255 }
#define BROWN       (Color){ 165, 42, 42, 255 }

#endif // GDMF_COLORNAMES_H
