// SC.exe is a command line tool intended to turn a 64x64 by 16 color image
// into a header file for use with the sprite system of Fuselage.
//
// The header will contain a packed bitmap with a matching palette using the
// same name plus the suffice _palette.

#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_COLORS 16
#define IMAGE_HEIGHT 64
#define IMAGE_WIDTH 64
#define PIXEL_ARRAY_SIZE ((IMAGE_HEIGHT * IMAGE_WIDTH) / 2) // 2048 bytes for 64x64 image

// Function to calculate color intensity
float ColorIntensity(Color color) {
    return 0.2126f * color.r + 0.7152f * color.g + 0.0722f * color.b;
}

// Function to calculate the hue of a color (normalized to 0–360)
float ColorHue(Color color) {
    float r = color.r / 255.0f;
    float g = color.g / 255.0f;
    float b = color.b / 255.0f;

    float max = fmaxf(r, fmaxf(g, b));
    float min = fminf(r, fminf(g, b));
    float delta = max - min;

    if (delta == 0) return 0; // Achromatic (grayscale)

    float hue;
    if (max == r) {
        hue = 60.0f * fmodf(((g - b) / delta), 6.0f);
    } else if (max == g) {
        hue = 60.0f * (((b - r) / delta) + 2.0f);
    } else {
        hue = 60.0f * (((r - g) / delta) + 4.0f);
    }

    return (hue < 0) ? hue + 360.0f : hue; // Ensure hue is positive
}

// Comparator for sorting colors
int CompareColors(const void *a, const void *b) {
    const Color *colorA = (const Color *)a;
    const Color *colorB = (const Color *)b;

    float intensityA = ColorIntensity(*colorA);
    float intensityB = ColorIntensity(*colorB);

    if (intensityA < intensityB) return -1;
    if (intensityA > intensityB) return 1;

    // Secondary sorting by hue
    float hueA = ColorHue(*colorA);
    float hueB = ColorHue(*colorB);

    if (hueA < hueB) return -1;
    if (hueA > hueB) return 1;

    return 0; // Equal colors
}

// Function to count unique colors in the image and build the palette
int CountUniqueColors(Image* image, Color* palette) {
    Color* pixels = LoadImageColors(*image);
    int width = image->width;
    int height = image->height;
    int uniqueColors = 0;

    // Initialize register 0 to {0, 0, 0, 0}
    palette[uniqueColors++] = (Color){ 0, 0, 0, 0 };

    for (int i = 0; i < width * height; i++) {
        // Map all transparent pixels to color 0
        if (pixels[i].a == 0) continue;

        int found = 0;
        for (int j = 1; j < uniqueColors; j++) { // Start from index 1
            if (pixels[i].r == palette[j].r &&
                pixels[i].g == palette[j].g &&
                pixels[i].b == palette[j].b) {
                found = 1;
                break;
            }
        }
        if (!found) {
            if (uniqueColors >= MAX_COLORS) {
                UnloadImageColors(pixels);
                return -1; // Too many unique colors
            }
            palette[uniqueColors++] = pixels[i];
        }
    }

    // Sort the palette from index 1 to uniqueColors
    qsort(&palette[1], uniqueColors - 1, sizeof(Color), CompareColors);

    UnloadImageColors(pixels);
    return uniqueColors;
}

// Function to validate if all colors in the image exist in the provided palette
int ValidateImageColors(Image* image, Color* palette, int paletteSize) {
    Color* pixels = LoadImageColors(*image);
    int width = image->width;
    int height = image->height;

    for (int i = 0; i < width * height; i++) {
        // Map all transparent pixels to color 0
        if (pixels[i].a == 0) continue;

        int found = 0;
        for (int j = 0; j < paletteSize; j++) {
            if (pixels[i].r == palette[j].r &&
                pixels[i].g == palette[j].g &&
                pixels[i].b == palette[j].b) {
                found = 1;
                break;
            }
        }
        if (!found) {
            UnloadImageColors(pixels);
            return -1; // Color not found in palette
        }
    }

    UnloadImageColors(pixels);
    return 0; // All colors validated
}

unsigned char GetColorIndex(Color color, Color* palette, int paletteSize) {
    // Check for fully transparent color
    if (color.a == 0 && color.r == 0 && color.g == 0 && color.b == 0) {
        return 0; // Fully transparent maps to color 0
    }

    // Match opaque colors, including solid black
    for (int i = 0; i < paletteSize; i++) {
        if (color.r == palette[i].r &&
            color.g == palette[i].g &&
            color.b == palette[i].b &&
            color.a == palette[i].a) {
            return i;
        }
    }
    return 0; // Default to 0 if not found (shouldn't happen in valid images)
}

// Function to write pixel data and palette to a header file
void WriteHeaderFile(const char* filename, unsigned char* data, Color* palette, int paletteSize) {
    char headerFilename[256];
    int size = PIXEL_ARRAY_SIZE;

    snprintf(headerFilename, sizeof(headerFilename), "%s.h", filename);

    FILE* file = fopen(headerFilename, "w");
    if (file == NULL) {
        printf("Error: Unable to create header file.\n");
        return;
    }

    // Write pixel data array
    fprintf(file, "unsigned char %s[%d] = {", filename, size);
    for (int i = 0; i < PIXEL_ARRAY_SIZE; i++) {
        if (i % 16 == 0) fprintf(file, "\n    ");
        fprintf(file, "0x%02X%s", data[i], (i < PIXEL_ARRAY_SIZE - 1) ? ", " : "");
    }
    fprintf(file, "\n};\n");

    // Write color palette array
    fprintf(file, "\nColor %s_palette[] = {", filename);
    for (int i = 0; i < MAX_COLORS; i++) {
        if (i % 4 == 0) fprintf(file, "\n    ");
        if (i < paletteSize) {
            fprintf(file, "{ %d, %d, %d, %d }%s",
                palette[i].r, palette[i].g, palette[i].b, palette[i].a,
                (i < MAX_COLORS - 1) ? ", " : "");
        }
        else {
            // Pad remaining entries with { 0, 0, 0, 0 }
            fprintf(file, "{ 0, 0, 0, 0 }%s",
                (i < MAX_COLORS - 1) ? ", " : "");
        }
    }
    fprintf(file, "\n};\n");

    fclose(file);

    printf("Header file '%s' created successfully.\n", headerFilename);
}

// Function to generate a palette image
void GeneratePaletteImage(const char *outputFile, Color *palette) {
    Image paletteImage = GenImageColor(64, 64, BLANK); // Create a blank image
    int squareSize = 16; // 64x64 image with a 4x4 grid of 16 squares

    // Draw each color in a 4x4 grid
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int index = y * 4 + x; // Palette index
            if (index < MAX_COLORS) {
                Rectangle rect = { x * squareSize, y * squareSize, squareSize, squareSize };
                ImageDrawRectangle(&paletteImage, rect.x, rect.y, rect.width, rect.height, palette[index]);
            }
        }
    }

    ExportImage(paletteImage, outputFile); // Save the image
    UnloadImage(paletteImage);

    printf("Palette file '%s' created successfully.\n", outputFile);
}

// Main function
int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 4) {
        printf("Usage:\n");
        printf("  %s <image_to_convert.png> [palette_image.png]\n", argv[0]);
        printf("  %s --generate-palette <image.png> <output_palette.png>\n", argv[0]);
        return 1;
    }

    if (argc == 4 && strcmp(argv[1], "--generate-palette") == 0) {
        const char *inputFile = argv[2];
        const char *outputFile = argv[3];

        // Load image
        Image image = LoadImage(inputFile);
        if (image.data == NULL) {
            printf("Error: Could not load image '%s'.\n", inputFile);
            return 1;
        }

        // Build and sort palette
        Color palette[MAX_COLORS];
        int uniqueColors = CountUniqueColors(&image, palette);
        if (uniqueColors == -1) {
            printf("Error: Image contains more than %d unique colors.\n", MAX_COLORS);
            UnloadImage(image);
            return 1;
        }

        // Ensure we have exactly 16 colors in the palette
        for (int i = uniqueColors; i < MAX_COLORS; i++) {
            palette[i] = (Color){0, 0, 0, 255}; // Fill unused slots with black
        }

        // Generate palette image
        GeneratePaletteImage(outputFile, palette);

        UnloadImage(image);
        return 0;
    }

    // Conversion functionality
    const char *inputFile = argv[1];
    const char *paletteFile = (argc == 3) ? argv[2] : NULL;

    char imageName[256];
    snprintf(imageName, sizeof(imageName), "%s", argv[1]);
    char *dot = strrchr(imageName, '.');
    if (dot) *dot = '\0'; // Remove file extension

    // Load image to convert
    Image image = LoadImage(inputFile);
    if (image.data == NULL) {
        printf("Error: Could not load image '%s'.\n", inputFile);
        return 1;
    }

    // Check image size
    if (image.width != IMAGE_WIDTH || image.height != IMAGE_HEIGHT) {
        printf("Error: Image must be %dx%d pixels.\n", IMAGE_WIDTH, IMAGE_HEIGHT);
        UnloadImage(image);
        return 1;
    }

    Color palette[MAX_COLORS];
    int uniqueColors;

    if (paletteFile) {
        // Load palette image
        Image paletteImage = LoadImage(paletteFile);
        if (paletteImage.data == NULL) {
            printf("Error: Could not load palette image '%s'.\n", paletteFile);
            UnloadImage(image);
            return 1;
        }

        // Build palette from palette image
        uniqueColors = CountUniqueColors(&paletteImage, palette);
        if (uniqueColors == -1) {
            printf("Error: Palette image contains more than %d unique colors.\n", MAX_COLORS);
            UnloadImage(image);
            UnloadImage(paletteImage);
            return 1;
        }

        UnloadImage(paletteImage);

        // Validate image against palette
        if (ValidateImageColors(&image, palette, uniqueColors) != 0) {
            printf("Error: Image contains colors not found in the provided palette.\n");
            UnloadImage(image);
            return 1;
        }
    } else {
        // Build and sort palette from the image itself
        uniqueColors = CountUniqueColors(&image, palette);
        if (uniqueColors == -1) {
            printf("Error: Image contains more than %d unique colors.\n", MAX_COLORS);
            UnloadImage(image);
            return 1;
        }
    }

    // Convert image to 4-bit packed hex array
    unsigned char data[PIXEL_ARRAY_SIZE] = {0};
    Color *pixels = LoadImageColors(image);

    for (int i = 0, byteIndex = 0; i < image.width * image.height; i += 2, byteIndex++) {
        unsigned char color1 = GetColorIndex(pixels[i], palette, uniqueColors);
        unsigned char color2 = GetColorIndex(pixels[i + 1], palette, uniqueColors);
        data[byteIndex] = (color1 << 4) | (color2 & 0x0F);
    }
    UnloadImageColors(pixels);

    // Write to header file
    WriteHeaderFile(imageName, data, palette, uniqueColors);

    // Cleanup
    UnloadImage(image);

    printf("Success: Image processed and header file created.\n");
    return 0;
}
