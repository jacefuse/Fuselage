#include <stdio.h>
#include <raylib.h>

#ifndef COLORS_H
#define COLORS_H

#ifdef __cplusplus
extern "C" {
#endif

#define FUSELAGE_PALETTE_SIZE 16

// Define the 256 colors as macros

// Grayscale
#define COLOR0  (Color){0, 0, 0, 255} // BLACK
#define COLOR1  (Color){17, 17, 17, 255}
#define COLOR2  (Color){34, 34, 34, 255}
#define COLOR3  (Color){51, 51, 51, 255}
#define COLOR4  (Color){68, 68, 68, 255}
#define COLOR5  (Color){85, 85, 85, 255}
#define COLOR6  (Color){102, 102, 102, 255}
#define COLOR7  (Color){119, 119, 119, 255}
#define COLOR8  (Color){136, 136, 136, 255}
#define COLOR9  (Color){153, 153, 153, 255}
#define COLOR10 (Color){170, 170, 170, 255}
#define COLOR11 (Color){187, 187, 187, 255}
#define COLOR12 (Color){204, 204, 204, 255}
#define COLOR13 (Color){221, 221, 221, 255}
#define COLOR14 (Color){238, 238, 238, 255}
#define COLOR15 (Color){255, 255, 255, 255} // WHITE

#define COLOR16  (Color){16, 0, 0, 255} // DARK RED
#define COLOR17  (Color){32, 0, 0, 255}
#define COLOR18  (Color){48, 0, 0, 255}
#define COLOR19  (Color){64, 0, 0, 255}
#define COLOR20  (Color){80, 0, 0, 255}
#define COLOR21  (Color){96, 0, 0, 255}
#define COLOR22  (Color){112, 0, 0, 255}
#define COLOR23  (Color){128, 0, 0, 255}
#define COLOR24  (Color){144, 0, 0, 255}
#define COLOR25  (Color){160, 0, 0, 255}
#define COLOR26  (Color){176, 0, 0, 255}
#define COLOR27  (Color){192, 0, 0, 255}
#define COLOR28  (Color){208, 0, 0, 255}
#define COLOR29  (Color){224, 0, 0, 255}
#define COLOR30  (Color){240, 0, 0, 255}
#define COLOR31  (Color){255, 0, 0, 255} // PURE RED

#define COLOR32  (Color){0, 16, 0, 255} // DARK GREEN
#define COLOR33  (Color){0, 32, 0, 255}
#define COLOR34  (Color){0, 48, 0, 255}
#define COLOR35  (Color){0, 64, 0, 255}
#define COLOR36  (Color){0, 80, 0, 255}
#define COLOR37  (Color){0, 96, 0, 255}
#define COLOR38  (Color){0, 112, 0, 255}
#define COLOR39  (Color){0, 128, 0, 255}
#define COLOR40  (Color){0, 144, 0, 255}
#define COLOR41  (Color){0, 160, 0, 255}
#define COLOR42  (Color){0, 176, 0, 255}
#define COLOR43  (Color){0, 192, 0, 255}
#define COLOR44  (Color){0, 208, 0, 255}
#define COLOR45  (Color){0, 224, 0, 255}
#define COLOR46  (Color){0, 240, 0, 255}
#define COLOR47  (Color){0, 255, 0, 255} // PURE GREEN

#define COLOR48  (Color){0, 0, 16, 255} // DARK BLUE
#define COLOR49  (Color){0, 0, 32, 255}
#define COLOR50  (Color){0, 0, 48, 255}
#define COLOR51  (Color){0, 0, 64, 255}
#define COLOR52  (Color){0, 0, 80, 255}
#define COLOR53  (Color){0, 0, 96, 255}
#define COLOR54  (Color){0, 0, 112, 255}
#define COLOR55  (Color){0, 0, 128, 255}
#define COLOR56  (Color){0, 0, 144, 255}
#define COLOR57  (Color){0, 0, 160, 255}
#define COLOR58  (Color){0, 0, 176, 255}
#define COLOR59  (Color){0, 0, 192, 255}
#define COLOR60  (Color){0, 0, 208, 255}
#define COLOR61  (Color){0, 0, 224, 255}
#define COLOR62  (Color){0, 0, 240, 255}
#define COLOR63  (Color){0, 0, 255, 255} // PURE BLUE

#define COLOR64  (Color){16, 16, 0, 255} // BROWN
#define COLOR65  (Color){32, 32, 0, 255}
#define COLOR66  (Color){48, 48, 0, 255}
#define COLOR67  (Color){64, 64, 0, 255}
#define COLOR68  (Color){80, 80, 0, 255}
#define COLOR69  (Color){96, 96, 0, 255}
#define COLOR70  (Color){112, 112, 0, 255}
#define COLOR71  (Color){128, 128, 0, 255}
#define COLOR72  (Color){144, 144, 0, 255}
#define COLOR73  (Color){160, 160, 0, 255}
#define COLOR74  (Color){176, 176, 0, 255}
#define COLOR75  (Color){192, 192, 0, 255}
#define COLOR76  (Color){208, 208, 0, 255}
#define COLOR77  (Color){224, 224, 0, 255}
#define COLOR78  (Color){240, 240, 0, 255}
#define COLOR79  (Color){255, 255, 0, 255} // YELLOW

#define COLOR80  (Color){16, 0, 16, 255} // DARK PURPLE
#define COLOR81  (Color){32, 0, 32, 255}
#define COLOR82  (Color){48, 0, 48, 255}
#define COLOR83  (Color){64, 0, 64, 255}
#define COLOR84  (Color){80, 0, 80, 255}
#define COLOR85  (Color){96, 0, 96, 255}
#define COLOR86  (Color){112, 0, 112, 255}
#define COLOR87  (Color){128, 0, 128, 255}
#define COLOR88  (Color){144, 0, 144, 255}
#define COLOR89  (Color){160, 0, 160, 255}
#define COLOR90  (Color){176, 0, 176, 255}
#define COLOR91  (Color){192, 0, 192, 255}
#define COLOR92  (Color){208, 0, 208, 255}
#define COLOR93  (Color){224, 0, 224, 255}
#define COLOR94  (Color){240, 0, 240, 255}
#define COLOR95  (Color){255, 0, 255, 255} // MAGENTA

#define COLOR96   (Color){0, 16, 16, 255} // TEAL
#define COLOR97   (Color){0, 32, 32, 255}
#define COLOR98   (Color){0, 48, 48, 255}
#define COLOR99   (Color){0, 64, 64, 255}
#define COLOR100  (Color){0, 80, 80, 255}
#define COLOR101  (Color){0, 96, 96, 255}
#define COLOR102  (Color){0, 112, 112, 255}
#define COLOR103  (Color){0, 128, 128, 255}
#define COLOR104  (Color){0, 144, 144, 255}
#define COLOR105  (Color){0, 160, 160, 255}
#define COLOR106  (Color){0, 176, 176, 255}
#define COLOR107  (Color){0, 192, 192, 255}
#define COLOR108  (Color){0, 208, 208, 255}
#define COLOR109  (Color){0, 224, 224, 255}
#define COLOR110  (Color){0, 240, 240, 255}
#define COLOR111  (Color){0, 255, 255, 255} // CYAN

// Transitionals
#define COLOR112  (Color){255, 64, 0, 255}    // Red-Orange
#define COLOR113  (Color){255, 80, 0, 255}    // Transition 1
#define COLOR114  (Color){255, 96, 0, 255}    // Transition 2
#define COLOR115  (Color){255, 112, 0, 255}   // Transition 3
#define COLOR116  (Color){255, 128, 0, 255}   // Orange
#define COLOR117  (Color){255, 144, 0, 255}   // Transition 1
#define COLOR118  (Color){255, 160, 0, 255}   // Transition 2
#define COLOR119  (Color){255, 176, 0, 255}   // Transition 3
#define COLOR120  (Color){255, 192, 0, 255}   // Yellow-Orange
#define COLOR121  (Color){255, 208, 0, 255}   // Transition 1
#define COLOR122  (Color){255, 224, 0, 255}   // Transition 2
#define COLOR123  (Color){255, 240, 0, 255}   // Transition 3
#define COLOR124  (Color){255, 255, 0, 255}   // Yellow
#define COLOR125  (Color){224, 255, 0, 255}   // Transition 1
#define COLOR126  (Color){192, 255, 0, 255}   // Transition 2
#define COLOR127  (Color){160, 255, 0, 255}   // Transition 3
#define COLOR128  (Color){128, 255, 0, 255}   // Lime Green
#define COLOR129  (Color){96, 255, 0, 255}    // Transition 1
#define COLOR130  (Color){64, 255, 0, 255}    // Green
#define COLOR131  (Color){32, 255, 16, 255}   // Transition 1
#define COLOR132  (Color){0, 255, 64, 255}    // Green-Teal
#define COLOR133  (Color){0, 255, 80, 255}    // Transition 1
#define COLOR134  (Color){0, 255, 96, 255}    // Transition 2
#define COLOR135  (Color){0, 255, 112, 255}   // Transition 3
#define COLOR136  (Color){0, 255, 128, 255}   // Teal
#define COLOR137  (Color){0, 255, 144, 255}   // Transition 1
#define COLOR138  (Color){0, 255, 160, 255}   // Transition 2
#define COLOR139  (Color){0, 255, 176, 255}   // Transition 3
#define COLOR140  (Color){0, 255, 192, 255}   // Aqua
#define COLOR141  (Color){0, 240, 208, 255}   // Transition 1
#define COLOR142  (Color){0, 224, 224, 255}   // Transition 2
#define COLOR143  (Color){0, 208, 240, 255}   // Transition 3
#define COLOR144  (Color){0, 192, 255, 255}   // Blue-Cyan
#define COLOR145  (Color){0, 160, 255, 255}   // Transition 1
#define COLOR146  (Color){0, 128, 255, 255}   // Light Blue
#define COLOR147  (Color){0, 96, 255, 255}    // Transition 1
#define COLOR148  (Color){0, 64, 255, 255}    // Bright Blue
#define COLOR149  (Color){0, 32, 255, 255}    // Transition 1
#define COLOR150  (Color){0, 0, 255, 255}     // Blue
#define COLOR151  (Color){16, 0, 240, 255}    // Transition 1
#define COLOR152  (Color){32, 0, 224, 255}    // Transition 2
#define COLOR153  (Color){48, 0, 208, 255}    // Transition 3
#define COLOR154  (Color){64, 0, 192, 255}    // Blue-Violet
#define COLOR155  (Color){80, 0, 176, 255}    // Transition 1
#define COLOR156  (Color){96, 0, 160, 255}    // Transition 2
#define COLOR157  (Color){112, 0, 144, 255}   // Transition 3
#define COLOR158  (Color){128, 0, 128, 255}   // Violet
#define COLOR159  (Color){144, 0, 112, 255}   // Transition 1
#define COLOR160  (Color){160, 0, 96, 255}    // Transition 2
#define COLOR161  (Color){176, 0, 80, 255}    // Transition 3
#define COLOR162  (Color){192, 0, 64, 255}    // Magenta
#define COLOR163  (Color){208, 0, 48, 255}    // Transition 1
#define COLOR164  (Color){224, 0, 32, 255}    // Transition 2
#define COLOR165  (Color){240, 0, 16, 255}    // Transition 3
#define COLOR166  (Color){255, 0, 0, 255}     // Pure Red
#define COLOR167  (Color){255, 16, 0, 255}    // Transition 1
#define COLOR168  (Color){255, 32, 0, 255}    // Transition 2
#define COLOR169  (Color){255, 48, 0, 255}    // Transition 3
#define COLOR170  (Color){255, 64, 0, 255}    // Back to Red-Orange

// Pastels
#define COLOR171  (Color){255, 192, 192, 255}   // Pastel Red
#define COLOR172  (Color){255, 224, 192, 255}   // Pastel Red-Orange
#define COLOR173  (Color){255, 240, 192, 255}   // Pastel Orange
#define COLOR174  (Color){255, 255, 192, 255}   // Pastel Yellow
#define COLOR175  (Color){240, 255, 192, 255}   // Pastel Yellow-Green
#define COLOR176  (Color){224, 255, 192, 255}   // Pastel Lime
#define COLOR177  (Color){192, 255, 192, 255}   // Pastel Green
#define COLOR178  (Color){192, 255, 224, 255}   // Pastel Green-Teal
#define COLOR179  (Color){192, 255, 240, 255}   // Pastel Teal
#define COLOR180  (Color){192, 255, 255, 255}   // Pastel Cyan
#define COLOR181  (Color){192, 240, 255, 255}   // Pastel Sky Blue
#define COLOR182  (Color){192, 224, 255, 255}   // Pastel Light Blue
#define COLOR183  (Color){192, 192, 255, 255}   // Pastel Blue
#define COLOR184  (Color){224, 192, 255, 255}   // Pastel Blue-Violet
#define COLOR185  (Color){240, 192, 255, 255}   // Pastel Violet
#define COLOR186  (Color){255, 192, 255, 255}   // Pastel Magenta
#define COLOR187  (Color){255, 192, 240, 255}   // Pastel Pink
#define COLOR188  (Color){255, 192, 224, 255}   // Pastel Rose
#define COLOR189  (Color){255, 192, 208, 255}   // Pastel Salmon
#define COLOR190  (Color){255, 192, 192, 255}   // Back to Pastel Red

// Muted
#define COLOR191  (Color){128, 64, 64, 255}     // Muted Red
#define COLOR192  (Color){128, 96, 64, 255}     // Muted Red-Orange
#define COLOR193  (Color){128, 112, 64, 255}    // Muted Orange
#define COLOR194  (Color){128, 128, 64, 255}    // Muted Yellow
#define COLOR195  (Color){112, 128, 64, 255}    // Muted Yellow-Green
#define COLOR196  (Color){96, 128, 64, 255}     // Muted Lime
#define COLOR197  (Color){64, 128, 64, 255}     // Muted Green
#define COLOR198  (Color){64, 128, 96, 255}     // Muted Green-Teal
#define COLOR199  (Color){64, 128, 112, 255}    // Muted Teal
#define COLOR200  (Color){64, 128, 128, 255}    // Muted Cyan
#define COLOR201  (Color){64, 112, 128, 255}    // Muted Blue-Cyan
#define COLOR202  (Color){64, 96, 128, 255}     // Muted Light Blue
#define COLOR203  (Color){64, 64, 128, 255}     // Muted Blue
#define COLOR204  (Color){96, 64, 128, 255}     // Muted Blue-Violet
#define COLOR205  (Color){112, 64, 128, 255}    // Muted Violet
#define COLOR206  (Color){128, 64, 128, 255}    // Muted Magenta
#define COLOR207  (Color){128, 64, 112, 255}    // Muted Rose
#define COLOR208  (Color){128, 64, 96, 255}     // Muted Salmon
#define COLOR209  (Color){128, 64, 80, 255}     // Muted Pink
#define COLOR210  (Color){128, 64, 64, 255}     // Back to Muted Red


// Commodore 64
#define COLOR211  (Color){0, 0, 0, 255}          // Black
#define COLOR212  (Color){255, 255, 255, 255}   // White
#define COLOR213  (Color){136, 0, 0, 255}       // Red
#define COLOR214  (Color){170, 255, 238, 255}   // Cyan
#define COLOR215  (Color){204, 68, 204, 255}    // Purple
#define COLOR216  (Color){0, 204, 85, 255}      // Green
#define COLOR217  (Color){0, 0, 170, 255}       // Blue
#define COLOR218  (Color){238, 238, 119, 255}   // Yellow
#define COLOR219  (Color){221, 136, 85, 255}    // Orange
#define COLOR220  (Color){102, 68, 0, 255}      // Brown
#define COLOR221  (Color){255, 119, 119, 255}   // Light Red
#define COLOR222  (Color){51, 51, 51, 255}      // Dark Grey
#define COLOR223  (Color){119, 119, 119, 255}   // Grey
#define COLOR224  (Color){170, 255, 102, 255}   // Light Green
#define COLOR225  (Color){119, 221, 255, 255}   // Light Blue
#define COLOR226  (Color){187, 187, 187, 255}   // Light Grey

// Retro Coloriffic 
#define COLOR227  (Color){188, 62, 62, 255}     // Deep Rust Red
#define COLOR228  (Color){255, 102, 0, 255}     // Bright Pumpkin Orange
#define COLOR229  (Color){255, 182, 74, 255}    // Warm Peach
#define COLOR230  (Color){255, 227, 77, 255}    // Bright Mustard Yellow
#define COLOR231  (Color){202, 255, 77, 255}    // Neon Lime
#define COLOR232  (Color){107, 255, 77, 255}    // Light Grass Green
#define COLOR233  (Color){0, 155, 72, 255}      // Deep Forest Green
#define COLOR234  (Color){77, 255, 202, 255}    // Aqua Turquoise
#define COLOR235  (Color){0, 155, 188, 255}     // Deep Cyan Blue
#define COLOR236  (Color){77, 182, 255, 255}    // Sky Blue
#define COLOR237  (Color){0, 72, 255, 255}      // Electric Blue
#define COLOR238  (Color){72, 0, 255, 255}      // Deep Purple
#define COLOR239  (Color){155, 77, 255, 255}    // Lavender Violet
#define COLOR240  (Color){255, 77, 255, 255}    // Hot Pink Magenta
#define COLOR241  (Color){255, 77, 182, 255}    // Neon Rose
#define COLOR242  (Color){155, 77, 107, 255}    // Dusty Mauve
#define COLOR243  (Color){107, 77, 62, 255}     // Coffee Brown
#define COLOR244  (Color){255, 148, 89, 255}    // Soft Tangerine
#define COLOR245  (Color){188, 107, 89, 255}    // Clay Red
#define COLOR246  (Color){107, 155, 89, 255}    // Olive Green
#define COLOR247  (Color){62, 107, 155, 255}    // Slate Blue
#define COLOR248  (Color){89, 62, 155, 255}     // Dark Indigo
#define COLOR249  (Color){155, 62, 188, 255}    // Grape Purple
#define COLOR250  (Color){89, 0, 188, 255}      // Royal Violet
#define COLOR251  (Color){89, 107, 188, 255}    // Periwinkle
#define COLOR252  (Color){89, 188, 255, 255}    // Pale Sky Blue
#define COLOR253  (Color){188, 255, 155, 255}   // Mint Green
#define COLOR254  (Color){255, 202, 155, 255}   // Creamy Peach
#define COLOR255  (Color){202, 202, 202, 255}   // Light Ash Grey

// Standard ANSI Colors
#define ANSI_BLACK       COLOR0    // Black
#define ANSI_RED         COLOR22   // Red
#define ANSI_GREEN       COLOR42   // Green
#define ANSI_YELLOW      COLOR74   // Yellow
#define ANSI_BLUE        COLOR58   // Blue
#define ANSI_MAGENTA     COLOR90   // Magenta
#define ANSI_CYAN        COLOR108  // Cyan
#define ANSI_WHITE       COLOR10   // White

// Bright ANSI Colors
#define ANSI_BRIGHT_BLACK  COLOR5    // Bright Black (Gray)
#define ANSI_BRIGHT_RED    COLOR234  // Bright Red
#define ANSI_BRIGHT_GREEN  COLOR175  // Bright Green
#define ANSI_BRIGHT_YELLOW COLOR230  // Bright Yellow
#define ANSI_BRIGHT_BLUE   COLOR237  // Bright Blue
#define ANSI_BRIGHT_MAGENTA COLOR240 // Bright Magenta
#define ANSI_BRIGHT_CYAN   COLOR252  // Bright Cyan
#define ANSI_BRIGHT_WHITE  COLOR15   // Bright White

// Standard Tandy Colors
#define TANDY_BLACK       COLOR0    // Black
#define TANDY_RED         COLOR22   // Red
#define TANDY_GREEN       COLOR42   // Green
#define TANDY_YELLOW      COLOR74   // Yellow
#define TANDY_BLUE        COLOR58   // Blue
#define TANDY_MAGENTA     COLOR90   // Magenta
#define TANDY_CYAN        COLOR108  // Cyan
#define TANDY_WHITE       COLOR10   // White

// Bright Tandy Colors
#define TANDY_BRIGHT_BLACK  COLOR5    // Bright Black (Gray)
#define TANDY_BRIGHT_RED    COLOR234  // Bright Red
#define TANDY_BRIGHT_GREEN  COLOR175  // Bright Green
#define TANDY_BRIGHT_YELLOW COLOR230  // Bright Yellow
#define TANDY_BRIGHT_BLUE   COLOR237  // Bright Blue
#define TANDY_BRIGHT_MAGENTA COLOR240 // Bright Magenta
#define TANDY_BRIGHT_CYAN   COLOR252  // Bright Cyan
#define TANDY_BRIGHT_WHITE  COLOR15   // Bright White

// Nintendo Entertainment System Colors
// Row 1: Darkest Colors
#define FUSELAGE_NES_COLOR00  COLOR5    // Dark Gray (closest to 84, 84, 84)
#define FUSELAGE_NES_COLOR01  COLOR48   // Dark Blue (closest to 0, 30, 116)
#define FUSELAGE_NES_COLOR02  COLOR49   // Blue (closest to 8, 16, 144)
#define FUSELAGE_NES_COLOR03  COLOR50   // Purple (closest to 48, 0, 136)
#define FUSELAGE_NES_COLOR04  COLOR84   // Deep Purple (closest to 68, 0, 100)
#define FUSELAGE_NES_COLOR05  COLOR96   // Dark Red (closest to 92, 0, 48)
#define FUSELAGE_NES_COLOR06  COLOR22   // Red (closest to 84, 4, 0)
#define FUSELAGE_NES_COLOR07  COLOR233  // Brown (closest to 60, 24, 0)
#define FUSELAGE_NES_COLOR08  COLOR64   // Olive (closest to 32, 42, 0)
#define FUSELAGE_NES_COLOR09  COLOR42   // Dark Green (closest to 8, 58, 0)
#define FUSELAGE_NES_COLOR0A  COLOR47   // Green (closest to 0, 64, 0)
#define FUSELAGE_NES_COLOR0B  COLOR45   // Teal (closest to 0, 60, 0)
#define FUSELAGE_NES_COLOR0C  COLOR96   // Cyan (closest to 0, 50, 60)
#define FUSELAGE_NES_COLOR0D  COLOR0    // Black
#define FUSELAGE_NES_COLOR0E  COLOR0    // Black (duplicate)
#define FUSELAGE_NES_COLOR0F  COLOR0    // Black (duplicate)

// Row 2: Low Brightness
#define FUSELAGE_NES_COLOR10  COLOR10   // Light Gray (closest to 152, 150, 152)
#define FUSELAGE_NES_COLOR11  COLOR58   // Light Blue (closest to 8, 76, 196)
#define FUSELAGE_NES_COLOR12  COLOR63   // Blue (closest to 48, 50, 236)
#define FUSELAGE_NES_COLOR13  COLOR94   // Purple (closest to 92, 30, 228)
#define FUSELAGE_NES_COLOR14  COLOR90   // Magenta (closest to 136, 20, 176)
#define FUSELAGE_NES_COLOR15  COLOR240  // Red-Magenta (closest to 160, 20, 100)
#define FUSELAGE_NES_COLOR16  COLOR22   // Red (closest to 152, 34, 32)
#define FUSELAGE_NES_COLOR17  COLOR234  // Brown (closest to 120, 60, 0)
#define FUSELAGE_NES_COLOR18  COLOR175  // Olive (closest to 84, 90, 0)
#define FUSELAGE_NES_COLOR19  COLOR42   // Green (closest to 40, 114, 0)
#define FUSELAGE_NES_COLOR1A  COLOR47   // Bright Green (closest to 8, 124, 0)
#define FUSELAGE_NES_COLOR1B  COLOR133  // Teal (closest to 0, 118, 40)
#define FUSELAGE_NES_COLOR1C  COLOR108  // Cyan (closest to 0, 102, 120)
#define FUSELAGE_NES_COLOR1D  COLOR0    // Black
#define FUSELAGE_NES_COLOR1E  COLOR0    // Black (duplicate)
#define FUSELAGE_NES_COLOR1F  COLOR0    // Black (duplicate)

// Row 3: Medium Brightness
#define FUSELAGE_NES_COLOR20  COLOR10   // Light Gray (closest to 204, 204, 204)
#define FUSELAGE_NES_COLOR21  COLOR237  // Light Blue (closest to 76, 150, 236)
#define FUSELAGE_NES_COLOR22  COLOR236  // Sky Blue (closest to 92, 136, 255)
#define FUSELAGE_NES_COLOR23  COLOR94   // Purple (closest to 128, 120, 255)
#define FUSELAGE_NES_COLOR24  COLOR240  // Magenta (closest to 172, 104, 236)
#define FUSELAGE_NES_COLOR25  COLOR95   // Bright Red-Magenta (closest to 204, 96, 192)
#define FUSELAGE_NES_COLOR26  COLOR234  // Orange-Red (closest to 204, 112, 100)
#define FUSELAGE_NES_COLOR27  COLOR245  // Soft Orange (closest to 184, 136, 0)
#define FUSELAGE_NES_COLOR28  COLOR230  // Yellow (closest to 160, 170, 0)
#define FUSELAGE_NES_COLOR29  COLOR175  // Green-Yellow (closest to 116, 196, 0)
#define FUSELAGE_NES_COLOR2A  COLOR128  // Bright Green (closest to 76, 208, 32)
#define FUSELAGE_NES_COLOR2B  COLOR133  // Aqua (closest to 56, 204, 108)
#define FUSELAGE_NES_COLOR2C  COLOR252  // Cyan (closest to 56, 180, 204)
#define FUSELAGE_NES_COLOR2D  COLOR0    // Black
#define FUSELAGE_NES_COLOR2E  COLOR0    // Black (duplicate)
#define FUSELAGE_NES_COLOR2F  COLOR0    // Black (duplicate)

// Row 4: Brightest Colors
#define FUSELAGE_NES_COLOR30  COLOR15   // Bright White (closest to 255, 255, 255)
#define FUSELAGE_NES_COLOR31  COLOR240  // Bright Pink (closest to 204, 204, 255)
#define FUSELAGE_NES_COLOR32  COLOR241  // Bright Cyan (closest to 132, 214, 255)
#define FUSELAGE_NES_COLOR33  COLOR252  // Sky Blue (closest to 180, 210, 255)
#define FUSELAGE_NES_COLOR34  COLOR95   // Purple (closest to 228, 180, 255)
#define FUSELAGE_NES_COLOR35  COLOR240  // Magenta (closest to 252, 180, 228)
#define FUSELAGE_NES_COLOR36  COLOR95   // Pink (closest to 255, 180, 192)
#define FUSELAGE_NES_COLOR37  COLOR254  // Creamy Peach (closest to 255, 208, 112)
#define FUSELAGE_NES_COLOR38  COLOR230  // Yellow (closest to 240, 220, 0)
#define FUSELAGE_NES_COLOR39  COLOR253  // Mint Green (closest to 196, 248, 0)
#define FUSELAGE_NES_COLOR3A  COLOR175  // Bright Green (closest to 136, 255, 0)
#define FUSELAGE_NES_COLOR3B  COLOR133  // Aqua (closest to 116, 255, 152)
#define FUSELAGE_NES_COLOR3C  COLOR252  // Cyan (closest to 116, 236, 252)
#define FUSELAGE_NES_COLOR3D  COLOR0    // Black
#define FUSELAGE_NES_COLOR3E  COLOR0    // Black (duplicate)
#define FUSELAGE_NES_COLOR3F  COLOR15   // White (closest to 255, 255, 255)

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
#define ZX_COLOR00  COLOR0    // Black
#define ZX_COLOR01  COLOR48   // Blue
#define ZX_COLOR02  COLOR22   // Red
#define ZX_COLOR03  COLOR90   // Magenta
#define ZX_COLOR04  COLOR42   // Green
#define ZX_COLOR05  COLOR108  // Cyan
#define ZX_COLOR06  COLOR74   // Yellow
#define ZX_COLOR07  COLOR10   // White

#define ZX_COLOR08  COLOR0    // Bright Black (still black)
#define ZX_COLOR09  COLOR63   // Bright Blue
#define ZX_COLOR10  COLOR31   // Bright Red
#define ZX_COLOR11  COLOR240  // Bright Magenta
#define ZX_COLOR12  COLOR47   // Bright Green
#define ZX_COLOR13  COLOR111  // Bright Cyan
#define ZX_COLOR14  COLOR79   // Bright Yellow
#define ZX_COLOR15  COLOR15   // Bright White

// Commodore 64 colors
#define C64_COLOR00  COLOR0    // Black
#define C64_COLOR01  COLOR15   // White
#define C64_COLOR02  COLOR22   // Red
#define C64_COLOR03  COLOR227  // Cyan
#define C64_COLOR04  COLOR90   // Purple
#define C64_COLOR05  COLOR42   // Green
#define C64_COLOR06  COLOR58   // Blue
#define C64_COLOR07  COLOR74   // Yellow
#define C64_COLOR08  COLOR116  // Orange
#define C64_COLOR09  COLOR233  // Brown
#define C64_COLOR10  COLOR234  // Light Red
#define C64_COLOR11  COLOR5    // Dark Grey
#define C64_COLOR12  COLOR10   // Grey
#define C64_COLOR13  COLOR175  // Light Green
#define C64_COLOR14  COLOR252  // Light Blue
#define C64_COLOR15  COLOR13   // Light Grey

bool LoadPaletteFromSprite(unsigned char paletteeIndex, Color* spritepalette);
Color GetColorFromChar(unsigned char input);
Color GetCommodoreColor(unsigned char input);
Color GetTandyColor(unsigned char input);
Color GetANSIColor(unsigned char input);

Color Colors[256][16];

bool SetPalette(unsigned char paletteIndex, char colorIndex, Color color);

void InitPalettes(void);

#ifdef __cplusplus
}
#endif

#endif // COLORS_H
