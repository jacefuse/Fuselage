// Grid/cursor state. SEE: (GDMF_TEXT.md, Grid state)
#include "gdmf_textlayer_state_private.h"

static gdmf_textlayer_state gdmf_textlayer_host = {.cursor_color = {255, 255, 255, 255}};
gdmf_textlayer_state* gdmf_textlayer_state_get(void) {
    return &gdmf_textlayer_host;
}
bool gdmf_textlayer_position_valid(int column, int row) {
    return column >= 0 && column <= TEXT_LAYER_WIDTH && row >= 0 && row < TEXT_LAYER_HEIGHT;
}
void gdmf_textlayer_place(unsigned char character, Color color, int column, int row) {
    if (column >= 0 && column < TEXT_LAYER_WIDTH && row >= 0 && row < TEXT_LAYER_HEIGHT) {
        gdmf_textlayer_host.characters[column][row] = character;
        gdmf_textlayer_host.colors[column][row] = color;
    }
}
bool tlStatus(void) {
    return gdmf_textlayer_host.active;
}
bool tlActivate(void) {
    gdmf_textlayer_host.active = true;
    return gdmf_textlayer_host.active;
}
bool tlDeactivate(void) {
    gdmf_textlayer_host.active = false;
    return gdmf_textlayer_host.active;
}
bool tlToggle(void) {
    gdmf_textlayer_host.active = !gdmf_textlayer_host.active;
    return gdmf_textlayer_host.active;
}
void tlHome(void) {
    gdmf_textlayer_host.cursor_column = 0;
    gdmf_textlayer_host.cursor_row = 0;
}
void tlSetColor(Color color) {
    gdmf_textlayer_host.cursor_color = color;
}
Color tlGetColor(void) {
    return gdmf_textlayer_host.cursor_color;
}
void tlSetCursor(int column, int row) {
    if (column < 0) {
        column = 0;
    } else if (column > TEXT_LAYER_WIDTH) {
        column = TEXT_LAYER_WIDTH;
    }
    if (row < 0) {
        row = 0;
    } else if (row >= TEXT_LAYER_HEIGHT) {
        row = TEXT_LAYER_HEIGHT - 1;
    }
    gdmf_textlayer_host.cursor_column = (unsigned short)column;
    gdmf_textlayer_host.cursor_row = (unsigned short)row;
}
int tlGetCursor(void) {
    return gdmf_textlayer_host.cursor_row * TEXT_LAYER_WIDTH + gdmf_textlayer_host.cursor_column;
}
unsigned short tlGetCursorX(void) {
    return gdmf_textlayer_host.cursor_column;
}
unsigned short tlGetCursorY(void) {
    return gdmf_textlayer_host.cursor_row;
}
unsigned char tlGetCellCharacter(int column, int row) {
    if (column < 0 || column >= TEXT_LAYER_WIDTH || row < 0 || row >= TEXT_LAYER_HEIGHT) {
        return ' ';
    }
    return gdmf_textlayer_host.characters[column][row];
}
Color tlGetCellColor(int column, int row) {
    if (column < 0 || column >= TEXT_LAYER_WIDTH || row < 0 || row >= TEXT_LAYER_HEIGHT) {
        return BLACK;
    }
    return gdmf_textlayer_host.colors[column][row];
}
void tlCLS(void) {
    for (int row = 0; row < TEXT_LAYER_HEIGHT; ++row) {
        for (int column = 0; column < TEXT_LAYER_WIDTH; ++column) {
            gdmf_textlayer_place(' ', BLACK, column, row);
        }
    }
    tlSetColor(BLACK);
    tlHome();
}
unsigned short tlScrollUp(void) {
    for (int row = 0; row < TEXT_LAYER_HEIGHT - 1; ++row) {
        for (int column = 0; column < TEXT_LAYER_WIDTH; ++column) {
            gdmf_textlayer_place(gdmf_textlayer_host.characters[column][row + 1],
                                 gdmf_textlayer_host.colors[column][row + 1], column, row);
        }
    }
    for (int column = 0; column < TEXT_LAYER_WIDTH; ++column) {
        gdmf_textlayer_place(0, BLANK, column, TEXT_LAYER_HEIGHT - 1);
    }
    return 0;
}
int tlNewLine(void) {
    gdmf_textlayer_host.cursor_column = 0;
    if (++gdmf_textlayer_host.cursor_row >= TEXT_LAYER_HEIGHT) {
        tlScrollUp();
        gdmf_textlayer_host.cursor_row = TEXT_LAYER_HEIGHT - 1;
    }
    return gdmf_textlayer_host.cursor_row;
}
