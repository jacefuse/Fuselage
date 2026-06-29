// spriteconverter.c - Converts a 64x64, 15-color sprite image into a
// Fuselage sprite header: a packed bitmap plus a matching Color palette
// array, using the same name as the input file with the suffix _palette.
// Output files are 16 colors (more about Index 0 down below).
// This is done because the Fuselage color system always has a color
// defined in this index. This convention is being reconsidered.


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define IMAGE_WIDTH 64
#define IMAGE_HEIGHT 64

// How many bits each packed pixel uses. Only 4-bit (16 color) palettes are
// supported today, matching the original tool. A future version will let
// this vary from 1-8 bits per pixel (2-256 colors); the packing loop in
// RunConvert() below is the one place that will need to generalize beyond
// fixed nibble-packing when that lands.
#define COLOR_BITS 4
#define MAX_COLORS (1 << COLOR_BITS)
#define PIXEL_ARRAY_SIZE ((IMAGE_WIDTH * IMAGE_HEIGHT) * COLOR_BITS / 8)

// NOTE: Fuselage considers palette alpha values superfluous -- objects that
// need alpha carry it on their own struct, not via their palette -- and may
// drop the `a` field from palettes entirely once that's settled. Until then
// it stays here and in the generated output, since it's still in active use
// for transparency lookups (see GetColorIndex) and removing it now would be
// premature.
typedef struct {
    uint8_t r, g, b, a;
} Color;

float ColorIntensity(Color color) {
    return 0.2126f * color.r + 0.7152f * color.g + 0.0722f * color.b;
}

float ColorHue(Color color) {
    float r = color.r / 255.0f;
    float g = color.g / 255.0f;
    float b = color.b / 255.0f;

    float max = fmaxf(r, fmaxf(g, b));
    float min = fminf(r, fminf(g, b));
    float delta = max - min;

    if (delta == 0) {
        return 0; // Achromatic (grayscale)
    }

    float hue;

    if (max == r) {
        hue = 60.0f * fmodf(((g - b) / delta), 6.0f);
    } else if (max == g) {
        hue = 60.0f * (((b - r) / delta) + 2.0f);
    } else {
        hue = 60.0f * (((r - g) / delta) + 4.0f);
    }

    return (hue < 0) ? hue + 360.0f : hue;
}

int CompareColors(const void *a, const void *b) {
    const Color *colorA = (const Color *)a;
    const Color *colorB = (const Color *)b;

    float intensityA = ColorIntensity(*colorA);
    float intensityB = ColorIntensity(*colorB);

    if (intensityA < intensityB) {
        return -1;
    }

    if (intensityA > intensityB) {
        return 1;
    }

    float hueA = ColorHue(*colorA);
    float hueB = ColorHue(*colorB);

    if (hueA < hueB) {
        return -1;
    }

    if (hueA > hueB) {
        return 1;
    }

    return 0;
}

// Loads an image as a flat array of Color pixels. stb_image's 4-channel
// output is already laid out as interleaved r,g,b,a bytes, matching Color's
// layout exactly, so no per-pixel conversion is needed.
int LoadSpriteImage(const char *filename, Color **outPixels, int *width, int *height) {
    int channels;
    uint8_t *data = stbi_load(filename, width, height, &channels, 4);

    if (!data) {
        return 0;
    }

    size_t count = (size_t)(*width) * (size_t)(*height);
    *outPixels = malloc(count * sizeof(Color));

    if (!*outPixels) {
        stbi_image_free(data);
        return 0;
    }

    memcpy(*outPixels, data, count * sizeof(Color));
    stbi_image_free(data);

    return 1;
}

// Builds a sorted palette from the unique colors in pixels[]. Fully
// transparent pixels (alpha 0) always map to palette index 0 and are not
// counted as a distinct color. Returns the palette size, or -1 if the image
// has more than MAX_COLORS distinct (non-transparent) colors.
//
// Index 0 is hard-coded to {0,0,0,0} rather than left for the image's own
// colors: Fuselage's palette system reserves index 0 system-wide (tied to
// a still-unfinalized 256-color standard palette addressable by a single
// 8-bit number -- see GDMF/colors.h), not just "whatever this sprite's
// first color happens to be."
int CountUniqueColors(const Color *pixels, int pixelCount, Color *palette) {
    int uniqueColors = 0;

    palette[uniqueColors++] = (Color){ 0, 0, 0, 0 };

    for (int i = 0; i < pixelCount; i++) {
        if (pixels[i].a == 0) {
            continue;
        }

        int found = 0;

        for (int j = 1; j < uniqueColors; j++) {
            if (pixels[i].r == palette[j].r && pixels[i].g == palette[j].g && pixels[i].b == palette[j].b) {
                found = 1;
                break;
            }
        }

        if (!found) {
            if (uniqueColors >= MAX_COLORS) {
                return -1;
            }

            palette[uniqueColors++] = pixels[i];
        }
    }

    qsort(&palette[1], uniqueColors - 1, sizeof(Color), CompareColors);

    return uniqueColors;
}

// Confirms every non-transparent color in pixels[] is present in palette[].
int ValidateImageColors(const Color *pixels, int pixelCount, const Color *palette, int paletteSize) {
    for (int i = 0; i < pixelCount; i++) {
        if (pixels[i].a == 0) {
            continue;
        }

        int found = 0;

        for (int j = 0; j < paletteSize; j++) {
            if (pixels[i].r == palette[j].r && pixels[i].g == palette[j].g && pixels[i].b == palette[j].b) {
                found = 1;
                break;
            }
        }

        if (!found) {
            return -1;
        }
    }

    return 0;
}

// Any fully transparent pixel maps to index 0, regardless of its RGB value
// (the original tool only took this shortcut when RGB was also {0,0,0}, but
// every other path through it still landed on 0 for any a==0 pixel anyway,
// since palette index 0 is the only entry with alpha 0 and CountUniqueColors
// never adds another one). The matching loop below still compares all four
// components, matching the original: palette[0] is always {0,0,0,0}, so an
// *opaque* black pixel (0,0,0,255) must NOT match it on RGB alone, or it
// would be misclassified as transparent.
uint8_t GetColorIndex(Color color, const Color *palette, int paletteSize) {
    if (color.a == 0) {
        return 0;
    }

    for (int i = 0; i < paletteSize; i++) {
        if (color.r == palette[i].r && color.g == palette[i].g &&
            color.b == palette[i].b && color.a == palette[i].a) {
            return (uint8_t)i;
        }
    }

    return 0;
}

void WriteHeaderFile(const char *spriteName, const uint8_t *data, const Color *palette, int paletteSize) {
    char headerFilename[256];

    snprintf(headerFilename, sizeof(headerFilename), "%s.h", spriteName);

    FILE *file = fopen(headerFilename, "w");

    if (!file) {
        fprintf(stderr, "Error: unable to create header file '%s'.\n", headerFilename);
        return;
    }

    fprintf(file, "unsigned char %s[%d] = {", spriteName, PIXEL_ARRAY_SIZE);

    for (int i = 0; i < PIXEL_ARRAY_SIZE; i++) {
        if (i % 16 == 0) {
            fprintf(file, "\n    ");
        }

        fprintf(file, "0x%02X%s", data[i], (i < PIXEL_ARRAY_SIZE - 1) ? ", " : "");
    }

    fprintf(file, "\n};\n");

    fprintf(file, "\nColor %s_palette[] = {", spriteName);

    for (int i = 0; i < MAX_COLORS; i++) {
        if (i % 4 == 0) {
            fprintf(file, "\n    ");
        }

        Color c = (i < paletteSize) ? palette[i] : (Color){ 0, 0, 0, 0 };
        fprintf(file, "{ %d, %d, %d, %d }%s", c.r, c.g, c.b, c.a, (i < MAX_COLORS - 1) ? ", " : "");
    }

    fprintf(file, "\n};\n");

    fclose(file);

    printf("Header file '%s' created successfully.\n", headerFilename);

    return;
}

void GeneratePaletteImage(const char *outputFile, const Color *palette) {
    const int squareSize = 16;
    uint8_t image[IMAGE_WIDTH * IMAGE_HEIGHT * 4] = { 0 };

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            Color c = palette[y * 4 + x];

            for (int py = 0; py < squareSize; py++) {
                for (int px = 0; px < squareSize; px++) {
                    int imgX = x * squareSize + px;
                    int imgY = y * squareSize + py;
                    uint8_t *p = &image[(imgY * IMAGE_WIDTH + imgX) * 4];

                    p[0] = c.r;
                    p[1] = c.g;
                    p[2] = c.b;
                    p[3] = c.a;
                }
            }
        }
    }

    if (!stbi_write_png(outputFile, IMAGE_WIDTH, IMAGE_HEIGHT, 4, image, IMAGE_WIDTH * 4)) {
        fprintf(stderr, "Error: unable to write palette image '%s'.\n", outputFile);
        return;
    }

    printf("Palette file '%s' created successfully.\n", outputFile);

    return;
}

void PrintUsage(const char *prog) {
    printf("Usage:\n");
    printf("  %s <image_to_convert.png> [palette_image.png]\n", prog);
    printf("  %s --generate-palette <image.png> <output_palette.png>\n", prog);

    return;
}

int RunGeneratePalette(const char *inputFile, const char *outputFile) {
    Color *pixels;
    int width, height;

    if (!LoadSpriteImage(inputFile, &pixels, &width, &height)) {
        fprintf(stderr, "Error: could not load image '%s'.\n", inputFile);
        return 1;
    }

    Color palette[MAX_COLORS];
    int uniqueColors = CountUniqueColors(pixels, width * height, palette);

    free(pixels);

    if (uniqueColors == -1) {
        fprintf(stderr, "Error: image contains more than %d unique colors.\n", MAX_COLORS);
        return 1;
    }

    for (int i = uniqueColors; i < MAX_COLORS; i++) {
        palette[i] = (Color){ 0, 0, 0, 255 };
    }

    GeneratePaletteImage(outputFile, palette);

    return 0;
}

int RunConvert(const char *inputFile, const char *paletteFile) {
    char spriteName[256];

    snprintf(spriteName, sizeof(spriteName), "%s", inputFile);
    char *dot = strrchr(spriteName, '.');

    if (dot) {
        *dot = '\0';
    }

    Color *pixels;
    int width, height;

    if (!LoadSpriteImage(inputFile, &pixels, &width, &height)) {
        fprintf(stderr, "Error: could not load image '%s'.\n", inputFile);
        return 1;
    }

    if (width != IMAGE_WIDTH || height != IMAGE_HEIGHT) {
        fprintf(stderr, "Error: image must be %dx%d pixels.\n", IMAGE_WIDTH, IMAGE_HEIGHT);
        free(pixels);
        return 1;
    }

    Color palette[MAX_COLORS];
    int uniqueColors;

    if (paletteFile) {
        Color *palettePixels;
        int paletteWidth, paletteHeight;

        if (!LoadSpriteImage(paletteFile, &palettePixels, &paletteWidth, &paletteHeight)) {
            fprintf(stderr, "Error: could not load palette image '%s'.\n", paletteFile);
            free(pixels);
            return 1;
        }

        uniqueColors = CountUniqueColors(palettePixels, paletteWidth * paletteHeight, palette);
        free(palettePixels);

        if (uniqueColors == -1) {
            fprintf(stderr, "Error: palette image contains more than %d unique colors.\n", MAX_COLORS);
            free(pixels);
            return 1;
        }

        if (ValidateImageColors(pixels, width * height, palette, uniqueColors) != 0) {
            fprintf(stderr, "Error: image contains colors not found in the provided palette.\n");
            free(pixels);
            return 1;
        }
    } else {
        uniqueColors = CountUniqueColors(pixels, width * height, palette);

        if (uniqueColors == -1) {
            fprintf(stderr, "Error: image contains more than %d unique colors.\n", MAX_COLORS);
            free(pixels);
            return 1;
        }
    }

    // NOTE: this assumes 4-bit indices packed two-per-byte. When COLOR_BITS
    // becomes configurable (1-8 bits), this loop will need to generalize to
    // pack a variable number of bits per pixel instead of a fixed nibble.
    uint8_t data[PIXEL_ARRAY_SIZE] = { 0 };

    for (int i = 0, byteIndex = 0; i < width * height; i += 2, byteIndex++) {
        uint8_t color1 = GetColorIndex(pixels[i], palette, uniqueColors);
        uint8_t color2 = GetColorIndex(pixels[i + 1], palette, uniqueColors);

        data[byteIndex] = (uint8_t)((color1 << 4) | (color2 & 0x0F));
    }

    free(pixels);

    // NOTE: a C header is the only output format today. A future binary
    // object format (for use with the asset processing suite) will be
    // selected here via a new command-line option once it exists.
    WriteHeaderFile(spriteName, data, palette, uniqueColors);

    printf("Success: image processed and header file created.\n");

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--generate-palette") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Error: --generate-palette requires exactly two arguments.\n\n");
            PrintUsage(argv[0]);
            return 1;
        }

        return RunGeneratePalette(argv[2], argv[3]);
    }

    if (argc > 3) {
        fprintf(stderr, "Error: too many arguments.\n\n");
        PrintUsage(argv[0]);
        return 1;
    }

    return RunConvert(argv[1], (argc == 3) ? argv[2] : NULL);
}