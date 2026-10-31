#ifndef GDMF_TEXTLAYER_H
#define GDMF_TEXTLAYER_H
#include "../gdmf_colors.h"

#define GDMF_TEXTLAYER_VERSION "0.5.2026102801 ENTERON"
#define TEXT_LAYER_WIDTH 80
#define TEXT_LAYER_HEIGHT 45
#define CHARACTER_WIDTH 16
#define CHARACTER_HEIGHT 16

// Diagnostic overlay, not application text. Status calls return the resulting state.
bool tlStatus(void);
bool tlActivate(void);
bool tlDeactivate(void);
bool tlToggle(void);

// C selects a color; CP also positions the cursor. Every successful print leaves
// the cursor after its output. SEE: (GDMF_TEXT.md, Printing)
// CP rejects positions outside columns 0..80/rows 0..44 with -1 and no state change.
// String calls return the number of glyphs written; NULL input returns zero.
// Formatted output accepts at most 255 bytes; -1 for formatting/truncation errors.
int tlPrintFormatted(const char* format, ...);
int tlPrintFormattedC(Color color, const char* format, ...);
int tlPrintCP(const char* input, Color color, int column, int row);
int tlPrintC(const char* input, Color color);
int tlPrint(const char* input);
// Raw glyph byte; newline, carriage return and tab have no special meaning here.
int tlPrintCharCP(unsigned char input, Color color, int column, int row);
int tlPrintCharC(unsigned char input, Color color);
int tlPrintChar(unsigned char input);
int tlPrintIntCP(int input, Color color, int column, int row);
int tlPrintIntC(int input, Color color);
int tlPrintInt(int input);
int tlNewLine(void);
void tlCLS(void);
void tlSetColor(Color color);
void tlHome(void);
// Clamp to columns 0..80 and rows 0..44; column 80 is a pending wrap.
void tlSetCursor(int column, int row);
int tlGetCursor(void);
unsigned short tlGetCursorX(void);
unsigned short tlGetCursorY(void);
// Shifts the grid without moving the cursor. Returns zero; history is unimplemented.
unsigned short tlScrollUp(void);
Color tlGetColor(void);
// Outside the grid: space glyph and opaque black. SEE: (GDMF_TEXT.md, Grid state)
unsigned char tlGetCellCharacter(int column, int row);
Color tlGetCellColor(int column, int row);
#endif
