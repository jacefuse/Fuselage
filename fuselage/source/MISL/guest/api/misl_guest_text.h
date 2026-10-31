#pragma once
#include "misl_guest_call_internal.h"

// Diagnostic text ecalls.
// SEE: (MISL_GUEST.md, Diagnostic text)

static inline int tlPrint(const char* input) {
    return MISL_GUEST_CALL(MISL_TL_PRINT, misl_guest_address(input), 0, 0, 0, 0, 0, 0);
}

static inline int tlPrintC(const char* input, Color color) {
    return MISL_GUEST_CALL(MISL_TL_PRINT_C, misl_guest_address(input), misl_guest_pack_color(color), 0, 0, 0,
                           0, 0);
}

static inline int tlPrintCP(const char* input, Color color, int column, int row) {
    return MISL_GUEST_CALL(MISL_TL_PRINT_CP, misl_guest_address(input), misl_guest_pack_color(color), column,
                           row, 0, 0, 0);
}

static inline int tlPrintChar(unsigned char input) {
    return MISL_GUEST_CALL(MISL_TL_PRINTCHAR, input, 0, 0, 0, 0, 0, 0);
}

static inline int tlPrintCharC(unsigned char input, Color color) {
    return MISL_GUEST_CALL(MISL_TL_PRINTCHAR_C, input, misl_guest_pack_color(color), 0, 0, 0, 0, 0);
}

static inline int tlPrintCharCP(unsigned char input, Color color, int column, int row) {
    return MISL_GUEST_CALL(MISL_TL_PRINTCHAR_CP, input, misl_guest_pack_color(color), column, row, 0, 0, 0);
}

static inline int tlPrintInt(int input) {
    return MISL_GUEST_CALL(MISL_TL_PRINTINT, input, 0, 0, 0, 0, 0, 0);
}

static inline int tlPrintIntC(int input, Color color) {
    return MISL_GUEST_CALL(MISL_TL_PRINTINT_C, input, misl_guest_pack_color(color), 0, 0, 0, 0, 0);
}

static inline int tlPrintIntCP(int input, Color color, int column, int row) {
    return MISL_GUEST_CALL(MISL_TL_PRINTINT_CP, input, misl_guest_pack_color(color), column, row, 0, 0, 0);
}

static inline int tlNewLine(void) {
    return MISL_GUEST_CALL(MISL_TL_NEWLINE, 0, 0, 0, 0, 0, 0, 0);
}

static inline void tlCLS(void) {
    MISL_GUEST_CALL(MISL_TL_CLS, 0, 0, 0, 0, 0, 0, 0);
}

static inline void tlHome(void) {
    MISL_GUEST_CALL(MISL_TL_HOME, 0, 0, 0, 0, 0, 0, 0);
}

static inline void tlSetColor(Color color) {
    MISL_GUEST_CALL(MISL_TL_SET_COLOR, misl_guest_pack_color(color), 0, 0, 0, 0, 0, 0);
}

static inline Color tlGetColor(void) {
    return misl_guest_unpack_color(MISL_GUEST_CALL(MISL_TL_GET_COLOR, 0, 0, 0, 0, 0, 0, 0));
}

static inline void tlSetCursor(int column, int row) {
    MISL_GUEST_CALL(MISL_TL_SET_CURSOR, column, row, 0, 0, 0, 0, 0);
}

static inline int tlGetCursor(void) {
    return MISL_GUEST_CALL(MISL_TL_GET_CURSOR, 0, 0, 0, 0, 0, 0, 0);
}

static inline unsigned short tlGetCursorX(void) {
    return (unsigned short)MISL_GUEST_CALL(MISL_TL_GET_CURSOR_X, 0, 0, 0, 0, 0, 0, 0);
}

static inline unsigned short tlGetCursorY(void) {
    return (unsigned short)MISL_GUEST_CALL(MISL_TL_GET_CURSOR_Y, 0, 0, 0, 0, 0, 0, 0);
}

static inline unsigned short tlScrollUp(void) {
    return (unsigned short)MISL_GUEST_CALL(MISL_TL_SCROLL_UP, 0, 0, 0, 0, 0, 0, 0);
}

static inline bool tlActivate(void) {
    return MISL_GUEST_CALL(MISL_TL_ACTIVE, 0, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool tlDeactivate(void) {
    return MISL_GUEST_CALL(MISL_TL_INACTIVE, 0, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool tlToggle(void) {
    return MISL_GUEST_CALL(MISL_TL_TOGGLE, 0, 0, 0, 0, 0, 0, 0) != 0;
}

static inline unsigned char tlGetCellCharacter(int column, int row) {
    return (unsigned char)MISL_GUEST_CALL(MISL_TL_GET_CELL_CHAR, column, row, 0, 0, 0, 0, 0);
}

static inline Color tlGetCellColor(int column, int row) {
    return misl_guest_unpack_color(MISL_GUEST_CALL(MISL_TL_GET_CELL_COLOR, column, row, 0, 0, 0, 0, 0));
}

static inline bool tlStatus(void) {
    return MISL_GUEST_CALL(MISL_TL_STATUS, 0, 0, 0, 0, 0, 0, 0) != 0;
}
