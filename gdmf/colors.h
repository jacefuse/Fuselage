// GDMF Colors - palette and color utilities.
#ifndef COLORS_H
#define COLORS_H

#include <stdbool.h>
#include <stdint.h>

#define GDMF_COLORS_VERSION "0.2.2026070101 BUTTOCKS"

#define FUSELAGE_PALETTE_SIZE 16

// Color structure definition (RayLib compatible)
// Guard matches fuselage.h so both can coexist in the same translation unit.
#ifndef FUSELAGE_COLOR_TYPE
#define FUSELAGE_COLOR_TYPE
typedef struct Color {
    unsigned char r;        // Red component
    unsigned char g;        // Green component
    unsigned char b;        // Blue component
    unsigned char a;        // Alpha component
} Color;
#endif

// PackRGBA8 depends on Color being exactly 4 bytes with no padding -- true
// for any plain struct of four unsigned chars, but worth catching at
// compile time if Color is ever changed (extra fields, wider components,
// reordering) rather than discovering it as silently corrupted colors on
// screen.
_Static_assert(sizeof(Color) == 4, "Fuselage expects RGBA8 Color (4 bytes, no padding).");

// Packs a Color into the little-endian uint32 layout GPU-side shaders
// expect when reading a Color buffer as raw uints (r in bits 0-7, g in
// 8-15, b in 16-23, a in 24-31) -- see sprite.frag's PaletteBuffer unpack.
// Exists so GPU upload sites depend on this explicit, named contract
// instead of memcpy-ing Color's in-memory layout and hoping it matches.
uint32_t PackRGBA8(Color c);

// Basic color palette indices (for compatibility with existing code)
#define BLACK_PALETTE       0
#define WHITE_PALETTE       15
#define RED_PALETTE         1
#define GREEN_PALETTE       2
#define BLUE_PALETTE        3
#define YELLOW_PALETTE      4
#define MAGENTA_PALETTE     5
#define CYAN_PALETTE        6
#define GRAY_PALETTE        7
#define DARKGRAY_PALETTE    8
#define LIGHTGRAY_PALETTE   9

// Standard color constants (for reference - use palette indices in practice)
#define BLANK       (Color){ 0, 0, 0, 0 }        // Transparent
#define BLACK       (Color){ 0, 0, 0, 255 }      // Black
#define WHITE       (Color){ 255, 255, 255, 255 }// White
#define RED         (Color){ 255, 0, 0, 255 }    // Red
#define GREEN       (Color){ 0, 255, 0, 255 }    // Green
#define BLUE        (Color){ 0, 0, 255, 255 }    // Blue
#define YELLOW      (Color){ 255, 255, 0, 255 }  // Yellow
#define MAGENTA     (Color){ 255, 0, 255, 255 }  // Magenta
#define CYAN        (Color){ 0, 255, 255, 255 }  // Cyan
#define GRAY        (Color){ 128, 128, 128, 255 }// Gray
#define DARKGRAY    (Color){ 80, 80, 80, 255 }   // Dark Gray
#define LIGHTGRAY   (Color){ 200, 200, 200, 255 }// Light Gray

// Define the 256 colors as macros

// Grayscale
#define GDMF_COLOR0  (Color){0, 0, 0, 255} // BLACK
#define GDMF_COLOR1  (Color){17, 17, 17, 255}
#define GDMF_COLOR2  (Color){34, 34, 34, 255}
#define GDMF_COLOR3  (Color){51, 51, 51, 255}
#define GDMF_COLOR4  (Color){68, 68, 68, 255}
#define GDMF_COLOR5  (Color){85, 85, 85, 255}
#define GDMF_COLOR6  (Color){102, 102, 102, 255}
#define GDMF_COLOR7  (Color){119, 119, 119, 255}
#define GDMF_COLOR8  (Color){136, 136, 136, 255}
#define GDMF_COLOR9  (Color){153, 153, 153, 255}
#define GDMF_COLOR10 (Color){170, 170, 170, 255}
#define GDMF_COLOR11 (Color){187, 187, 187, 255}
#define GDMF_COLOR12 (Color){204, 204, 204, 255}
#define GDMF_COLOR13 (Color){221, 221, 221, 255}
#define GDMF_COLOR14 (Color){238, 238, 238, 255}
#define GDMF_COLOR15 (Color){255, 255, 255, 255} // WHITE

#define GDMF_COLOR16  (Color){16, 0, 0, 255} // DARK RED
#define GDMF_COLOR17  (Color){32, 0, 0, 255}
#define GDMF_COLOR18  (Color){48, 0, 0, 255}
#define GDMF_COLOR19  (Color){64, 0, 0, 255}
#define GDMF_COLOR20  (Color){80, 0, 0, 255}
#define GDMF_COLOR21  (Color){96, 0, 0, 255}
#define GDMF_COLOR22  (Color){112, 0, 0, 255}
#define GDMF_COLOR23  (Color){128, 0, 0, 255}
#define GDMF_COLOR24  (Color){144, 0, 0, 255}
#define GDMF_COLOR25  (Color){160, 0, 0, 255}
#define GDMF_COLOR26  (Color){176, 0, 0, 255}
#define GDMF_COLOR27  (Color){192, 0, 0, 255}
#define GDMF_COLOR28  (Color){208, 0, 0, 255}
#define GDMF_COLOR29  (Color){224, 0, 0, 255}
#define GDMF_COLOR30  (Color){240, 0, 0, 255}
#define GDMF_COLOR31  (Color){255, 0, 0, 255} // PURE RED

#define GDMF_COLOR32  (Color){0, 16, 0, 255} // DARK GREEN
#define GDMF_COLOR33  (Color){0, 32, 0, 255}
#define GDMF_COLOR34  (Color){0, 48, 0, 255}
#define GDMF_COLOR35  (Color){0, 64, 0, 255}
#define GDMF_COLOR36  (Color){0, 80, 0, 255}
#define GDMF_COLOR37  (Color){0, 96, 0, 255}
#define GDMF_COLOR38  (Color){0, 112, 0, 255}
#define GDMF_COLOR39  (Color){0, 128, 0, 255}
#define GDMF_COLOR40  (Color){0, 144, 0, 255}
#define GDMF_COLOR41  (Color){0, 160, 0, 255}
#define GDMF_COLOR42  (Color){0, 176, 0, 255}
#define GDMF_COLOR43  (Color){0, 192, 0, 255}
#define GDMF_COLOR44  (Color){0, 208, 0, 255}
#define GDMF_COLOR45  (Color){0, 224, 0, 255}
#define GDMF_COLOR46  (Color){0, 240, 0, 255}
#define GDMF_COLOR47  (Color){0, 255, 0, 255} // PURE GREEN

#define GDMF_COLOR48  (Color){0, 0, 16, 255} // DARK BLUE
#define GDMF_COLOR49  (Color){0, 0, 32, 255}
#define GDMF_COLOR50  (Color){0, 0, 48, 255}
#define GDMF_COLOR51  (Color){0, 0, 64, 255}
#define GDMF_COLOR52  (Color){0, 0, 80, 255}
#define GDMF_COLOR53  (Color){0, 0, 96, 255}
#define GDMF_COLOR54  (Color){0, 0, 112, 255}
#define GDMF_COLOR55  (Color){0, 0, 128, 255}
#define GDMF_COLOR56  (Color){0, 0, 144, 255}
#define GDMF_COLOR57  (Color){0, 0, 160, 255}
#define GDMF_COLOR58  (Color){0, 0, 176, 255}
#define GDMF_COLOR59  (Color){0, 0, 192, 255}
#define GDMF_COLOR60  (Color){0, 0, 208, 255}
#define GDMF_COLOR61  (Color){0, 0, 224, 255}
#define GDMF_COLOR62  (Color){0, 0, 240, 255}
#define GDMF_COLOR63  (Color){0, 0, 255, 255} // PURE BLUE

#define GDMF_COLOR64  (Color){16, 16, 0, 255} // BROWN
#define GDMF_COLOR65  (Color){32, 32, 0, 255}
#define GDMF_COLOR66  (Color){48, 48, 0, 255}
#define GDMF_COLOR67  (Color){64, 64, 0, 255}
#define GDMF_COLOR68  (Color){80, 80, 0, 255}
#define GDMF_COLOR69  (Color){96, 96, 0, 255}
#define GDMF_COLOR70  (Color){112, 112, 0, 255}
#define GDMF_COLOR71  (Color){128, 128, 0, 255}
#define GDMF_COLOR72  (Color){144, 144, 0, 255}
#define GDMF_COLOR73  (Color){160, 160, 0, 255}
#define GDMF_COLOR74  (Color){176, 176, 0, 255}
#define GDMF_COLOR75  (Color){192, 192, 0, 255}
#define GDMF_COLOR76  (Color){208, 208, 0, 255}
#define GDMF_COLOR77  (Color){224, 224, 0, 255}
#define GDMF_COLOR78  (Color){240, 240, 0, 255}
#define GDMF_COLOR79  (Color){255, 255, 0, 255} // YELLOW

#define GDMF_COLOR80  (Color){16, 0, 16, 255} // DARK PURPLE
#define GDMF_COLOR81  (Color){32, 0, 32, 255}
#define GDMF_COLOR82  (Color){48, 0, 48, 255}
#define GDMF_COLOR83  (Color){64, 0, 64, 255}
#define GDMF_COLOR84  (Color){80, 0, 80, 255}
#define GDMF_COLOR85  (Color){96, 0, 96, 255}
#define GDMF_COLOR86  (Color){112, 0, 112, 255}
#define GDMF_COLOR87  (Color){128, 0, 128, 255}
#define GDMF_COLOR88  (Color){144, 0, 144, 255}
#define GDMF_COLOR89  (Color){160, 0, 160, 255}
#define GDMF_COLOR90  (Color){176, 0, 176, 255}
#define GDMF_COLOR91  (Color){192, 0, 192, 255}
#define GDMF_COLOR92  (Color){208, 0, 208, 255}
#define GDMF_COLOR93  (Color){224, 0, 224, 255}
#define GDMF_COLOR94  (Color){240, 0, 240, 255}
#define GDMF_COLOR95  (Color){255, 0, 255, 255} // MAGENTA

#define GDMF_COLOR96   (Color){0, 16, 16, 255} // TEAL
#define GDMF_COLOR97   (Color){0, 32, 32, 255}
#define GDMF_COLOR98   (Color){0, 48, 48, 255}
#define GDMF_COLOR99   (Color){0, 64, 64, 255}
#define GDMF_COLOR100  (Color){0, 80, 80, 255}
#define GDMF_COLOR101  (Color){0, 96, 96, 255}
#define GDMF_COLOR102  (Color){0, 112, 112, 255}
#define GDMF_COLOR103  (Color){0, 128, 128, 255}
#define GDMF_COLOR104  (Color){0, 144, 144, 255}
#define GDMF_COLOR105  (Color){0, 160, 160, 255}
#define GDMF_COLOR106  (Color){0, 176, 176, 255}
#define GDMF_COLOR107  (Color){0, 192, 192, 255}
#define GDMF_COLOR108  (Color){0, 208, 208, 255}
#define GDMF_COLOR109  (Color){0, 224, 224, 255}
#define GDMF_COLOR110  (Color){0, 240, 240, 255}
#define GDMF_COLOR111  (Color){0, 255, 255, 255} // CYAN

// Transitionals
#define GDMF_COLOR112  (Color){255, 64, 0, 255}    // Red-Orange
#define GDMF_COLOR113  (Color){255, 80, 0, 255}    // Transition 1
#define GDMF_COLOR114  (Color){255, 96, 0, 255}    // Transition 2
#define GDMF_COLOR115  (Color){255, 112, 0, 255}   // Transition 3
#define GDMF_COLOR116  (Color){255, 128, 0, 255}   // Orange
#define GDMF_COLOR117  (Color){255, 144, 0, 255}   // Transition 1
#define GDMF_COLOR118  (Color){255, 160, 0, 255}   // Transition 2
#define GDMF_COLOR119  (Color){255, 176, 0, 255}   // Transition 3
#define GDMF_COLOR120  (Color){255, 192, 0, 255}   // Yellow-Orange
#define GDMF_COLOR121  (Color){255, 208, 0, 255}   // Transition 1
#define GDMF_COLOR122  (Color){255, 224, 0, 255}   // Transition 2
#define GDMF_COLOR123  (Color){255, 240, 0, 255}   // Transition 3
#define GDMF_COLOR124  (Color){255, 255, 0, 255}   // Yellow
#define GDMF_COLOR125  (Color){224, 255, 0, 255}   // Transition 1
#define GDMF_COLOR126  (Color){192, 255, 0, 255}   // Transition 2
#define GDMF_COLOR127  (Color){160, 255, 0, 255}   // Transition 3
#define GDMF_COLOR128  (Color){128, 255, 0, 255}   // Lime Green
#define GDMF_COLOR129  (Color){96, 255, 0, 255}    // Transition 1
#define GDMF_COLOR130  (Color){64, 255, 0, 255}    // Green
#define GDMF_COLOR131  (Color){32, 255, 16, 255}   // Transition 1
#define GDMF_COLOR132  (Color){0, 255, 64, 255}    // Green-Teal
#define GDMF_COLOR133  (Color){0, 255, 80, 255}    // Transition 1
#define GDMF_COLOR134  (Color){0, 255, 96, 255}    // Transition 2
#define GDMF_COLOR135  (Color){0, 255, 112, 255}   // Transition 3
#define GDMF_COLOR136  (Color){0, 255, 128, 255}   // Teal
#define GDMF_COLOR137  (Color){0, 255, 144, 255}   // Transition 1
#define GDMF_COLOR138  (Color){0, 255, 160, 255}   // Transition 2
#define GDMF_COLOR139  (Color){0, 255, 176, 255}   // Transition 3
#define GDMF_COLOR140  (Color){0, 255, 192, 255}   // Aqua
#define GDMF_COLOR141  (Color){0, 240, 208, 255}   // Transition 1
#define GDMF_COLOR142  (Color){0, 224, 224, 255}   // Transition 2
#define GDMF_COLOR143  (Color){0, 208, 240, 255}   // Transition 3
#define GDMF_COLOR144  (Color){0, 192, 255, 255}   // Blue-Cyan
#define GDMF_COLOR145  (Color){0, 160, 255, 255}   // Transition 1
#define GDMF_COLOR146  (Color){0, 128, 255, 255}   // Light Blue
#define GDMF_COLOR147  (Color){0, 96, 255, 255}    // Transition 1
#define GDMF_COLOR148  (Color){0, 64, 255, 255}    // Bright Blue
#define GDMF_COLOR149  (Color){0, 32, 255, 255}    // Transition 1
#define GDMF_COLOR150  (Color){0, 0, 255, 255}     // Blue
#define GDMF_COLOR151  (Color){16, 0, 240, 255}    // Transition 1
#define GDMF_COLOR152  (Color){32, 0, 224, 255}    // Transition 2
#define GDMF_COLOR153  (Color){48, 0, 208, 255}    // Transition 3
#define GDMF_COLOR154  (Color){64, 0, 192, 255}    // Blue-Violet
#define GDMF_COLOR155  (Color){80, 0, 176, 255}    // Transition 1
#define GDMF_COLOR156  (Color){96, 0, 160, 255}    // Transition 2
#define GDMF_COLOR157  (Color){112, 0, 144, 255}   // Transition 3
#define GDMF_COLOR158  (Color){128, 0, 128, 255}   // Violet
#define GDMF_COLOR159  (Color){144, 0, 112, 255}   // Transition 1
#define GDMF_COLOR160  (Color){160, 0, 96, 255}    // Transition 2
#define GDMF_COLOR161  (Color){176, 0, 80, 255}    // Transition 3
#define GDMF_COLOR162  (Color){192, 0, 64, 255}    // Magenta
#define GDMF_COLOR163  (Color){208, 0, 48, 255}    // Transition 1
#define GDMF_COLOR164  (Color){224, 0, 32, 255}    // Transition 2
#define GDMF_COLOR165  (Color){240, 0, 16, 255}    // Transition 3
#define GDMF_COLOR166  (Color){255, 0, 0, 255}     // Pure Red
#define GDMF_COLOR167  (Color){255, 16, 0, 255}    // Transition 1
#define GDMF_COLOR168  (Color){255, 32, 0, 255}    // Transition 2
#define GDMF_COLOR169  (Color){255, 48, 0, 255}    // Transition 3
#define GDMF_COLOR170  (Color){255, 64, 0, 255}    // Back to Red-Orange

// Pastels
#define GDMF_COLOR171  (Color){255, 192, 192, 255}   // Pastel Red
#define GDMF_COLOR172  (Color){255, 224, 192, 255}   // Pastel Red-Orange
#define GDMF_COLOR173  (Color){255, 240, 192, 255}   // Pastel Orange
#define GDMF_COLOR174  (Color){255, 255, 192, 255}   // Pastel Yellow
#define GDMF_COLOR175  (Color){240, 255, 192, 255}   // Pastel Yellow-Green
#define GDMF_COLOR176  (Color){224, 255, 192, 255}   // Pastel Lime
#define GDMF_COLOR177  (Color){192, 255, 192, 255}   // Pastel Green
#define GDMF_COLOR178  (Color){192, 255, 224, 255}   // Pastel Green-Teal
#define GDMF_COLOR179  (Color){192, 255, 240, 255}   // Pastel Teal
#define GDMF_COLOR180  (Color){192, 255, 255, 255}   // Pastel Cyan
#define GDMF_COLOR181  (Color){192, 240, 255, 255}   // Pastel Sky Blue
#define GDMF_COLOR182  (Color){192, 224, 255, 255}   // Pastel Light Blue
#define GDMF_COLOR183  (Color){192, 192, 255, 255}   // Pastel Blue
#define GDMF_COLOR184  (Color){224, 192, 255, 255}   // Pastel Blue-Violet
#define GDMF_COLOR185  (Color){240, 192, 255, 255}   // Pastel Violet
#define GDMF_COLOR186  (Color){255, 192, 255, 255}   // Pastel Magenta
#define GDMF_COLOR187  (Color){255, 192, 240, 255}   // Pastel Pink
#define GDMF_COLOR188  (Color){255, 192, 224, 255}   // Pastel Rose
#define GDMF_COLOR189  (Color){255, 192, 208, 255}   // Pastel Salmon
#define GDMF_COLOR190  (Color){255, 192, 192, 255}   // Back to Pastel Red

// Muted
#define GDMF_COLOR191  (Color){128, 64, 64, 255}     // Muted Red
#define GDMF_COLOR192  (Color){128, 96, 64, 255}     // Muted Red-Orange
#define GDMF_COLOR193  (Color){128, 112, 64, 255}    // Muted Orange
#define GDMF_COLOR194  (Color){128, 128, 64, 255}    // Muted Yellow
#define GDMF_COLOR195  (Color){112, 128, 64, 255}    // Muted Yellow-Green
#define GDMF_COLOR196  (Color){96, 128, 64, 255}     // Muted Lime
#define GDMF_COLOR197  (Color){64, 128, 64, 255}     // Muted Green
#define GDMF_COLOR198  (Color){64, 128, 96, 255}     // Muted Green-Teal
#define GDMF_COLOR199  (Color){64, 128, 112, 255}    // Muted Teal
#define GDMF_COLOR200  (Color){64, 128, 128, 255}    // Muted Cyan
#define GDMF_COLOR201  (Color){64, 112, 128, 255}    // Muted Blue-Cyan
#define GDMF_COLOR202  (Color){64, 96, 128, 255}     // Muted Light Blue
#define GDMF_COLOR203  (Color){64, 64, 128, 255}     // Muted Blue
#define GDMF_COLOR204  (Color){96, 64, 128, 255}     // Muted Blue-Violet
#define GDMF_COLOR205  (Color){112, 64, 128, 255}    // Muted Violet
#define GDMF_COLOR206  (Color){128, 64, 128, 255}    // Muted Magenta
#define GDMF_COLOR207  (Color){128, 64, 112, 255}    // Muted Rose
#define GDMF_COLOR208  (Color){128, 64, 96, 255}     // Muted Salmon
#define GDMF_COLOR209  (Color){128, 64, 80, 255}     // Muted Pink
#define GDMF_COLOR210  (Color){128, 64, 64, 255}     // Back to Muted Red


// Commodore 64
#define GDMF_COLOR211  (Color){0, 0, 0, 255}          // Black
#define GDMF_COLOR212  (Color){255, 255, 255, 255}   // White
#define GDMF_COLOR213  (Color){136, 0, 0, 255}       // Red
#define GDMF_COLOR214  (Color){170, 255, 238, 255}   // Cyan
#define GDMF_COLOR215  (Color){204, 68, 204, 255}    // Purple
#define GDMF_COLOR216  (Color){0, 204, 85, 255}      // Green
#define GDMF_COLOR217  (Color){0, 0, 170, 255}       // Blue
#define GDMF_COLOR218  (Color){238, 238, 119, 255}   // Yellow
#define GDMF_COLOR219  (Color){221, 136, 85, 255}    // Orange
#define GDMF_COLOR220  (Color){102, 68, 0, 255}      // Brown
#define GDMF_COLOR221  (Color){255, 119, 119, 255}   // Light Red
#define GDMF_COLOR222  (Color){51, 51, 51, 255}      // Dark Grey
#define GDMF_COLOR223  (Color){119, 119, 119, 255}   // Grey
#define GDMF_COLOR224  (Color){170, 255, 102, 255}   // Light Green
#define GDMF_COLOR225  (Color){119, 221, 255, 255}   // Light Blue
#define GDMF_COLOR226  (Color){187, 187, 187, 255}   // Light Grey

// Retro Coloriffic
#define GDMF_COLOR227  (Color){188, 62, 62, 255}     // Deep Rust Red
#define GDMF_COLOR228  (Color){255, 102, 0, 255}     // Bright Pumpkin Orange
#define GDMF_COLOR229  (Color){255, 182, 74, 255}    // Warm Peach
#define GDMF_COLOR230  (Color){255, 227, 77, 255}    // Bright Mustard Yellow
#define GDMF_COLOR231  (Color){202, 255, 77, 255}    // Neon Lime
#define GDMF_COLOR232  (Color){107, 255, 77, 255}    // Light Grass Green
#define GDMF_COLOR233  (Color){0, 155, 72, 255}      // Deep Forest Green
#define GDMF_COLOR234  (Color){77, 255, 202, 255}    // Aqua Turquoise
#define GDMF_COLOR235  (Color){0, 155, 188, 255}     // Deep Cyan Blue
#define GDMF_COLOR236  (Color){77, 182, 255, 255}    // Sky Blue
#define GDMF_COLOR237  (Color){0, 72, 255, 255}      // Electric Blue
#define GDMF_COLOR238  (Color){72, 0, 255, 255}      // Deep Purple
#define GDMF_COLOR239  (Color){155, 77, 255, 255}    // Lavender Violet
#define GDMF_COLOR240  (Color){255, 77, 255, 255}    // Hot Pink Magenta
#define GDMF_COLOR241  (Color){255, 77, 182, 255}    // Neon Rose
#define GDMF_COLOR242  (Color){155, 77, 107, 255}    // Dusty Mauve
#define GDMF_COLOR243  (Color){107, 77, 62, 255}     // Coffee Brown
#define GDMF_COLOR244  (Color){255, 148, 89, 255}    // Soft Tangerine
#define GDMF_COLOR245  (Color){188, 107, 89, 255}    // Clay Red
#define GDMF_COLOR246  (Color){107, 155, 89, 255}    // Olive Green
#define GDMF_COLOR247  (Color){62, 107, 155, 255}    // Slate Blue
#define GDMF_COLOR248  (Color){89, 62, 155, 255}     // Dark Indigo
#define GDMF_COLOR249  (Color){155, 62, 188, 255}    // Grape Purple
#define GDMF_COLOR250  (Color){89, 0, 188, 255}      // Royal Violet
#define GDMF_COLOR251  (Color){89, 107, 188, 255}    // Periwinkle
#define GDMF_COLOR252  (Color){89, 188, 255, 255}    // Pale Sky Blue
#define GDMF_COLOR253  (Color){188, 255, 155, 255}   // Mint Green
#define GDMF_COLOR254  (Color){255, 202, 155, 255}   // Creamy Peach
#define GDMF_COLOR255  (Color){202, 202, 202, 255}   // Light Ash Grey

// Standard ANSI Colors
#define ANSI_BLACK       GDMF_COLOR0    // Black
#define ANSI_RED         GDMF_COLOR22   // Red
#define ANSI_GREEN       GDMF_COLOR42   // Green
#define ANSI_YELLOW      GDMF_COLOR74   // Yellow
#define ANSI_BLUE        GDMF_COLOR58   // Blue
#define ANSI_MAGENTA     GDMF_COLOR90   // Magenta
#define ANSI_CYAN        GDMF_COLOR108  // Cyan
#define ANSI_WHITE       GDMF_COLOR10   // White

// Bright ANSI Colors
#define ANSI_BRIGHT_BLACK  GDMF_COLOR5    // Bright Black (Gray)
#define ANSI_BRIGHT_RED    GDMF_COLOR234  // Bright Red
#define ANSI_BRIGHT_GREEN  GDMF_COLOR175  // Bright Green
#define ANSI_BRIGHT_YELLOW GDMF_COLOR230  // Bright Yellow
#define ANSI_BRIGHT_BLUE   GDMF_COLOR237  // Bright Blue
#define ANSI_BRIGHT_MAGENTA GDMF_COLOR240 // Bright Magenta
#define ANSI_BRIGHT_CYAN   GDMF_COLOR252  // Bright Cyan
#define ANSI_BRIGHT_WHITE  GDMF_COLOR15   // Bright White

// Standard Tandy Colors
#define TANDY_BLACK       GDMF_COLOR0    // Black
#define TANDY_RED         GDMF_COLOR22   // Red
#define TANDY_GREEN       GDMF_COLOR42   // Green
#define TANDY_YELLOW      GDMF_COLOR74   // Yellow
#define TANDY_BLUE        GDMF_COLOR58   // Blue
#define TANDY_MAGENTA     GDMF_COLOR90   // Magenta
#define TANDY_CYAN        GDMF_COLOR108  // Cyan
#define TANDY_WHITE       GDMF_COLOR10   // White

// Bright Tandy Colors
#define TANDY_BRIGHT_BLACK  GDMF_COLOR5    // Bright Black (Gray)
#define TANDY_BRIGHT_RED    GDMF_COLOR234  // Bright Red
#define TANDY_BRIGHT_GREEN  GDMF_COLOR175  // Bright Green
#define TANDY_BRIGHT_YELLOW GDMF_COLOR230  // Bright Yellow
#define TANDY_BRIGHT_BLUE   GDMF_COLOR237  // Bright Blue
#define TANDY_BRIGHT_MAGENTA GDMF_COLOR240 // Bright Magenta
#define TANDY_BRIGHT_CYAN   GDMF_COLOR252  // Bright Cyan
#define TANDY_BRIGHT_WHITE  GDMF_COLOR15   // Bright White

// Nintendo Entertainment System Colors
// Row 1: Darkest Colors
#define FUSELAGE_NES_COLOR00  GDMF_COLOR5    // Dark Gray (closest to 84, 84, 84)
#define FUSELAGE_NES_COLOR01  GDMF_COLOR48   // Dark Blue (closest to 0, 30, 116)
#define FUSELAGE_NES_COLOR02  GDMF_COLOR49   // Blue (closest to 8, 16, 144)
#define FUSELAGE_NES_COLOR03  GDMF_COLOR50   // Purple (closest to 48, 0, 136)
#define FUSELAGE_NES_COLOR04  GDMF_COLOR84   // Deep Purple (closest to 68, 0, 100)
#define FUSELAGE_NES_COLOR05  GDMF_COLOR96   // Dark Red (closest to 92, 0, 48)
#define FUSELAGE_NES_COLOR06  GDMF_COLOR22   // Red (closest to 84, 4, 0)
#define FUSELAGE_NES_COLOR07  GDMF_COLOR233  // Brown (closest to 60, 24, 0)
#define FUSELAGE_NES_COLOR08  GDMF_COLOR64   // Olive (closest to 32, 42, 0)
#define FUSELAGE_NES_COLOR09  GDMF_COLOR42   // Dark Green (closest to 8, 58, 0)
#define FUSELAGE_NES_COLOR0A  GDMF_COLOR47   // Green (closest to 0, 64, 0)
#define FUSELAGE_NES_COLOR0B  GDMF_COLOR45   // Teal (closest to 0, 60, 0)
#define FUSELAGE_NES_COLOR0C  GDMF_COLOR96   // Cyan (closest to 0, 50, 60)
#define FUSELAGE_NES_COLOR0D  GDMF_COLOR0    // Black
#define FUSELAGE_NES_COLOR0E  GDMF_COLOR0    // Black (duplicate)
#define FUSELAGE_NES_COLOR0F  GDMF_COLOR0    // Black (duplicate)

// Row 2: Low Brightness
#define FUSELAGE_NES_COLOR10  GDMF_COLOR10   // Light Gray (closest to 152, 150, 152)
#define FUSELAGE_NES_COLOR11  GDMF_COLOR58   // Light Blue (closest to 8, 76, 196)
#define FUSELAGE_NES_COLOR12  GDMF_COLOR63   // Blue (closest to 48, 50, 236)
#define FUSELAGE_NES_COLOR13  GDMF_COLOR94   // Purple (closest to 92, 30, 228)
#define FUSELAGE_NES_COLOR14  GDMF_COLOR90   // Magenta (closest to 136, 20, 176)
#define FUSELAGE_NES_COLOR15  GDMF_COLOR240  // Red-Magenta (closest to 160, 20, 100)
#define FUSELAGE_NES_COLOR16  GDMF_COLOR22   // Red (closest to 152, 34, 32)
#define FUSELAGE_NES_COLOR17  GDMF_COLOR234  // Brown (closest to 120, 60, 0)
#define FUSELAGE_NES_COLOR18  GDMF_COLOR175  // Olive (closest to 84, 90, 0)
#define FUSELAGE_NES_COLOR19  GDMF_COLOR42   // Green (closest to 40, 114, 0)
#define FUSELAGE_NES_COLOR1A  GDMF_COLOR47   // Bright Green (closest to 8, 124, 0)
#define FUSELAGE_NES_COLOR1B  GDMF_COLOR133  // Teal (closest to 0, 118, 40)
#define FUSELAGE_NES_COLOR1C  GDMF_COLOR108  // Cyan (closest to 0, 102, 120)
#define FUSELAGE_NES_COLOR1D  GDMF_COLOR0    // Black
#define FUSELAGE_NES_COLOR1E  GDMF_COLOR0    // Black (duplicate)
#define FUSELAGE_NES_COLOR1F  GDMF_COLOR0    // Black (duplicate)

// Row 3: Medium Brightness
#define FUSELAGE_NES_COLOR20  GDMF_COLOR10   // Light Gray (closest to 204, 204, 204)
#define FUSELAGE_NES_COLOR21  GDMF_COLOR237  // Light Blue (closest to 76, 150, 236)
#define FUSELAGE_NES_COLOR22  GDMF_COLOR236  // Sky Blue (closest to 92, 136, 255)
#define FUSELAGE_NES_COLOR23  GDMF_COLOR94   // Purple (closest to 128, 120, 255)
#define FUSELAGE_NES_COLOR24  GDMF_COLOR240  // Magenta (closest to 172, 104, 236)
#define FUSELAGE_NES_COLOR25  GDMF_COLOR95   // Bright Red-Magenta (closest to 204, 96, 192)
#define FUSELAGE_NES_COLOR26  GDMF_COLOR234  // Orange-Red (closest to 204, 112, 100)
#define FUSELAGE_NES_COLOR27  GDMF_COLOR245  // Soft Orange (closest to 184, 136, 0)
#define FUSELAGE_NES_COLOR28  GDMF_COLOR230  // Yellow (closest to 160, 170, 0)
#define FUSELAGE_NES_COLOR29  GDMF_COLOR175  // Green-Yellow (closest to 116, 196, 0)
#define FUSELAGE_NES_COLOR2A  GDMF_COLOR128  // Bright Green (closest to 76, 208, 32)
#define FUSELAGE_NES_COLOR2B  GDMF_COLOR133  // Aqua (closest to 56, 204, 108)
#define FUSELAGE_NES_COLOR2C  GDMF_COLOR252  // Cyan (closest to 56, 180, 204)
#define FUSELAGE_NES_COLOR2D  GDMF_COLOR0    // Black
#define FUSELAGE_NES_COLOR2E  GDMF_COLOR0    // Black (duplicate)
#define FUSELAGE_NES_COLOR2F  GDMF_COLOR0    // Black (duplicate)

// Row 4: Brightest Colors
#define FUSELAGE_NES_COLOR30  GDMF_COLOR15   // Bright White (closest to 255, 255, 255)
#define FUSELAGE_NES_COLOR31  GDMF_COLOR240  // Bright Pink (closest to 204, 204, 255)
#define FUSELAGE_NES_COLOR32  GDMF_COLOR241  // Bright Cyan (closest to 132, 214, 255)
#define FUSELAGE_NES_COLOR33  GDMF_COLOR252  // Sky Blue (closest to 180, 210, 255)
#define FUSELAGE_NES_COLOR34  GDMF_COLOR95   // Purple (closest to 228, 180, 255)
#define FUSELAGE_NES_COLOR35  GDMF_COLOR240  // Magenta (closest to 252, 180, 228)
#define FUSELAGE_NES_COLOR36  GDMF_COLOR95   // Pink (closest to 255, 180, 192)
#define FUSELAGE_NES_COLOR37  GDMF_COLOR254  // Creamy Peach (closest to 255, 208, 112)
#define FUSELAGE_NES_COLOR38  GDMF_COLOR230  // Yellow (closest to 240, 220, 0)
#define FUSELAGE_NES_COLOR39  GDMF_COLOR253  // Mint Green (closest to 196, 248, 0)
#define FUSELAGE_NES_COLOR3A  GDMF_COLOR175  // Bright Green (closest to 136, 255, 0)
#define FUSELAGE_NES_COLOR3B  GDMF_COLOR133  // Aqua (closest to 116, 255, 152)
#define FUSELAGE_NES_COLOR3C  GDMF_COLOR252  // Cyan (closest to 116, 236, 252)
#define FUSELAGE_NES_COLOR3D  GDMF_COLOR0    // Black
#define FUSELAGE_NES_COLOR3E  GDMF_COLOR0    // Black (duplicate)
#define FUSELAGE_NES_COLOR3F  GDMF_COLOR15   // White (closest to 255, 255, 255)

// Row 1: Darkest Colors
#define TRUE_NES_COLOR00  (Color){84, 84, 84, 255}       // Dark Gray
#define TRUE_NES_COLOR01  (Color){0, 30, 116, 255}       // Dark Blue
#define TRUE_NES_COLOR02  (Color){8, 16, 144, 255}       // Blue
#define TRUE_NES_COLOR03  (Color){48, 0, 136, 255}       // Purple
#define TRUE_NES_COLOR04  (Color){68, 0, 100, 255}       // Deep Purple
#define TRUE_NES_COLOR05  (Color){92, 0, 48, 255}        // Deep Red
#define TRUE_NES_COLOR06  (Color){84, 4, 0, 255}         // Red
#define TRUE_NES_COLOR07  (Color){60, 24, 0, 255}        // Brown
#define TRUE_NES_COLOR08  (Color){32, 42, 0, 255}        // Olive
#define TRUE_NES_COLOR09  (Color){8, 58, 0, 255}         // Green
#define TRUE_NES_COLOR0A  (Color){0, 64, 0, 255}         // Dark Green
#define TRUE_NES_COLOR0B  (Color){0, 60, 0, 255}         // Teal
#define TRUE_NES_COLOR0C  (Color){0, 50, 60, 255}        // Cyan
#define TRUE_NES_COLOR0D  (Color){0, 0, 0, 255}          // Black
#define TRUE_NES_COLOR0E  (Color){0, 0, 0, 255}          // Black (duplicate)
#define TRUE_NES_COLOR0F  (Color){0, 0, 0, 255}          // Black (duplicate)

// Row 2: Low Brightness
#define TRUE_NES_COLOR10  (Color){152, 150, 152, 255}    // Light Gray
#define TRUE_NES_COLOR11  (Color){8, 76, 196, 255}       // Blue
#define TRUE_NES_COLOR12  (Color){48, 50, 236, 255}      // Light Blue
#define TRUE_NES_COLOR13  (Color){92, 30, 228, 255}      // Purple
#define TRUE_NES_COLOR14  (Color){136, 20, 176, 255}     // Magenta
#define TRUE_NES_COLOR15  (Color){160, 20, 100, 255}     // Red-Magenta
#define TRUE_NES_COLOR16  (Color){152, 34, 32, 255}      // Red
#define TRUE_NES_COLOR17  (Color){120, 60, 0, 255}       // Brown
#define TRUE_NES_COLOR18  (Color){84, 90, 0, 255}        // Olive
#define TRUE_NES_COLOR19  (Color){40, 114, 0, 255}       // Green
#define TRUE_NES_COLOR1A  (Color){8, 124, 0, 255}        // Bright Green
#define TRUE_NES_COLOR1B  (Color){0, 118, 40, 255}       // Teal
#define TRUE_NES_COLOR1C  (Color){0, 102, 120, 255}      // Cyan
#define TRUE_NES_COLOR1D  (Color){0, 0, 0, 255}          // Black
#define TRUE_NES_COLOR1E  (Color){0, 0, 0, 255}          // Black (duplicate)
#define TRUE_NES_COLOR1F  (Color){0, 0, 0, 255}          // Black (duplicate)

// Row 3: Medium Brightness
#define TRUE_NES_COLOR20  (Color){204, 204, 204, 255}    // Light Gray
#define TRUE_NES_COLOR21  (Color){76, 150, 236, 255}     // Blue
#define TRUE_NES_COLOR22  (Color){92, 136, 255, 255}     // Sky Blue
#define TRUE_NES_COLOR23  (Color){128, 120, 255, 255}    // Purple
#define TRUE_NES_COLOR24  (Color){172, 104, 236, 255}    // Magenta
#define TRUE_NES_COLOR25  (Color){204, 96, 192, 255}     // Bright Magenta
#define TRUE_NES_COLOR26  (Color){204, 112, 100, 255}    // Red-Orange
#define TRUE_NES_COLOR27  (Color){184, 136, 0, 255}      // Orange
#define TRUE_NES_COLOR28  (Color){160, 170, 0, 255}      // Yellow
#define TRUE_NES_COLOR29  (Color){116, 196, 0, 255}      // Lime Green
#define TRUE_NES_COLOR2A  (Color){76, 208, 32, 255}      // Bright Green
#define TRUE_NES_COLOR2B  (Color){56, 204, 108, 255}     // Teal
#define TRUE_NES_COLOR2C  (Color){56, 180, 204, 255}     // Cyan
#define TRUE_NES_COLOR2D  (Color){0, 0, 0, 255}          // Black
#define TRUE_NES_COLOR2E  (Color){0, 0, 0, 255}          // Black (duplicate)
#define TRUE_NES_COLOR2F  (Color){0, 0, 0, 255}          // Black (duplicate)

// Row 4: Brightest Colors
#define TRUE_NES_COLOR30  (Color){255, 255, 255, 255}    // White
#define TRUE_NES_COLOR31  (Color){204, 204, 255, 255}    // Bright Purple
#define TRUE_NES_COLOR32  (Color){132, 214, 255, 255}    // Bright Cyan
#define TRUE_NES_COLOR33  (Color){180, 210, 255, 255}    // Sky Blue
#define TRUE_NES_COLOR34  (Color){228, 180, 255, 255}    // Purple
#define TRUE_NES_COLOR35  (Color){252, 180, 228, 255}    // Pink
#define TRUE_NES_COLOR36  (Color){255, 180, 192, 255}    // Salmon
#define TRUE_NES_COLOR37  (Color){255, 208, 112, 255}    // Creamy Peach
#define TRUE_NES_COLOR38  (Color){240, 220, 0, 255}      // Yellow
#define TRUE_NES_COLOR39  (Color){196, 248, 0, 255}      // Bright Lime Green
#define TRUE_NES_COLOR3A  (Color){136, 255, 0, 255}      // Bright Green
#define TRUE_NES_COLOR3B  (Color){116, 255, 152, 255}    // Mint
#define TRUE_NES_COLOR3C  (Color){116, 236, 252, 255}    // Cyan
#define TRUE_NES_COLOR3D  (Color){0, 0, 0, 255}          // Black
#define TRUE_NES_COLOR3E  (Color){0, 0, 0, 255}          // Black (duplicate)
#define TRUE_NES_COLOR3F  (Color){255, 255, 255, 255}    // White

// Zed Ex Speccy colors
#define ZX_COLOR00  GDMF_COLOR0    // Black
#define ZX_COLOR01  GDMF_COLOR48   // Blue
#define ZX_COLOR02  GDMF_COLOR22   // Red
#define ZX_COLOR03  GDMF_COLOR90   // Magenta
#define ZX_COLOR04  GDMF_COLOR42   // Green
#define ZX_COLOR05  GDMF_COLOR108  // Cyan
#define ZX_COLOR06  GDMF_COLOR74   // Yellow
#define ZX_COLOR07  GDMF_COLOR10   // White

#define ZX_COLOR08  GDMF_COLOR0    // Bright Black (still black)
#define ZX_COLOR09  GDMF_COLOR63   // Bright Blue
#define ZX_COLOR10  GDMF_COLOR31   // Bright Red
#define ZX_COLOR11  GDMF_COLOR240  // Bright Magenta
#define ZX_COLOR12  GDMF_COLOR47   // Bright Green
#define ZX_COLOR13  GDMF_COLOR111  // Bright Cyan
#define ZX_COLOR14  GDMF_COLOR79   // Bright Yellow
#define ZX_COLOR15  GDMF_COLOR15   // Bright White

// Commodore 64 colors
#define C64_COLOR00  GDMF_COLOR0    // Black
#define C64_COLOR01  GDMF_COLOR15   // White
#define C64_COLOR02  GDMF_COLOR22   // Red
#define C64_COLOR03  GDMF_COLOR227  // Cyan
#define C64_COLOR04  GDMF_COLOR90   // Purple
#define C64_COLOR05  GDMF_COLOR42   // Green
#define C64_COLOR06  GDMF_COLOR58   // Blue
#define C64_COLOR07  GDMF_COLOR74   // Yellow
#define C64_COLOR08  GDMF_COLOR116  // Orange
#define C64_COLOR09  GDMF_COLOR233  // Brown
#define C64_COLOR10  GDMF_COLOR234  // Light Red
#define C64_COLOR11  GDMF_COLOR5    // Dark Grey
#define C64_COLOR12  GDMF_COLOR10   // Grey
#define C64_COLOR13  GDMF_COLOR175  // Light Green
#define C64_COLOR14  GDMF_COLOR252  // Light Blue
#define C64_COLOR15  GDMF_COLOR13   // Light Grey


bool LoadPaletteFromSprite(unsigned char paletteeIndex, Color* spritepalette);
Color GetColorFromChar(unsigned char input);
Color GetCommodoreColor(unsigned char input);
Color GetTandyColor(unsigned char input);
Color GetANSIColor(unsigned char input);

extern Color Colors[256][16];

bool SetPalette(unsigned char paletteIndex, unsigned char colorIndex, Color color);
Color GetPalette(unsigned char paletteIndex, unsigned char colorIndex);

void InitPalettes(void);

#endif // COLORS_H