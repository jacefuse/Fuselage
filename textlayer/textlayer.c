#include "raylib.h"
#include "textlayer.h"
#include "packedbitmaps.h" 

//  This bitmap should be an array: char characterBitmap[256][64]
//  Creating this bitmap by hand is not advised.
//  The program Edit Bitmap (aka Edibima) should be used.
//  The reason this is is still C++ is because of the use of function overloads.
//  When functions are converted to variadic, this will change.

bool textLayerActiveStatus = 1;
bool textHasChanged = 0;
Color cursorColor = { 255, 255, 255, 255 };
Color backgroundColor = { 0, 0, 0, 0 };
Color textDisplayCellColor[TEXT_LAYER_WIDTH][TEXT_LAYER_HEIGHT];
unsigned char textDisplayCell[TEXT_LAYER_WIDTH][TEXT_LAYER_HEIGHT];
//unsigned char textHistoryBuffer[64800]; // Currently unused.
unsigned short cursorPositionX = 0;
unsigned short cursorPositionY = 0;
unsigned short textHistoryBufferPosition = 0;
//Texture2D charTexture[256]; //Obsolete -- Uses individual textures per character.
Texture2D characterAtlas;
unsigned char charCountIncWrap(); // Locale because not being kept

// Reads every cell value and places the corresponding texture at that location.
// Should be called in main game loop inside drawing calls, preferably before the last.
// This layout assumes each character is 16x16 pixels and fills a 1280x720 display.
// The veriables are as listed below
// unsigned char textDisplayCell[80][45];
// Color textDisplayCellColor[80][45];
// Each character is drawn based on the content of the cell gathering its character bitmap from textDisplayCell and
// coloring it based on the content of textDisplayCellColor.
void DrawTextLayer() { 

	if (textLayerActiveStatus) {
		for (int x = 0; x < TEXT_LAYER_WIDTH; x++) {
			for (int y = 0; y < TEXT_LAYER_HEIGHT; y++) {
				unsigned char cellValue = textDisplayCell[x][y];
				int atlasX = (cellValue % 16) * 16;
				int atlasY = (cellValue / 16) * 16;
				Rectangle source = { (float)atlasX, (float)atlasY, 16, 16 }; // Coordinates inside atlas to draw.
				Rectangle destination = { (float)x * 16, (float)y * 16, 16.0, 16.0 }; // Location and size for drawing texture.
				Vector2 offset = { 0 , 0 }; // Origin point of drawn texture.
				float rotation = 0.0f;
				DrawTexturePro(characterAtlas, source, destination, offset, rotation, textDisplayCellColor[x][y]);
			}
		}
	}

}

// Pushes all character arrays into textures. 
// Called at Initialization.
void SetupCharacterMaps() {

	// Initialize the atlas image.
	Image atlas = GenImageColor(256, 256, BLANK); // Assuming BLANK is a predefined color.
	SetTextureFilter(characterAtlas, TEXTURE_FILTER_POINT);
	char unpackedValues[4] = { 0 };

	for (int i = 0; i < 256; i++) {
		for (int j = 0; j < 64; j++) {
			// Unpack HEX values from array into character values 0-3 for 4 elements of the bitmap array.
			for (int k = 0; k < 4; ++k) {
				unpackedValues[k] = ((unsigned char)(characterBitmap[i][j]) >> (2 * k)) & 0x3;
			}

			for (int k = 0; k < 4; k++) {
				// Calculate global x and y in the atlas.
				int charX = (i % 16) * 16; // Character's top-left X in atlas
				int charY = (i / 16) * 16; // Character's top-left Y in atlas
				int x = charX + (j * 4 + k) % 16; // X position within the character cell
				int y = charY + (j * 4 + k) / 16; // Y position within the character cell
					
				Color color = BLANK; // Default to BLANK
				if (unpackedValues[k] == 1) color = WHITE;
				if (unpackedValues[k] == 2) color = DARKGRAY;
				if (unpackedValues[k] == 3) color = LIGHTGRAY;
				ImageDrawPixel(&atlas, x, y, color); // Draw the pixel directly onto the atlas.
			}
		}
	}

	// Convert the atlas to a Texture2D.
	characterAtlas = LoadTextureFromImage(atlas);
	UnloadImage(atlas);

	return;
}

// This is the primary function of Layer 0. This is a grid based text overlay which can be enabled or disabled.
// packedbitmaps.h provides the typeface and can be edited with Edibima.
// Eventually all tlPrint functions will be rolled into one variadic function.
// Several overloads are provided to handle various argument arrangements.

// Formatted accepts arguments like printf. Does not accept Fuselage options other than color.
// Usage: tlPrintFormatted(const char*, (optional color), variadic arguments...);

int tlPrintFormatted(const char* format, ...) {
	if (!format) {
		return 0; // Gracefully handle null format
	}

	// Default to the current text layer state
	Color color = tlGetColor(); // Default color

	va_list args;
	va_start(args, format);

	// Peek at the first variadic argument
	va_list argsCopy;
	va_copy(argsCopy, args);

	// Initialize a flag for whether `Color` is provided
	int hasColor = 0;

	// Check if the first argument is a valid `Color`
	Color tempColor = va_arg(argsCopy, Color);
	if (tempColor.r >= 0 && tempColor.g >= 0 && tempColor.b >= 0 && tempColor.a >= 0) {
		hasColor = 1;  // Mark `Color` as present
		color = tempColor; // Use the provided color
	}
	va_end(argsCopy);

	// Advance the argument list if `Color` was provided
	if (hasColor) {
		(void)va_arg(args, Color);
	}

	// Buffer to hold the formatted string
	char buffer[256];

	// Format the string with the remaining arguments
	int neededSize = vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	// Check for truncation
	if (neededSize >= (int)sizeof(buffer)) {
		fprintf(stderr, "Error: Formatted string exceeds buffer size of 256 characters.\n");
		return -1;
	}

	// Pass the formatted string and parameters to the core tlPrint function
	return tlPrintCP(buffer, color, cursorPositionX, cursorPositionY);
}

int tlPrintCP(const char* print, Color color, int currentPrintX, int currentPrintY) {

	int outputcount = 0;
	cursorColor = color;
	for (int i = 0; print[i] != '\0'; i++) {
		if (currentPrintX >= TEXT_LAYER_WIDTH) {
			currentPrintX = 0;
			currentPrintY++;
		}
		if (currentPrintY > TEXT_LAYER_HEIGHT -1 ) {
			tlScrollUp();
			currentPrintY = TEXT_LAYER_HEIGHT - 1;
		}
		PlaceCharacterAtCell(currentPrintX, currentPrintY, print[i], color);
		outputcount++;
		currentPrintX++;
	}
	cursorPositionX = currentPrintX;
	cursorPositionY = currentPrintY;

	return outputcount;
}

int tlPrintC(const char* print, Color color) {

	return tlPrintCP(print, color, cursorPositionX, cursorPositionY);

}

int tlPrint(const char* print) {

	Color color = cursorColor;

	return tlPrintCP(print, color, cursorPositionX, cursorPositionY);

}

// Integer based calls to standard tlPrint will need to have ASCII values added to them in order to return the expected character results.
// 32 = Whitespace : 48 = Zero : 65 = A
// This will be handled automatically once the function has been made variadic. 

int tlPrintCharCP(unsigned char input, Color color, int currentPrintX, int currentPrintY) {
		int outputcount = 0;
		cursorColor = color;
		if (currentPrintX >= TEXT_LAYER_WIDTH) {
			currentPrintX = 0;
			currentPrintY++;
		}
		if (currentPrintY > TEXT_LAYER_HEIGHT - 1) {
			tlScrollUp();
			currentPrintY = TEXT_LAYER_HEIGHT - 1;
		}
		PlaceCharacterAtCell(currentPrintX, currentPrintY, input, color);
		outputcount++;
		currentPrintX++;

		cursorPositionX = currentPrintX;
		cursorPositionY = currentPrintY;

		return outputcount;
}

int tlPrintCharC(unsigned char input, Color color) {

	return tlPrintCharCP(input, color, cursorPositionX, cursorPositionY);

}

int tlPrintChar(unsigned char input) {

	return tlPrintCharCP(input, cursorColor, cursorPositionX, cursorPositionY);

}

// Integer based Print Function converts Int type variables to Strings for Printing.
int tlPrintIntCP(int input, Color color, int currentPrintX, int currentPrintY) {
	//std::cout << input << "\n";
	char output[16] = "";
	int temp = input;
	int count = 1;

	for (long int i = 10; i <= input; i = i * 10) {
		count++;
	}
	//std::cout << count << "\n";
	for (int i = count - 1; i >= 0; i--) {								// Store the numbers
		if (count < 15) output[i] = (temp % 10) + 48;
		temp = temp / 10;
	}

	output[count] = '\0';

	//std::cout << output + 48 << "\n";

	return tlPrintCP((char*)output, color, currentPrintX, currentPrintY);
}

int tlPrintIntC(int input, Color color) {

	return tlPrintIntCP(input, color, cursorPositionX, cursorPositionY);
}

int tlPrintInt(int input) {

	return tlPrintIntCP(input, cursorColor, cursorPositionX, cursorPositionY);
}

/*/ Clear Screen Function.
void tlCLS() {
	cursorColor = BLACK;
	return tlCLS(cursorColor);
};*/

// Clears Screen and sets cursor color.
void tlCLS() {
	Color color = BLACK;
	for (int y = 0; y < TEXT_LAYER_HEIGHT; y++)
		for (int x = 0; x < TEXT_LAYER_WIDTH; x++) {
			{
				if (y < 45) {
					textDisplayCell[x][y] = 32;
					textDisplayCellColor[x][y] = color;
				}
			}
		}
	cursorColor = color;
	tlHome();

	return;
};

// Sets cursor position to X0:Y0.
void tlHome() {

	cursorPositionX = 0;
	cursorPositionY = 0;

	return;
}

// Sets cursor color.
void tlSetColor(Color color) {

	cursorColor = color;

	return;
}
// Gets cursor color.
Color tlGetColor() {
	return cursorColor;
}

// Sets cursor location.
void tlSetCursor(int x, int y) {

	cursorPositionX = x;
	cursorPositionY = y;

	return;
}

// Gets cursor location 0-3599

int tlGetCursor() {
	int x = cursorPositionX;
	int y = cursorPositionY;
	return y * TEXT_LAYER_WIDTH + x;
}

unsigned short tlGetCursorX() {
	return cursorPositionX;
}

unsigned short tlGetCursorY() {
	return cursorPositionY;
}

// Scrolls screen up and stores contents of top line in history buffer. 
// History buffer currently unused.
unsigned short tlScrollUp() {

	/*// Currently unused.
	for (int i = 0; i < TEXT_LAYER_WIDTH; ++i) {
		if (textHistoryBufferPosition == 64799) textHistoryBufferPosition = 0;
		textHistoryBuffer[textHistoryBufferPosition++] = textDisplayCell[i][0];
	} //*/

	for (int y = 0; y < TEXT_LAYER_HEIGHT - 1; ++y) {
		for (int x = 0; x < TEXT_LAYER_WIDTH; ++x) {
			textDisplayCell[x][y] = textDisplayCell[x][y + 1];
			textDisplayCellColor[x][y] = textDisplayCellColor[x][y + 1];
		}
	}

	for (int i = 0; i < TEXT_LAYER_WIDTH; ++i) {
		textDisplayCell[i][TEXT_LAYER_HEIGHT - 1] = 0;
		textDisplayCellColor[i][TEXT_LAYER_HEIGHT - 1] = BLANK;
	}

	return textHistoryBufferPosition;
}

// Returns cursor to position X0 and linefeeds cursor down to next line.
int tlNewLine() {

	cursorPositionX = 0;
	cursorPositionY++;
	if (cursorPositionY > TEXT_LAYER_HEIGHT - 1) {
		tlScrollUp();
		cursorPositionY = TEXT_LAYER_HEIGHT - 1;
	}

	return cursorPositionY; 
}

// Places Character value C inside Layer 0 at X/Y location.
void PlaceCharacterAtCell(unsigned short x, unsigned short y, unsigned char c, Color color) {

	//if (c == 0) c = charCountIncWrap();
	textDisplayCell[x][y] = c;
	textDisplayCellColor[x][y] = color;

	return;
}

// Release textures at shutdown. Will be replaced with iterative function when all bitmaps are complete.
void ShutdownCharacterMaps() {

	// for (int i = 0; i < 256; i++) {
	//	UnloadTexture(charTexture[i]); // Obsolete -- Uses individual textures per character.
	//	}
	
	UnloadTexture(characterAtlas);

	return;
}

// Returns the current text display status without a change.
bool TextLayerStatus() { 

	return textLayerActiveStatus;
}

// Enables the text layer and returns the status of ON.
bool TextLayerActive() { 

	textLayerActiveStatus = 1;

	return textLayerActiveStatus;
}

// Toggles the current text layer status and returns the value.
bool TextLayerToggle() { 

	textLayerActiveStatus = !textLayerActiveStatus;
	//std::cout << "TOGGLE\n";

	return textLayerActiveStatus;
}

// Disables the text layer and returns the status of OFF.
bool TextLayerInactive() {

	textLayerActiveStatus = 0;

	return textLayerActiveStatus;
}

// For testing - cycles through letters
unsigned char charCountIncWrap() {

	static unsigned char count = 0;

	if ((count < 32) || (count > 127)) count = 65;
	else count++;

	return count;
}
