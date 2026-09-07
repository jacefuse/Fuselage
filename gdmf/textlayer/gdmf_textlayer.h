#ifndef GDMF_TEXTLAYER_H
#define GDMF_TEXTLAYER_H

#include "gdmf.h"
#include "gdmf_colors.h"

#define GDMF_TEXTLAYER_VERSION "0.3.2026071502 COLON"

#define TEXT_LAYER_WIDTH  80
#define TEXT_LAYER_HEIGHT 45
#define CHARACTER_WIDTH   16
#define CHARACTER_HEIGHT  16

// Engine-internal shutdown hook -- called by GDMF, not by game code.
void gdmf_textlayer_shutdown(void);

// Layer 0 activation / status. Layer 0 is the engine + devtools text overlay
// (errors, stats, debugging) -- not for game/application content, which builds
// its own tile layer for text. The lowercase tl* prefix marks it as such.
bool tlStatus(void);
bool tlActivate(void);
bool tlDeactivate(void);
bool tlToggle(void);

// Print functions
int  tlPrintFormatted(const char* format, ...);
int  tlPrintFormattedC(Color color, const char* format, ...);
int  tlPrintCP(const char* input, Color color, int currentPrintX, int currentPrintY);
int  tlPrintC(const char* input, Color color);
int  tlPrint(const char* input);
int  tlPrintCharCP(unsigned char input, Color color, int currentPrintX, int currentPrintY);
int  tlPrintCharC(unsigned char input, Color color);
int  tlPrintChar(unsigned char input);
int  tlPrintIntCP(int input, Color color, int currentPrintX, int currentPrintY);
int  tlPrintIntC(int input, Color color);
int  tlPrintInt(int input);
int  tlNewLine(void);
void tlCLS(void);
void tlSetColor(Color color);
void tlHome(void);
void tlSetCursor(int x, int y);
int  tlGetCursor(void);
unsigned short tlGetCursorX(void);
unsigned short tlGetCursorY(void);
unsigned short tlScrollUp(void);
Color tlGetColor(void);

#endif // GDMF_TEXTLAYER_H
