#ifndef GDMF_TEXTLAYER_STATE_PRIVATE_H
#define GDMF_TEXTLAYER_STATE_PRIVATE_H
#include "gdmf_textlayer.h"

typedef struct {
    bool active;
    Color cursor_color;
    unsigned short cursor_column, cursor_row;
    unsigned char characters[TEXT_LAYER_WIDTH][TEXT_LAYER_HEIGHT];
    Color colors[TEXT_LAYER_WIDTH][TEXT_LAYER_HEIGHT];
} gdmf_textlayer_state;

// Borrowed live host state; serialize access with printing and renderer prepare.
gdmf_textlayer_state* gdmf_textlayer_state_get(void);
bool gdmf_textlayer_position_valid(int column, int row);
void gdmf_textlayer_place(unsigned char character, Color color, int column, int row);
#endif
