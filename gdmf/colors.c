#include "colors.h"
#include <stdio.h>

Color Colors[256][16];

uint32_t PackRGBA8(Color c) {
    return ((uint32_t)c.r) |
           ((uint32_t)c.g << 8) |
           ((uint32_t)c.b << 16) |
           ((uint32_t)c.a << 24);
}

bool LoadPaletteFromSprite(unsigned char paletteIndex, Color* spritepalette) {
    if (paletteIndex >= 255) {
        printf("Invalid palette index or palette size.\n");
        return false;
    }

    // We do not overwrite entry 0 of any palette, as it represents transparency for all sprites but a color index for Pixies.
    for (int i = 1; i < FUSELAGE_PALETTE_SIZE; i++) {
        Color testcolor = spritepalette[i];

        if (testcolor.r == Colors[paletteIndex][0].r &&
            testcolor.g == Colors[paletteIndex][0].g &&
            testcolor.b == Colors[paletteIndex][0].b &&
            testcolor.a == Colors[paletteIndex][0].a) { Colors[paletteIndex][i] = BLANK; }
        else { Colors[paletteIndex][i] = spritepalette[i]; }
        //printf("COLOR[%u][%u]\n", paletteIndex, spritepalette[i]);
    }

    return true;
}

bool SetPalette(unsigned char paletteIndex, unsigned char colorIndex, Color color) {
    if (paletteIndex >= 255) {
        printf("Invalid palette index or palette size.\n");
        return false;
    }
    //printf("Palette %d set to %d.\n", paletteIndex, colorIndex);
    Colors[paletteIndex][colorIndex] = color;

    return true;
}

Color GetPalette(unsigned char paletteIndex, unsigned char colorIndex) {
    if (colorIndex >= FUSELAGE_PALETTE_SIZE) {
        printf("Invalid color index.\n");
        return BLANK;
    }

    return Colors[paletteIndex][colorIndex];
}

void InitPalettes(void) {
    for (int i = 0; i < 255; i++) {
        for (int j = 0; j < 16; j++) {
            Colors[i][j] = GetColorFromChar(i);
            //printf("color[%d][%d] Set.\n", i, j);
        }
    }

    return;
}

Color GetColorFromChar(unsigned char input) {
    switch (input) {
    case 0: return GDMF_COLOR0;
    case 1: return GDMF_COLOR1;
    case 2: return GDMF_COLOR2;
    case 3: return GDMF_COLOR3;
    case 4: return GDMF_COLOR4;
    case 5: return GDMF_COLOR5;
    case 6: return GDMF_COLOR6;
    case 7: return GDMF_COLOR7;
    case 8: return GDMF_COLOR8;
    case 9: return GDMF_COLOR9;
    case 10: return GDMF_COLOR10;
    case 11: return GDMF_COLOR11;
    case 12: return GDMF_COLOR12;
    case 13: return GDMF_COLOR13;
    case 14: return GDMF_COLOR14;
    case 15: return GDMF_COLOR15;
    case 16: return GDMF_COLOR16;
    case 17: return GDMF_COLOR17;
    case 18: return GDMF_COLOR18;
    case 19: return GDMF_COLOR19;
    case 20: return GDMF_COLOR20;
    case 21: return GDMF_COLOR21;
    case 22: return GDMF_COLOR22;
    case 23: return GDMF_COLOR23;
    case 24: return GDMF_COLOR24;
    case 25: return GDMF_COLOR25;
    case 26: return GDMF_COLOR26;
    case 27: return GDMF_COLOR27;
    case 28: return GDMF_COLOR28;
    case 29: return GDMF_COLOR29;
    case 30: return GDMF_COLOR30;
    case 31: return GDMF_COLOR31;
    case 32: return GDMF_COLOR32;
    case 33: return GDMF_COLOR33;
    case 34: return GDMF_COLOR34;
    case 35: return GDMF_COLOR35;
    case 36: return GDMF_COLOR36;
    case 37: return GDMF_COLOR37;
    case 38: return GDMF_COLOR38;
    case 39: return GDMF_COLOR39;
    case 40: return GDMF_COLOR40;
    case 41: return GDMF_COLOR41;
    case 42: return GDMF_COLOR42;
    case 43: return GDMF_COLOR43;
    case 44: return GDMF_COLOR44;
    case 45: return GDMF_COLOR45;
    case 46: return GDMF_COLOR46;
    case 47: return GDMF_COLOR47;
    case 48: return GDMF_COLOR48;
    case 49: return GDMF_COLOR49;
    case 50: return GDMF_COLOR50;
    case 51: return GDMF_COLOR51;
    case 52: return GDMF_COLOR52;
    case 53: return GDMF_COLOR53;
    case 54: return GDMF_COLOR54;
    case 55: return GDMF_COLOR55;
    case 56: return GDMF_COLOR56;
    case 57: return GDMF_COLOR57;
    case 58: return GDMF_COLOR58;
    case 59: return GDMF_COLOR59;
    case 60: return GDMF_COLOR60;
    case 61: return GDMF_COLOR61;
    case 62: return GDMF_COLOR62;
    case 63: return GDMF_COLOR63;
    case 64: return GDMF_COLOR64;
    case 65: return GDMF_COLOR65;
    case 66: return GDMF_COLOR66;
    case 67: return GDMF_COLOR67;
    case 68: return GDMF_COLOR68;
    case 69: return GDMF_COLOR69;
    case 70: return GDMF_COLOR70;
    case 71: return GDMF_COLOR71;
    case 72: return GDMF_COLOR72;
    case 73: return GDMF_COLOR73;
    case 74: return GDMF_COLOR74;
    case 75: return GDMF_COLOR75;
    case 76: return GDMF_COLOR76;
    case 77: return GDMF_COLOR77;
    case 78: return GDMF_COLOR78;
    case 79: return GDMF_COLOR79;
    case 80: return GDMF_COLOR80;
    case 81: return GDMF_COLOR81;
    case 82: return GDMF_COLOR82;
    case 83: return GDMF_COLOR83;
    case 84: return GDMF_COLOR84;
    case 85: return GDMF_COLOR85;
    case 86: return GDMF_COLOR86;
    case 87: return GDMF_COLOR87;
    case 88: return GDMF_COLOR88;
    case 89: return GDMF_COLOR89;
    case 90: return GDMF_COLOR90;
    case 91: return GDMF_COLOR91;
    case 92: return GDMF_COLOR92;
    case 93: return GDMF_COLOR93;
    case 94: return GDMF_COLOR94;
    case 95: return GDMF_COLOR95;
    case 96: return GDMF_COLOR96;
    case 97: return GDMF_COLOR97;
    case 98: return GDMF_COLOR98;
    case 99: return GDMF_COLOR99;
    case 100: return GDMF_COLOR100;
    case 101: return GDMF_COLOR101;
    case 102: return GDMF_COLOR102;
    case 103: return GDMF_COLOR103;
    case 104: return GDMF_COLOR104;
    case 105: return GDMF_COLOR105;
    case 106: return GDMF_COLOR106;
    case 107: return GDMF_COLOR107;
    case 108: return GDMF_COLOR108;
    case 109: return GDMF_COLOR109;
    case 110: return GDMF_COLOR110;
    case 111: return GDMF_COLOR111;
    case 112: return GDMF_COLOR112;
    case 113: return GDMF_COLOR113;
    case 114: return GDMF_COLOR114;
    case 115: return GDMF_COLOR115;
    case 116: return GDMF_COLOR116;
    case 117: return GDMF_COLOR117;
    case 118: return GDMF_COLOR118;
    case 119: return GDMF_COLOR119;
    case 120: return GDMF_COLOR120;
    case 121: return GDMF_COLOR121;
    case 122: return GDMF_COLOR122;
    case 123: return GDMF_COLOR123;
    case 124: return GDMF_COLOR124;
    case 125: return GDMF_COLOR125;
    case 126: return GDMF_COLOR126;
    case 127: return GDMF_COLOR127;
    case 128: return GDMF_COLOR128;
    case 129: return GDMF_COLOR129;
    case 130: return GDMF_COLOR130;
    case 131: return GDMF_COLOR131;
    case 132: return GDMF_COLOR132;
    case 133: return GDMF_COLOR133;
    case 134: return GDMF_COLOR134;
    case 135: return GDMF_COLOR135;
    case 136: return GDMF_COLOR136;
    case 137: return GDMF_COLOR137;
    case 138: return GDMF_COLOR138;
    case 139: return GDMF_COLOR139;
    case 140: return GDMF_COLOR140;
    case 141: return GDMF_COLOR141;
    case 142: return GDMF_COLOR142;
    case 143: return GDMF_COLOR143;
    case 144: return GDMF_COLOR144;
    case 145: return GDMF_COLOR145;
    case 146: return GDMF_COLOR146;
    case 147: return GDMF_COLOR147;
    case 148: return GDMF_COLOR148;
    case 149: return GDMF_COLOR149;
    case 150: return GDMF_COLOR150;
    case 151: return GDMF_COLOR151;
    case 152: return GDMF_COLOR152;
    case 153: return GDMF_COLOR153;
    case 154: return GDMF_COLOR154;
    case 155: return GDMF_COLOR155;
    case 156: return GDMF_COLOR156;
    case 157: return GDMF_COLOR157;
    case 158: return GDMF_COLOR158;
    case 159: return GDMF_COLOR159;
    case 160: return GDMF_COLOR160;
    case 161: return GDMF_COLOR161;
    case 162: return GDMF_COLOR162;
    case 163: return GDMF_COLOR163;
    case 164: return GDMF_COLOR164;
    case 165: return GDMF_COLOR165;
    case 166: return GDMF_COLOR166;
    case 167: return GDMF_COLOR167;
    case 168: return GDMF_COLOR168;
    case 169: return GDMF_COLOR169;
    case 170: return GDMF_COLOR170;
    case 171: return GDMF_COLOR171;
    case 172: return GDMF_COLOR172;
    case 173: return GDMF_COLOR173;
    case 174: return GDMF_COLOR174;
    case 175: return GDMF_COLOR175;
    case 176: return GDMF_COLOR176;
    case 177: return GDMF_COLOR177;
    case 178: return GDMF_COLOR178;
    case 179: return GDMF_COLOR179;
    case 180: return GDMF_COLOR180;
    case 181: return GDMF_COLOR181;
    case 182: return GDMF_COLOR182;
    case 183: return GDMF_COLOR183;
    case 184: return GDMF_COLOR184;
    case 185: return GDMF_COLOR185;
    case 186: return GDMF_COLOR186;
    case 187: return GDMF_COLOR187;
    case 188: return GDMF_COLOR188;
    case 189: return GDMF_COLOR189;
    case 190: return GDMF_COLOR190;
    case 191: return GDMF_COLOR191;
    case 192: return GDMF_COLOR192;
    case 193: return GDMF_COLOR193;
    case 194: return GDMF_COLOR194;
    case 195: return GDMF_COLOR195;
    case 196: return GDMF_COLOR196;
    case 197: return GDMF_COLOR197;
    case 198: return GDMF_COLOR198;
    case 199: return GDMF_COLOR199;
    case 200: return GDMF_COLOR200;
    case 201: return GDMF_COLOR201;
    case 202: return GDMF_COLOR202;
    case 203: return GDMF_COLOR203;
    case 204: return GDMF_COLOR204;
    case 205: return GDMF_COLOR205;
    case 206: return GDMF_COLOR206;
    case 207: return GDMF_COLOR207;
    case 208: return GDMF_COLOR208;
    case 209: return GDMF_COLOR209;
    case 210: return GDMF_COLOR210;
    case 211: return GDMF_COLOR211;
    case 212: return GDMF_COLOR212;
    case 213: return GDMF_COLOR213;
    case 214: return GDMF_COLOR214;
    case 215: return GDMF_COLOR215;
    case 216: return GDMF_COLOR216;
    case 217: return GDMF_COLOR217;
    case 218: return GDMF_COLOR218;
    case 219: return GDMF_COLOR219;
    case 220: return GDMF_COLOR220;
    case 221: return GDMF_COLOR221;
    case 222: return GDMF_COLOR222;
    case 223: return GDMF_COLOR223;
    case 224: return GDMF_COLOR224;
    case 225: return GDMF_COLOR225;
    case 226: return GDMF_COLOR226;
    case 227: return GDMF_COLOR227;
    case 228: return GDMF_COLOR228;
    case 229: return GDMF_COLOR229;
    case 230: return GDMF_COLOR230;
    case 231: return GDMF_COLOR231;
    case 232: return GDMF_COLOR232;
    case 233: return GDMF_COLOR233;
    case 234: return GDMF_COLOR234;
    case 235: return GDMF_COLOR235;
    case 236: return GDMF_COLOR236;
    case 237: return GDMF_COLOR237;
    case 238: return GDMF_COLOR238;
    case 239: return GDMF_COLOR239;
    case 240: return GDMF_COLOR240;
    case 241: return GDMF_COLOR241;
    case 242: return GDMF_COLOR242;
    case 243: return GDMF_COLOR243;
    case 244: return GDMF_COLOR244;
    case 245: return GDMF_COLOR245;
    case 246: return GDMF_COLOR246;
    case 247: return GDMF_COLOR247;
    case 248: return GDMF_COLOR248;
    case 249: return GDMF_COLOR249;
    case 250: return GDMF_COLOR250;
    case 251: return GDMF_COLOR251;
    case 252: return GDMF_COLOR252;
    case 253: return GDMF_COLOR253;
    case 254: return GDMF_COLOR254;
    case 255: return GDMF_COLOR255;

    default: return (Color) { 0, 0, 0, 255 }; // Return black if the input is invalid
    }
}

Color GetCommodoreColor(unsigned char input) {
    switch (input) {
    case 0: return C64_COLOR00;
    case 1: return C64_COLOR01;
    case 2: return C64_COLOR02;
    case 3: return C64_COLOR03;
    case 4: return C64_COLOR04;
    case 5: return C64_COLOR05;
    case 6: return C64_COLOR06;
    case 7: return C64_COLOR07;
    case 8: return C64_COLOR08;
    case 9: return C64_COLOR09;
    case 10: return C64_COLOR10;
    case 11: return C64_COLOR11;
    case 12: return C64_COLOR12;
    case 13: return C64_COLOR13;
    case 14: return C64_COLOR14;
    case 15: return C64_COLOR15;

    default: return (Color) { 0, 0, 0, 255 }; // Return black if the input is invalid
    }
}

Color GetTandyColor(unsigned char input) {
    switch (input) {
    case 0: return TANDY_BLACK;
    case 1: return TANDY_BLUE;
    case 2: return TANDY_GREEN;
    case 3: return TANDY_CYAN;
    case 4: return TANDY_RED;
    case 5: return TANDY_MAGENTA;
    case 6: return TANDY_YELLOW;
    case 7: return TANDY_WHITE;
    case 8: return TANDY_BRIGHT_BLACK;
    case 9: return TANDY_BRIGHT_BLUE;
    case 10: return TANDY_BRIGHT_GREEN;
    case 11: return TANDY_BRIGHT_CYAN;
    case 12: return TANDY_BRIGHT_RED;
    case 13: return TANDY_BRIGHT_MAGENTA;
    case 14: return TANDY_BRIGHT_YELLOW;
    case 15: return TANDY_BRIGHT_WHITE;

    default: return (Color) { 0, 0, 0, 255 }; // Return black if the input is invalid
    }
}

Color GetANSIColor(unsigned char input) {
    switch (input) {
    case 0: return ANSI_BLACK;
    case 1: return ANSI_BLUE;
    case 2: return ANSI_GREEN;
    case 3: return ANSI_CYAN;
    case 4: return ANSI_RED;
    case 5: return ANSI_MAGENTA;
    case 6: return ANSI_YELLOW;
    case 7: return ANSI_WHITE;
    case 8: return ANSI_BRIGHT_BLACK;
    case 9: return ANSI_BRIGHT_BLUE;
    case 10: return ANSI_BRIGHT_GREEN;
    case 11: return ANSI_BRIGHT_CYAN;
    case 12: return ANSI_BRIGHT_RED;
    case 13: return ANSI_BRIGHT_MAGENTA;
    case 14: return ANSI_BRIGHT_YELLOW;
    case 15: return ANSI_BRIGHT_WHITE;

    default: return (Color) { 0, 0, 0, 255 }; // Return black if the input is invalid
    }
}