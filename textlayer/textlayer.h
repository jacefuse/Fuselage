#include <raylib.h>
#include <stdio.h>
#include <stdarg.h>

#ifndef FUSELAGE_TEXTLAYER_H
#define FUSELAGE_TEXTLAYER_H
#define FUSELAGE_TEXTLAYER_VERSION "20250108"

#define TEXT_LAYER_WIDTH 80
#define TEXT_LAYER_HEIGHT 45
#define CHARACTER_WIDTH 16
#define CHARACTER_HEIGHT 16

#define DEFAULT_COLOR_R -1
#define DEFAULT_COLOR_G -1
#define DEFAULT_COLOR_B -1
#define DEFAULT_COLOR_A -1
//#define DEFAULT_POSITION -1
//#define CURRENT_POSITION -1
#define DEFAULT_COLOR (Color){ DEFAULT_COLOR_R, DEFAULT_COLOR_G, DEFAULT_COLOR_B, DEFAULT_COLOR_A }

#ifdef __cplusplus
extern "C" {
#endif

void SetupCharacterMaps();

void PlaceCharacterAtCell(unsigned short x, unsigned short y, unsigned char c, Color color);
void ShutdownCharacterMaps();
void DrawTextLayer();

bool TextLayerStatus();
bool TextLayerActive(); // Returns True if Active
bool TextLayerToggle(); // Returns True if Activated, False if Deactivated
bool TextLayerInactive(); // Returns True if Inactive

int tlPrintFormatted(const char* format, ...);
int tlPrintCP(const char* input, Color color, int currentPrintX, int currentPrintY);
int tlPrintC(const char* input, Color color);
int tlPrint(const char* input);
int tlPrintCharCP(unsigned char input, Color, int currentPrintX, int currentPrintY);
int tlPrintCharC(unsigned char input, Color);
int tlPrintChar(unsigned char input);
int tlPrintIntCP(int input, Color color, int currentPrintX, int currentPrintY);
int tlPrintIntC(int input, Color color);
int tlPrintInt(int input);
int tlNewLine();
void tlCLS();
void tlSetColor(Color);
void tlHome();
void tlSetCursor(int x, int y);
int tlGetCursor();
unsigned short tlGetCursorX();
unsigned short tlGetCursorY();
unsigned short tlScrollUp();
Color tlGetColor();

#ifdef __cplusplus
}
#endif

#endif
