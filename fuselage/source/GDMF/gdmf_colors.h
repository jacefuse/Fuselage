// GDMF Colors - palette storage, color constants, and palette operations.
#ifndef GDMF_COLORS_H
#define GDMF_COLORS_H

#include <stdbool.h>
#include <stdint.h>

#define GDMF_COLORS_VERSION "0.5.2026102101 ENTERON"
#define FUSELAGE_PALETTE_SIZE 16

// RGBA8 components. The shared guard permits an existing compatible Color type.
#ifndef FUSELAGE_COLOR_TYPE
#define FUSELAGE_COLOR_TYPE
typedef struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} Color;
#endif

_Static_assert(sizeof(Color) == 4, "Fuselage expects RGBA8 Color (4 bytes, no padding).");

#include "colors/gdmf_colornames.h"

// Packs R, G, B, A into bits 0-7, 8-15, 16-23, and 24-31 respectively.
uint32_t PackColorsRGBA8(Color color);

// Change marker for accepted palette setter calls and initialization.
// Compare for inequality, not ordering: this uint32_t value can wrap to zero.
// Direct array writes bypass the marker. Synchronize palette access with rendering.
uint32_t GetColorsGeneration(void);

// Legacy palette indices.
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

// Numbered base colors, indexed 0-255.
#define GDMF_COLOR0  (Color){0, 0, 0, 255}
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
#define GDMF_COLOR15 (Color){255, 255, 255, 255}
#define GDMF_COLOR16  (Color){16, 0, 0, 255}
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
#define GDMF_COLOR31  (Color){255, 0, 0, 255}
#define GDMF_COLOR32  (Color){0, 16, 0, 255}
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
#define GDMF_COLOR47  (Color){0, 255, 0, 255}
#define GDMF_COLOR48  (Color){0, 0, 16, 255}
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
#define GDMF_COLOR63  (Color){0, 0, 255, 255}
#define GDMF_COLOR64  (Color){16, 16, 0, 255}
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
#define GDMF_COLOR79  (Color){255, 255, 0, 255}
#define GDMF_COLOR80  (Color){16, 0, 16, 255}
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
#define GDMF_COLOR95  (Color){255, 0, 255, 255}
#define GDMF_COLOR96   (Color){0, 16, 16, 255}
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
#define GDMF_COLOR111  (Color){0, 255, 255, 255}
#define GDMF_COLOR112  (Color){255, 64, 0, 255}
#define GDMF_COLOR113  (Color){255, 80, 0, 255}
#define GDMF_COLOR114  (Color){255, 96, 0, 255}
#define GDMF_COLOR115  (Color){255, 112, 0, 255}
#define GDMF_COLOR116  (Color){255, 128, 0, 255}
#define GDMF_COLOR117  (Color){255, 144, 0, 255}
#define GDMF_COLOR118  (Color){255, 160, 0, 255}
#define GDMF_COLOR119  (Color){255, 176, 0, 255}
#define GDMF_COLOR120  (Color){255, 192, 0, 255}
#define GDMF_COLOR121  (Color){255, 208, 0, 255}
#define GDMF_COLOR122  (Color){255, 224, 0, 255}
#define GDMF_COLOR123  (Color){255, 240, 0, 255}
#define GDMF_COLOR124  (Color){255, 255, 0, 255}
#define GDMF_COLOR125  (Color){224, 255, 0, 255}
#define GDMF_COLOR126  (Color){192, 255, 0, 255}
#define GDMF_COLOR127  (Color){160, 255, 0, 255}
#define GDMF_COLOR128  (Color){128, 255, 0, 255}
#define GDMF_COLOR129  (Color){96, 255, 0, 255}
#define GDMF_COLOR130  (Color){64, 255, 0, 255}
#define GDMF_COLOR131  (Color){32, 255, 16, 255}
#define GDMF_COLOR132  (Color){0, 255, 64, 255}
#define GDMF_COLOR133  (Color){0, 255, 80, 255}
#define GDMF_COLOR134  (Color){0, 255, 96, 255}
#define GDMF_COLOR135  (Color){0, 255, 112, 255}
#define GDMF_COLOR136  (Color){0, 255, 128, 255}
#define GDMF_COLOR137  (Color){0, 255, 144, 255}
#define GDMF_COLOR138  (Color){0, 255, 160, 255}
#define GDMF_COLOR139  (Color){0, 255, 176, 255}
#define GDMF_COLOR140  (Color){0, 255, 192, 255}
#define GDMF_COLOR141  (Color){0, 240, 208, 255}
#define GDMF_COLOR142  (Color){0, 224, 224, 255}
#define GDMF_COLOR143  (Color){0, 208, 240, 255}
#define GDMF_COLOR144  (Color){0, 192, 255, 255}
#define GDMF_COLOR145  (Color){0, 160, 255, 255}
#define GDMF_COLOR146  (Color){0, 128, 255, 255}
#define GDMF_COLOR147  (Color){0, 96, 255, 255}
#define GDMF_COLOR148  (Color){0, 64, 255, 255}
#define GDMF_COLOR149  (Color){0, 32, 255, 255}
#define GDMF_COLOR150  (Color){0, 0, 255, 255}
#define GDMF_COLOR151  (Color){16, 0, 240, 255}
#define GDMF_COLOR152  (Color){32, 0, 224, 255}
#define GDMF_COLOR153  (Color){48, 0, 208, 255}
#define GDMF_COLOR154  (Color){64, 0, 192, 255}
#define GDMF_COLOR155  (Color){80, 0, 176, 255}
#define GDMF_COLOR156  (Color){96, 0, 160, 255}
#define GDMF_COLOR157  (Color){112, 0, 144, 255}
#define GDMF_COLOR158  (Color){128, 0, 128, 255}
#define GDMF_COLOR159  (Color){144, 0, 112, 255}
#define GDMF_COLOR160  (Color){160, 0, 96, 255}
#define GDMF_COLOR161  (Color){176, 0, 80, 255}
#define GDMF_COLOR162  (Color){192, 0, 64, 255}
#define GDMF_COLOR163  (Color){208, 0, 48, 255}
#define GDMF_COLOR164  (Color){224, 0, 32, 255}
#define GDMF_COLOR165  (Color){240, 0, 16, 255}
#define GDMF_COLOR166  (Color){255, 0, 0, 255}
#define GDMF_COLOR167  (Color){255, 16, 0, 255}
#define GDMF_COLOR168  (Color){255, 32, 0, 255}
#define GDMF_COLOR169  (Color){255, 48, 0, 255}
#define GDMF_COLOR170  (Color){255, 64, 0, 255}
#define GDMF_COLOR171  (Color){255, 192, 192, 255}
#define GDMF_COLOR172  (Color){255, 224, 192, 255}
#define GDMF_COLOR173  (Color){255, 240, 192, 255}
#define GDMF_COLOR174  (Color){255, 255, 192, 255}
#define GDMF_COLOR175  (Color){240, 255, 192, 255}
#define GDMF_COLOR176  (Color){224, 255, 192, 255}
#define GDMF_COLOR177  (Color){192, 255, 192, 255}
#define GDMF_COLOR178  (Color){192, 255, 224, 255}
#define GDMF_COLOR179  (Color){192, 255, 240, 255}
#define GDMF_COLOR180  (Color){192, 255, 255, 255}
#define GDMF_COLOR181  (Color){192, 240, 255, 255}
#define GDMF_COLOR182  (Color){192, 224, 255, 255}
#define GDMF_COLOR183  (Color){192, 192, 255, 255}
#define GDMF_COLOR184  (Color){224, 192, 255, 255}
#define GDMF_COLOR185  (Color){240, 192, 255, 255}
#define GDMF_COLOR186  (Color){255, 192, 255, 255}
#define GDMF_COLOR187  (Color){255, 192, 240, 255}
#define GDMF_COLOR188  (Color){255, 192, 224, 255}
#define GDMF_COLOR189  (Color){255, 192, 208, 255}
#define GDMF_COLOR190  (Color){255, 192, 192, 255}
#define GDMF_COLOR191  (Color){128, 64, 64, 255}
#define GDMF_COLOR192  (Color){128, 96, 64, 255}
#define GDMF_COLOR193  (Color){128, 112, 64, 255}
#define GDMF_COLOR194  (Color){128, 128, 64, 255}
#define GDMF_COLOR195  (Color){112, 128, 64, 255}
#define GDMF_COLOR196  (Color){96, 128, 64, 255}
#define GDMF_COLOR197  (Color){64, 128, 64, 255}
#define GDMF_COLOR198  (Color){64, 128, 96, 255}
#define GDMF_COLOR199  (Color){64, 128, 112, 255}
#define GDMF_COLOR200  (Color){64, 128, 128, 255}
#define GDMF_COLOR201  (Color){64, 112, 128, 255}
#define GDMF_COLOR202  (Color){64, 96, 128, 255}
#define GDMF_COLOR203  (Color){64, 64, 128, 255}
#define GDMF_COLOR204  (Color){96, 64, 128, 255}
#define GDMF_COLOR205  (Color){112, 64, 128, 255}
#define GDMF_COLOR206  (Color){128, 64, 128, 255}
#define GDMF_COLOR207  (Color){128, 64, 112, 255}
#define GDMF_COLOR208  (Color){128, 64, 96, 255}
#define GDMF_COLOR209  (Color){128, 64, 80, 255}
#define GDMF_COLOR210  (Color){128, 64, 64, 255}
#define GDMF_COLOR211  (Color){0, 0, 0, 255}
#define GDMF_COLOR212  (Color){255, 255, 255, 255}
#define GDMF_COLOR213  (Color){136, 0, 0, 255}
#define GDMF_COLOR214  (Color){170, 255, 238, 255}
#define GDMF_COLOR215  (Color){204, 68, 204, 255}
#define GDMF_COLOR216  (Color){0, 204, 85, 255}
#define GDMF_COLOR217  (Color){0, 0, 170, 255}
#define GDMF_COLOR218  (Color){238, 238, 119, 255}
#define GDMF_COLOR219  (Color){221, 136, 85, 255}
#define GDMF_COLOR220  (Color){102, 68, 0, 255}
#define GDMF_COLOR221  (Color){255, 119, 119, 255}
#define GDMF_COLOR222  (Color){51, 51, 51, 255}
#define GDMF_COLOR223  (Color){119, 119, 119, 255}
#define GDMF_COLOR224  (Color){170, 255, 102, 255}
#define GDMF_COLOR225  (Color){119, 221, 255, 255}
#define GDMF_COLOR226  (Color){187, 187, 187, 255}
#define GDMF_COLOR227  (Color){188, 62, 62, 255}
#define GDMF_COLOR228  (Color){255, 102, 0, 255}
#define GDMF_COLOR229  (Color){255, 182, 74, 255}
#define GDMF_COLOR230  (Color){255, 227, 77, 255}
#define GDMF_COLOR231  (Color){202, 255, 77, 255}
#define GDMF_COLOR232  (Color){107, 255, 77, 255}
#define GDMF_COLOR233  (Color){0, 155, 72, 255}
#define GDMF_COLOR234  (Color){77, 255, 202, 255}
#define GDMF_COLOR235  (Color){0, 155, 188, 255}
#define GDMF_COLOR236  (Color){77, 182, 255, 255}
#define GDMF_COLOR237  (Color){0, 72, 255, 255}
#define GDMF_COLOR238  (Color){72, 0, 255, 255}
#define GDMF_COLOR239  (Color){155, 77, 255, 255}
#define GDMF_COLOR240  (Color){255, 77, 255, 255}
#define GDMF_COLOR241  (Color){255, 77, 182, 255}
#define GDMF_COLOR242  (Color){155, 77, 107, 255}
#define GDMF_COLOR243  (Color){107, 77, 62, 255}
#define GDMF_COLOR244  (Color){255, 148, 89, 255}
#define GDMF_COLOR245  (Color){188, 107, 89, 255}
#define GDMF_COLOR246  (Color){107, 155, 89, 255}
#define GDMF_COLOR247  (Color){62, 107, 155, 255}
#define GDMF_COLOR248  (Color){89, 62, 155, 255}
#define GDMF_COLOR249  (Color){155, 62, 188, 255}
#define GDMF_COLOR250  (Color){89, 0, 188, 255}
#define GDMF_COLOR251  (Color){89, 107, 188, 255}
#define GDMF_COLOR252  (Color){89, 188, 255, 255}
#define GDMF_COLOR253  (Color){188, 255, 155, 255}
#define GDMF_COLOR254  (Color){255, 202, 155, 255}
#define GDMF_COLOR255  (Color){202, 202, 202, 255}

// Tandy/CGA RGBI color constants. TANDY_YELLOW is the brown entry.
#define TANDY_BLACK          (Color){0x00, 0x00, 0x00, 255}
#define TANDY_BLUE           (Color){0x00, 0x00, 0xAA, 255}
#define TANDY_GREEN          (Color){0x00, 0xAA, 0x00, 255}
#define TANDY_CYAN           (Color){0x00, 0xAA, 0xAA, 255}
#define TANDY_RED            (Color){0xAA, 0x00, 0x00, 255}
#define TANDY_MAGENTA        (Color){0xAA, 0x00, 0xAA, 255}
#define TANDY_YELLOW         (Color){0xAA, 0x55, 0x00, 255}
#define TANDY_WHITE          (Color){0xAA, 0xAA, 0xAA, 255}
#define TANDY_BRIGHT_BLACK   (Color){0x55, 0x55, 0x55, 255}
#define TANDY_BRIGHT_BLUE    (Color){0x55, 0x55, 0xFF, 255}
#define TANDY_BRIGHT_GREEN   (Color){0x55, 0xFF, 0x55, 255}
#define TANDY_BRIGHT_CYAN    (Color){0x55, 0xFF, 0xFF, 255}
#define TANDY_BRIGHT_RED     (Color){0xFF, 0x55, 0x55, 255}
#define TANDY_BRIGHT_MAGENTA (Color){0xFF, 0x55, 0xFF, 255}
#define TANDY_BRIGHT_YELLOW  (Color){0xFF, 0xFF, 0x55, 255}
#define TANDY_BRIGHT_WHITE   (Color){0xFF, 0xFF, 0xFF, 255}

// ANSI names use the Tandy values; terminal RGB colors may differ.
#define ANSI_BLACK           TANDY_BLACK
#define ANSI_RED             TANDY_RED
#define ANSI_GREEN           TANDY_GREEN
#define ANSI_YELLOW          TANDY_YELLOW
#define ANSI_BLUE            TANDY_BLUE
#define ANSI_MAGENTA         TANDY_MAGENTA
#define ANSI_CYAN            TANDY_CYAN
#define ANSI_WHITE           TANDY_WHITE
#define ANSI_BRIGHT_BLACK    TANDY_BRIGHT_BLACK
#define ANSI_BRIGHT_RED      TANDY_BRIGHT_RED
#define ANSI_BRIGHT_GREEN    TANDY_BRIGHT_GREEN
#define ANSI_BRIGHT_YELLOW   TANDY_BRIGHT_YELLOW
#define ANSI_BRIGHT_BLUE     TANDY_BRIGHT_BLUE
#define ANSI_BRIGHT_MAGENTA  TANDY_BRIGHT_MAGENTA
#define ANSI_BRIGHT_CYAN     TANDY_BRIGHT_CYAN
#define ANSI_BRIGHT_WHITE    TANDY_BRIGHT_WHITE

// NES reference palette. Original source attribution: nesdev 2C02G_wiki.pal.
#define NES_COLOR00  (Color){ 98,  98,  98, 255}
#define NES_COLOR01  (Color){  0,  28, 149, 255}
#define NES_COLOR02  (Color){ 25,   4, 172, 255}
#define NES_COLOR03  (Color){ 66,   0, 157, 255}
#define NES_COLOR04  (Color){ 97,   0, 107, 255}
#define NES_COLOR05  (Color){110,   0,  37, 255}
#define NES_COLOR06  (Color){101,   5,   0, 255}
#define NES_COLOR07  (Color){ 73,  30,   0, 255}
#define NES_COLOR08  (Color){ 34,  55,   0, 255}
#define NES_COLOR09  (Color){  0,  73,   0, 255}
#define NES_COLOR0A  (Color){  0,  79,   0, 255}
#define NES_COLOR0B  (Color){  0,  72,  22, 255}
#define NES_COLOR0C  (Color){  0,  53,  94, 255}
#define NES_COLOR0D  (Color){  0,   0,   0, 255}
#define NES_COLOR0E  (Color){  0,   0,   0, 255}
#define NES_COLOR0F  (Color){  0,   0,   0, 255}
#define NES_COLOR10  (Color){171, 171, 171, 255}
#define NES_COLOR11  (Color){ 12,  78, 219, 255}
#define NES_COLOR12  (Color){ 61,  46, 255, 255}
#define NES_COLOR13  (Color){113,  21, 243, 255}
#define NES_COLOR14  (Color){155,  11, 185, 255}
#define NES_COLOR15  (Color){176,  18,  98, 255}
#define NES_COLOR16  (Color){169,  39,   4, 255}
#define NES_COLOR17  (Color){137,  70,   0, 255}
#define NES_COLOR18  (Color){ 87, 102,   0, 255}
#define NES_COLOR19  (Color){ 35, 127,   0, 255}
#define NES_COLOR1A  (Color){  0, 137,   0, 255}
#define NES_COLOR1B  (Color){  0, 131,  50, 255}
#define NES_COLOR1C  (Color){  0, 109, 144, 255}
#define NES_COLOR1D  (Color){  0,   0,   0, 255}
#define NES_COLOR1E  (Color){  0,   0,   0, 255}
#define NES_COLOR1F  (Color){  0,   0,   0, 255}
#define NES_COLOR20  (Color){255, 255, 255, 255}
#define NES_COLOR21  (Color){ 87, 165, 255, 255}
#define NES_COLOR22  (Color){130, 135, 255, 255}
#define NES_COLOR23  (Color){180, 109, 255, 255}
#define NES_COLOR24  (Color){223,  96, 255, 255}
#define NES_COLOR25  (Color){248,  99, 198, 255}
#define NES_COLOR26  (Color){248, 116, 109, 255}
#define NES_COLOR27  (Color){222, 144,  32, 255}
#define NES_COLOR28  (Color){179, 174,   0, 255}
#define NES_COLOR29  (Color){129, 200,   0, 255}
#define NES_COLOR2A  (Color){ 86, 213,  34, 255}
#define NES_COLOR2B  (Color){ 61, 211, 111, 255}
#define NES_COLOR2C  (Color){ 62, 193, 200, 255}
#define NES_COLOR2D  (Color){ 78,  78,  78, 255}
#define NES_COLOR2E  (Color){  0,   0,   0, 255}
#define NES_COLOR2F  (Color){  0,   0,   0, 255}
#define NES_COLOR30  (Color){255, 255, 255, 255}
#define NES_COLOR31  (Color){190, 224, 255, 255}
#define NES_COLOR32  (Color){205, 212, 255, 255}
#define NES_COLOR33  (Color){224, 202, 255, 255}
#define NES_COLOR34  (Color){241, 196, 255, 255}
#define NES_COLOR35  (Color){252, 196, 239, 255}
#define NES_COLOR36  (Color){253, 202, 206, 255}
#define NES_COLOR37  (Color){245, 212, 175, 255}
#define NES_COLOR38  (Color){230, 223, 156, 255}
#define NES_COLOR39  (Color){211, 233, 154, 255}
#define NES_COLOR3A  (Color){194, 239, 168, 255}
#define NES_COLOR3B  (Color){183, 239, 196, 255}
#define NES_COLOR3C  (Color){182, 234, 229, 255}
#define NES_COLOR3D  (Color){184, 184, 184, 255}
#define NES_COLOR3E  (Color){  0,   0,   0, 255}
#define NES_COLOR3F  (Color){  0,   0,   0, 255}

// Commodore 64 palette. Original attribution: Pepto PAL, VICE pepto-pal.vpl.
#define C64_COLOR00  (Color){0x00, 0x00, 0x00, 255}
#define C64_COLOR01  (Color){0xFF, 0xFF, 0xFF, 255}
#define C64_COLOR02  (Color){0x68, 0x37, 0x2B, 255}
#define C64_COLOR03  (Color){0x70, 0xA4, 0xB2, 255}
#define C64_COLOR04  (Color){0x6F, 0x3D, 0x86, 255}
#define C64_COLOR05  (Color){0x58, 0x8D, 0x43, 255}
#define C64_COLOR06  (Color){0x35, 0x28, 0x79, 255}
#define C64_COLOR07  (Color){0xB8, 0xC7, 0x6F, 255}
#define C64_COLOR08  (Color){0x6F, 0x4F, 0x25, 255}
#define C64_COLOR09  (Color){0x43, 0x39, 0x00, 255}
#define C64_COLOR10  (Color){0x9A, 0x67, 0x59, 255}
#define C64_COLOR11  (Color){0x44, 0x44, 0x44, 255}
#define C64_COLOR12  (Color){0x6C, 0x6C, 0x6C, 255}
#define C64_COLOR13  (Color){0x9A, 0xD2, 0x84, 255}
#define C64_COLOR14  (Color){0x6C, 0x5E, 0xB5, 255}
#define C64_COLOR15  (Color){0x95, 0x95, 0x95, 255}

// 256 independent palettes of 16 entries. Read through the getters and use
// setters for mutations so renderer change tracking is updated.
extern Color Colors[256][16];

// Copies entries 1-15 from a 16-entry input, preserving entry zero's default.
// An entry exactly matching entry zero (including alpha) becomes BLANK.
// Returns false for NULL input, otherwise true. Use the individual setter to
// change entry zero explicitly. This is a usage convention, not a restriction
// imposed by every palette operation.
bool SetColorsPalette(unsigned char palette_index, Color* palette);

// Sets one entry, including entry zero. Returns false if color_index >= 16;
// otherwise true. All unsigned-char palette indices select a valid palette.
bool SetColorsPaletteColor(unsigned char palette_index, unsigned char color_index, Color color);

// Returns BLANK if color_index >= 16. BLANK can also be a stored value.
Color GetColorsPaletteColor(unsigned char palette_index, unsigned char color_index);

// Fills every entry of palette N with GDMF_COLORN, for all 256 palettes.
void InitColorsPalettes(void);

// Looks up a numbered base constant, independently of mutable palette storage.
Color GetColorsColorFromChar(unsigned char color_index);

// Indices 0-15; larger inputs return opaque black.
Color GetColorsCommodoreColor(unsigned char color_index);
Color GetColorsTandyColor(unsigned char color_index);

// Uses Tandy/CGA index order: black, blue, green, cyan, red, magenta, yellow,
// white, then the bright variants. These are not ANSI SGR color numbers.
// Inputs greater than 15 return opaque black.
Color GetColorsANSIColor(unsigned char color_index);

// Separate storage for sixteen 256-entry palettes, used by PALETTE8BPP Pixies.
// Changes here do not alter the 16-entry palettes above.
#define FUSELAGE_DEEP_PALETTE_COUNT 16
#define FUSELAGE_DEEP_PALETTE_SIZE  256
extern Color DeepColors[FUSELAGE_DEEP_PALETTE_COUNT][FUSELAGE_DEEP_PALETTE_SIZE];

// Change marker with the same wrap and synchronization requirements as
// GetColorsGeneration. Direct array writes bypass it.
uint32_t GetColorsDeepGeneration(void);

// Copies all 256 entries, including entry zero. Input must contain 256 Colors.
// Returns false for palette_index >= 16 or NULL input, otherwise true.
bool SetColorsDeepPalette(unsigned char palette_index, const Color* palette);

// Sets one entry, including entry zero. Returns false for palette_index >= 16,
// otherwise true. Every unsigned-char color index is valid in a deep palette.
bool SetColorsDeepPaletteColor(unsigned char palette_index, unsigned char color_index, Color color);

// Returns BLANK for palette_index >= 16; BLANK can also be a stored value.
Color GetColorsDeepPaletteColor(unsigned char palette_index, unsigned char color_index);

// Initializes palette zero to opaque grayscale and the remaining palettes to
// opaque channel-offset ramps. Called by the engine at startup.
void InitColorsDeepPalettes(void);

#endif // GDMF_COLORS_H
