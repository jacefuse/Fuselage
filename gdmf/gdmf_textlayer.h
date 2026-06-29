#pragma once
#include <stdio.h>
#include <stdarg.h>
#include "gdmf.h"
#include "colors.h"

#ifndef FUSELAGE_TEXTLAYER_H
#define FUSELAGE_TEXTLAYER_H

#define GDMF_TEXTLAYER_VERSION "0.2.2026062701"

#define TEXT_LAYER_WIDTH  80
#define TEXT_LAYER_HEIGHT 45
#define CHARACTER_WIDTH   16
#define CHARACTER_HEIGHT  16

#define DEFAULT_COLOR_R -1
#define DEFAULT_COLOR_G -1
#define DEFAULT_COLOR_B -1
#define DEFAULT_COLOR_A -1
#define DEFAULT_COLOR (Color){ DEFAULT_COLOR_R, DEFAULT_COLOR_G, DEFAULT_COLOR_B, DEFAULT_COLOR_A }

// Debug helpers
void debug_cursor_and_text(const char* operation);
void debug_print_text_grid(void);
void debug_buffer_contents(void);

// Lifecycle
void SetupCharacterMaps(void);
void ShutdownCharacterMaps(void);
void gdmf_textlayer_shutdown(void);

// Cleanup
void cleanup_text_layer_resources(void);

// Cell write
void PlaceCharacterAtCell(unsigned short x, unsigned short y, unsigned char c, Color color);

// Status
bool TextLayerStatus(void);
bool TextLayerActive(void);
bool TextLayerToggle(void);
bool TextLayerInactive(void);

// Print functions
int  tlPrintFormatted(const char* format, ...);
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

#endif // FUSELAGE_TEXTLAYER_H
