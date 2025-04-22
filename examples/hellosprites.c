// Hello Sprites

#include "raylib.h"
#include "colors.h"
#include "textlayer.h"
#include "sprites.h"

#include "assets/player_ship.h"
#include "assets/collision.h"

#define FUSELAGE_PROGRESS_LEVEL "ANUS"
#define FUSELAGE_TESTPROGRAM_VERSION "0.1"
#define FUSELAGE_BUILD_DATE "250422"
#define TARGETFPS 60

void handleInput(void);
float GetGamepadLeftStickRotation(int gamepad);

// States
bool exitFlag = 0;
short fps = 0;
double scrollX = 0.0;
double scrollY = 0.0;
double scrollSpeed = 4.0;

// Player and NPC ship positions
Vector2 playerPos = { 608, 328 }; // Fixed position in the center of the screen
Vector2 npcShipPos = { 500, 500 }; // This position will update as the player moves

int WinMain() {

    const int screenWidth = 1280;
    const int screenHeight = 720;

    SetConfigFlags(FLAG_VSYNC_HINT);
    SetWindowSize(screenWidth, screenHeight);
    InitWindow(screenWidth, screenHeight, "Tile Layer Scrolling Test Program");
    SetExitKey(0);
    SetTargetFPS(TARGETFPS);

    // Init Fuselage components
    SetupCharacterMaps();
    InitSprites();
    InitPalettes();

    // Player Ship
    LoadPaletteFromSprite(10, player_ship_palette);
    SetSpriteColorPalette(10, 10);
    AssignSprite(10, player_ship);
    SetSpriteEnabled(10, true);
    SetSpriteVisible(10, true);
    UpdateSpritePosition(10, playerPos.x, playerPos.y);

    // NPC ship setup
    SetSpriteColorPalette(11, 10);
    AssignSprite(11, player_ship);
    SetSpriteEnabled(11, true);
    SetSpriteVisible(11, true);
    UpdateSpritePosition(11, npcShipPos.x, npcShipPos.y);

    // Collision text
    LoadPaletteFromSprite(20, collision_palette);
    SetSpriteColorPalette(20, 20);
    SetSpriteColorPalette(21, 20);
    AssignSprite(20, colli);
    AssignSprite(21, sion);
    SetSpriteEnabled(20, true);
    SetSpriteEnabled(21, true);
    SetSpriteVisible(20, false);
    SetSpriteVisible(21, false);
    UpdateSpritePosition(20, npcShipPos.x, npcShipPos.y);
    UpdateSpritePosition(21, npcShipPos.x, npcShipPos.y);

    while (!(exitFlag || WindowShouldClose())) {
        fps = GetFPS();
        tlCLS();
        tlPrintCP("Hello Sprites...", COLOR79, 1, 0);
        tlSetCursor(1, 44);
        tlPrintFormatted("%s - %s.%s", WHITE, FUSELAGE_PROGRESS_LEVEL, FUSELAGE_TESTPROGRAM_VERSION, FUSELAGE_BUILD_DATE);
        tlPrintCP("FPS:", RED, 72, 0); tlSetCursor(76, 0); tlPrintFormatted("%d", WHITE, fps);

        SetSpriteVisible(20, false);
        SetSpriteVisible(21, false);

        handleInput();

        float joyrot = GetGamepadLeftStickRotation(0);
        if (joyrot != -1) SetSpriteRotation(10, joyrot);

        if (scrollX || scrollY) {
            npcShipPos.x += (float) - scrollX;
            npcShipPos.y += (float) - scrollY;
            UpdateSpritePosition(11, npcShipPos.x, npcShipPos.y);
        }

        bool collisionDetected = CheckRotatedPixelCollision(10, 11);
        if (collisionDetected) {
            UpdateSpritePosition(20, npcShipPos.x - 32, npcShipPos.y + 64);
            UpdateSpritePosition(21, npcShipPos.x + 32, npcShipPos.y + 64);
            SetSpriteVisible(20, true);
            SetSpriteVisible(21, true);
        }

        BeginDrawing();
        ClearBackground(BLANK);
        DisplaySprites();
        DrawTextLayer();
        EndDrawing();
    }

    ShutdownCharacterMaps();
    ShutdownSprites();
    CloseWindow();
    return 0;
}

void handleInput(void) {
scrollX = 0.0;
scrollY = 0.0;

if (((IsKeyDown(KEY_RIGHT_ALT)) || (IsKeyDown(KEY_LEFT_ALT))) && (IsKeyPressed(KEY_ENTER))) ToggleFullscreen();
if (IsKeyPressed(KEY_ESCAPE)) exitFlag = 1;  // Quit on ESC
if (IsKeyPressed(KEY_ZERO)) TextLayerToggle();

if (IsGamepadAvailable(0)) {
float moveX = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
float moveY = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
if (fabs(moveX) > 0.2f) scrollX = moveX * scrollSpeed;
if (fabs(moveY) > 0.2f) scrollY = moveY * scrollSpeed;
}

if (IsKeyDown(KEY_RIGHT)) scrollX = 2.0;
if (IsKeyDown(KEY_LEFT)) scrollX = -2.0;
if (IsKeyDown(KEY_DOWN)) scrollY = 2.0;
if (IsKeyDown(KEY_UP)) scrollY = -2.0;

}

// Function to get the rotation angle of the left stick
float GetGamepadLeftStickRotation(int gamepad)
{
    float x = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_X);
    float y = -GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_Y);

    float deadzone = 0.2f;
    if (fabs(x) < deadzone && fabs(y) < deadzone)
        return -1.0f; // No significant movement

    float angle = atan2f(y, x) * (180.0f / PI);
    angle = 90.0f - angle;
    if (angle < 0.0f) angle += 360.0f;

    return angle;
}
