// Cursor-advancing output. SEE: (GDMF_TEXT.md, Printing)
#include "gdmf_textlayer_state_private.h"
#include <limits.h>

int tlPrintCP(const char* input, Color color, int column, int row) {
    if (!input) {
        return 0;
    }
    if (!gdmf_textlayer_position_valid(column, row)) {
        return -1;
    }
    int output_count = 0;
    tlSetColor(color);
    for (const unsigned char* character = (const unsigned char*)input; *character; ++character) {
        if (*character == '\n') {
            column = 0;
            if (++row >= TEXT_LAYER_HEIGHT) {
                tlScrollUp();
                row = TEXT_LAYER_HEIGHT - 1;
            }
            continue;
        }
        if (*character == '\r') {
            continue;
        }
        if (*character == '\t') {
            column = (column / 8 + 1) * 8;
            if (column >= TEXT_LAYER_WIDTH) {
                column = 0;
                if (++row >= TEXT_LAYER_HEIGHT) {
                    tlScrollUp();
                    row = TEXT_LAYER_HEIGHT - 1;
                }
            }
            continue;
        }
        if (column >= TEXT_LAYER_WIDTH) {
            column = 0;
            ++row;
        }
        if (row >= TEXT_LAYER_HEIGHT) {
            tlScrollUp();
            row = TEXT_LAYER_HEIGHT - 1;
        }
        gdmf_textlayer_place(*character, color, column, row);
        if (output_count < INT_MAX) {
            ++output_count;
        }
        ++column;
    }
    tlSetCursor(column, row);
    return output_count;
}
int tlPrintC(const char* input, Color color) {
    return tlPrintCP(input, color, tlGetCursorX(), tlGetCursorY());
}
int tlPrint(const char* input) {
    return tlPrintC(input, tlGetColor());
}
int tlPrintCharCP(unsigned char input, Color color, int column, int row) {
    if (!gdmf_textlayer_position_valid(column, row)) {
        return -1;
    }
    tlSetColor(color);
    if (column >= TEXT_LAYER_WIDTH) {
        column = 0;
        ++row;
    }
    if (row >= TEXT_LAYER_HEIGHT) {
        tlScrollUp();
        row = TEXT_LAYER_HEIGHT - 1;
    }
    gdmf_textlayer_place(input, color, column, row);
    tlSetCursor(column + 1, row);
    return 1;
}
int tlPrintCharC(unsigned char input, Color color) {
    return tlPrintCharCP(input, color, tlGetCursorX(), tlGetCursorY());
}
int tlPrintChar(unsigned char input) {
    return tlPrintCharC(input, tlGetColor());
}
int tlPrintIntCP(int input, Color color, int column, int row) {
    char digits[sizeof(int) * CHAR_BIT + 2];
    char* first_digit = digits + sizeof(digits) - 1;
    // Unsigned subtraction also handles INT_MIN.
    unsigned magnitude = input < 0 ? 0u - (unsigned)input : (unsigned)input;
    *first_digit = '\0';
    do {
        *--first_digit = (char)('0' + magnitude % 10u);
        magnitude /= 10u;
    } while (magnitude);
    if (input < 0) {
        *--first_digit = '-';
    }
    return tlPrintCP(first_digit, color, column, row);
}
int tlPrintIntC(int input, Color color) {
    return tlPrintIntCP(input, color, tlGetCursorX(), tlGetCursorY());
}
int tlPrintInt(int input) {
    return tlPrintIntC(input, tlGetColor());
}
