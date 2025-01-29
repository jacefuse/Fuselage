#include "colors.h"

bool LoadPaletteFromSprite(unsigned char paletteIndex, Color* spritepalette) {
    if (paletteIndex >= 256) {
        printf("Invalid palette index or palette size.\n");
        return false;
    }

    // We do not overwrite entry 0 of any palette, as it represents transparency for all sprites but a color index for Pixies.
    for (int i = 1; i < FUSELAGE_PALETTE_SIZE; i++) {
        Color testcolor = spritepalette[i];
        if (testcolor.r == Colors[paletteIndex][0].r && 
            testcolor.g == Colors[paletteIndex][0].g && 
            testcolor.b == Colors[paletteIndex][0].b && 
            testcolor.a == Colors[paletteIndex][0].a)
            Colors[paletteIndex][i] = BLANK;
        else Colors[paletteIndex][i] = spritepalette[i];
        //printf("COLOR[%u][%u]\n", paletteIndex, spritepalette[i]);
    }

    return true;
}

bool SetPalette(unsigned char paletteIndex, char colorIndex, Color color) {
    if (paletteIndex >= 256) {
        printf("Invalid palette index or palette size.\n");
        return false;
    }
    printf("Palette %d set to %d.\n", paletteIndex, colorIndex);
    Colors[paletteIndex][colorIndex] = color;

    return true;
}

void InitPalettes(void) {
    for (int i = 0; i < 255; i++) {
        for (int j = 0; j < 16; j++) {
            Colors[i][j] = GetColorFromChar(i);
            //printf("color[%d][%d] Set.\n", i, j);
        }
    }
}

Color GetColorFromChar(unsigned char input) {
    switch (input) {
    case 0: return COLOR0;
    case 1: return COLOR1;
    case 2: return COLOR2;
    case 3: return COLOR3;
    case 4: return COLOR4;
    case 5: return COLOR5;
    case 6: return COLOR6;
    case 7: return COLOR7;
    case 8: return COLOR8;
    case 9: return COLOR9;
    case 10: return COLOR10;
    case 11: return COLOR11;
    case 12: return COLOR12;
    case 13: return COLOR13;
    case 14: return COLOR14;
    case 15: return COLOR15;
    case 16: return COLOR16;
    case 17: return COLOR17;
    case 18: return COLOR18;
    case 19: return COLOR19;
    case 20: return COLOR20;
    case 21: return COLOR21;
    case 22: return COLOR22;
    case 23: return COLOR23;
    case 24: return COLOR24;
    case 25: return COLOR25;
    case 26: return COLOR26;
    case 27: return COLOR27;
    case 28: return COLOR28;
    case 29: return COLOR29;
    case 30: return COLOR30;
    case 31: return COLOR31;
    case 32: return COLOR32;
    case 33: return COLOR33;
    case 34: return COLOR34;
    case 35: return COLOR35;
    case 36: return COLOR36;
    case 37: return COLOR37;
    case 38: return COLOR38;
    case 39: return COLOR39;
    case 40: return COLOR40;
    case 41: return COLOR41;
    case 42: return COLOR42;
    case 43: return COLOR43;
    case 44: return COLOR44;
    case 45: return COLOR45;
    case 46: return COLOR46;
    case 47: return COLOR47;
    case 48: return COLOR48;
    case 49: return COLOR49;
    case 50: return COLOR50;
    case 51: return COLOR51;
    case 52: return COLOR52;
    case 53: return COLOR53;
    case 54: return COLOR54;
    case 55: return COLOR55;
    case 56: return COLOR56;
    case 57: return COLOR57;
    case 58: return COLOR58;
    case 59: return COLOR59;
    case 60: return COLOR60;
    case 61: return COLOR61;
    case 62: return COLOR62;
    case 63: return COLOR63;
    case 64: return COLOR64;
    case 65: return COLOR65;
    case 66: return COLOR66;
    case 67: return COLOR67;
    case 68: return COLOR68;
    case 69: return COLOR69;
    case 70: return COLOR70;
    case 71: return COLOR71;
    case 72: return COLOR72;
    case 73: return COLOR73;
    case 74: return COLOR74;
    case 75: return COLOR75;
    case 76: return COLOR76;
    case 77: return COLOR77;
    case 78: return COLOR78;
    case 79: return COLOR79;
    case 80: return COLOR80;
    case 81: return COLOR81;
    case 82: return COLOR82;
    case 83: return COLOR83;
    case 84: return COLOR84;
    case 85: return COLOR85;
    case 86: return COLOR86;
    case 87: return COLOR87;
    case 88: return COLOR88;
    case 89: return COLOR89;
    case 90: return COLOR90;
    case 91: return COLOR91;
    case 92: return COLOR92;
    case 93: return COLOR93;
    case 94: return COLOR94;
    case 95: return COLOR95;
    case 96: return COLOR96;
    case 97: return COLOR97;
    case 98: return COLOR98;
    case 99: return COLOR99;
    case 100: return COLOR100;
    case 101: return COLOR101;
    case 102: return COLOR102;
    case 103: return COLOR103;
    case 104: return COLOR104;
    case 105: return COLOR105;
    case 106: return COLOR106;
    case 107: return COLOR107;
    case 108: return COLOR108;
    case 109: return COLOR109;
    case 110: return COLOR110;
    case 111: return COLOR111;
    case 112: return COLOR112;
    case 113: return COLOR113;
    case 114: return COLOR114;
    case 115: return COLOR115;
    case 116: return COLOR116;
    case 117: return COLOR117;
    case 118: return COLOR118;
    case 119: return COLOR119;
    case 120: return COLOR120;
    case 121: return COLOR121;
    case 122: return COLOR122;
    case 123: return COLOR123;
    case 124: return COLOR124;
    case 125: return COLOR125;
    case 126: return COLOR126;
    case 127: return COLOR127;
    case 128: return COLOR128;
    case 129: return COLOR129;
    case 130: return COLOR130;
    case 131: return COLOR131;
    case 132: return COLOR132;
    case 133: return COLOR133;
    case 134: return COLOR134;
    case 135: return COLOR135;
    case 136: return COLOR136;
    case 137: return COLOR137;
    case 138: return COLOR138;
    case 139: return COLOR139;
    case 140: return COLOR140;
    case 141: return COLOR141;
    case 142: return COLOR142;
    case 143: return COLOR143;
    case 144: return COLOR144;
    case 145: return COLOR145;
    case 146: return COLOR146;
    case 147: return COLOR147;
    case 148: return COLOR148;
    case 149: return COLOR149;
    case 150: return COLOR150;
    case 151: return COLOR151;
    case 152: return COLOR152;
    case 153: return COLOR153;
    case 154: return COLOR154;
    case 155: return COLOR155;
    case 156: return COLOR156;
    case 157: return COLOR157;
    case 158: return COLOR158;
    case 159: return COLOR159;
    case 160: return COLOR160;
    case 161: return COLOR161;
    case 162: return COLOR162;
    case 163: return COLOR163;
    case 164: return COLOR164;
    case 165: return COLOR165;
    case 166: return COLOR166;
    case 167: return COLOR167;
    case 168: return COLOR168;
    case 169: return COLOR169;
    case 170: return COLOR170;
    case 171: return COLOR171;
    case 172: return COLOR172;
    case 173: return COLOR173;
    case 174: return COLOR174;
    case 175: return COLOR175;
    case 176: return COLOR176;
    case 177: return COLOR177;
    case 178: return COLOR178;
    case 179: return COLOR179;
    case 180: return COLOR180;
    case 181: return COLOR181;
    case 182: return COLOR182;
    case 183: return COLOR183;
    case 184: return COLOR184;
    case 185: return COLOR185;
    case 186: return COLOR186;
    case 187: return COLOR187;
    case 188: return COLOR188;
    case 189: return COLOR189;
    case 190: return COLOR190;
    case 191: return COLOR191;
    case 192: return COLOR192;
    case 193: return COLOR193;
    case 194: return COLOR194;
    case 195: return COLOR195;
    case 196: return COLOR196;
    case 197: return COLOR197;
    case 198: return COLOR198;
    case 199: return COLOR199;
    case 200: return COLOR200;
    case 201: return COLOR201;
    case 202: return COLOR202;
    case 203: return COLOR203;
    case 204: return COLOR204;
    case 205: return COLOR205;
    case 206: return COLOR206;
    case 207: return COLOR207;
    case 208: return COLOR208;
    case 209: return COLOR209;
    case 210: return COLOR210;
    case 211: return COLOR211;
    case 212: return COLOR212;
    case 213: return COLOR213;
    case 214: return COLOR214;
    case 215: return COLOR215;
    case 216: return COLOR216;
    case 217: return COLOR217;
    case 218: return COLOR218;
    case 219: return COLOR219;
    case 220: return COLOR220;
    case 221: return COLOR221;
    case 222: return COLOR222;
    case 223: return COLOR223;
    case 224: return COLOR224;
    case 225: return COLOR225;
    case 226: return COLOR226;
    case 227: return COLOR227;
    case 228: return COLOR228;
    case 229: return COLOR229;
    case 230: return COLOR230;
    case 231: return COLOR231;
    case 232: return COLOR232;
    case 233: return COLOR233;
    case 234: return COLOR234;
    case 235: return COLOR235;
    case 236: return COLOR236;
    case 237: return COLOR237;
    case 238: return COLOR238;
    case 239: return COLOR239;
    case 240: return COLOR240;
    case 241: return COLOR241;
    case 242: return COLOR242;
    case 243: return COLOR243;
    case 244: return COLOR244;
    case 245: return COLOR245;
    case 246: return COLOR246;
    case 247: return COLOR247;
    case 248: return COLOR248;
    case 249: return COLOR249;
    case 250: return COLOR250;
    case 251: return COLOR251;
    case 252: return COLOR252;
    case 253: return COLOR253;
    case 254: return COLOR254;
    case 255: return COLOR255;

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