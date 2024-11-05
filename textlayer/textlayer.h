#include "raylib.h"

/* fuselagetextlayer.h 

The Text Layer is Layer 0 
By default Layer 0 is off.

*/

#ifndef TEXTLAYER_H
#define TEXTLAYER_H

#define TEXT_LAYER_WIDTH 80
#define TEXT_LAYER_HEIGHT 45
#define CHARACTER_WIDTH 16
#define CHARACTER_HEIGHT 16

void SetupCharacterMaps();

void PlaceCharacterAtCell(int x, int y, unsigned char c, Color color);
void ShutdownCharacterMaps();
void DrawTextLayer();

bool TextLayerStatus();
bool TextLayerActive(); // Returns True if Active
bool TextLayerToggle(); // Returns True if Activated, False if Deactivated
bool TextLayerInactive(); // Returns True if Inactive

int tlPrint(const char* input, Color color, int currentPrintX, int currentPrintY);
int tlPrint(const char* input, Color color);
int tlPrint(const char* input);
int tlPrint(int input, Color, int currentPrintX, int currentPrintY);
int tlPrint(int input, Color);
int tlPrint(int input);
int tlPrintInt(int input, Color color, int currentPrintX, int currentPrintY);
int tlPrintInt(int input, Color color);
int tlPrintInt(int input);
int tlNewLine();
void tlCLS();
void tlCLS(Color);
void tlColor(Color);
void tlHome();
void tlSetCursor(int x, int y);
short tlGetCursor();
unsigned short tlScrollUp();
Color tlColor();

#endif
