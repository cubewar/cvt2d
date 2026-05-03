#include "raylib.h"
#include "SharedConfig.h"
#include <string>
#include <algorithm>

// Helper function to draw and handle a simple button
bool DoButton(Rectangle rect, const char* text, Color bg = LIGHTGRAY, Color fg = DARKGRAY) {
    bool hovered = CheckCollisionPointRec(GetMousePosition(), rect);
    bool clicked = hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    
    DrawRectangleRec(rect, hovered ? GRAY : bg);
    DrawRectangleLinesEx(rect, 1, DARKGRAY);
    
    int textW = MeasureText(text, 20);
    DrawText(text, rect.x + rect.width/2 - textW/2, rect.y + rect.height/2 - 10, 20, fg);
    
    return clicked;
}

int main() {
    SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);
    InitWindow(500, 450, "VTuber Control Panel");
    SetTargetFPS(60);

    SharedConfig config;
    if (!config.Init(false)) { // As client
        CloseWindow();
        return -1;
    }

    while (!WindowShouldClose()) {
        if (!config.data) break;

        // --- Hotkeys (Kept as requested) ---
        if (IsKeyPressed(KEY_S)) {
            config.data->streamMode = !config.data->streamMode;
        }

        if (IsKeyPressed(KEY_ONE)) config.data->colorMode = 0; // Emotion
        if (IsKeyPressed(KEY_TWO)) config.data->colorMode = 1; // Blue
        if (IsKeyPressed(KEY_THREE)) config.data->colorMode = 2; // Purple
        if (IsKeyPressed(KEY_FOUR)) config.data->colorMode = 3; // Red
        // -----------------------------------

        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("VTUBER CONTROL PANEL", 20, 20, 20, DARKGRAY);
        DrawText("--------------------", 20, 40, 20, DARKGRAY);

        // Stream Mode Button + Hotkey Info
        Color smColor = config.data->streamMode ? GREEN : RED;
        if (DoButton({20, 70, 150, 30}, "Stream Mode", smColor, WHITE)) {
            config.data->streamMode = !config.data->streamMode;
        }
        DrawText(TextFormat("[S] Status: %s", config.data->streamMode ? "ON" : "OFF"), 180, 75, 16, smColor);

        int startY = 130;
        int spacing = 50;

        // 1. Fire Size
        DrawText(TextFormat("Fire Size: %.2fx", config.data->fireSizeMultiplier), 20, startY + 5, 16, DARKGRAY);
        if (DoButton({220, (float)startY, 30, 30}, "-")) config.data->fireSizeMultiplier = std::max(0.1f, config.data->fireSizeMultiplier - 0.1f);
        if (DoButton({260, (float)startY, 30, 30}, "+")) config.data->fireSizeMultiplier = std::min(5.0f, config.data->fireSizeMultiplier + 0.1f);
        
        // 2. Particle Speed
        startY += spacing;
        DrawText(TextFormat("Particle Speed: %.2fx", config.data->particleSpeedMulti), 20, startY + 5, 16, DARKGRAY);
        if (DoButton({220, (float)startY, 30, 30}, "-")) config.data->particleSpeedMulti = std::max(0.1f, config.data->particleSpeedMulti - 0.1f);
        if (DoButton({260, (float)startY, 30, 30}, "+")) config.data->particleSpeedMulti = std::min(5.0f, config.data->particleSpeedMulti + 0.1f);
        
        // 3. Disappearing time (Life)
        startY += spacing;
        DrawText(TextFormat("Disappear Time: %.2fx", config.data->particleLifeMulti), 20, startY + 5, 16, DARKGRAY);
        if (DoButton({220, (float)startY, 30, 30}, "-")) config.data->particleLifeMulti = std::max(0.1f, config.data->particleLifeMulti - 0.1f);
        if (DoButton({260, (float)startY, 30, 30}, "+")) config.data->particleLifeMulti = std::min(5.0f, config.data->particleLifeMulti + 0.1f);
        
        // 4. Suit Offset
        startY += spacing;
        DrawText(TextFormat("Body Offset Y: %.1f", config.data->suitOffsetY), 20, startY + 5, 16, DARKGRAY);
        if (DoButton({220, (float)startY, 30, 30}, "-")) config.data->suitOffsetY -= 10.0f;
        if (DoButton({260, (float)startY, 30, 30}, "+")) config.data->suitOffsetY += 10.0f;

        // Colors (Buttons)
        startY += spacing + 10;
        DrawText("Colors [1-4]:", 20, startY + 5, 16, DARKGRAY);
        
        if (DoButton({150, (float)startY, 70, 30}, "Normal", config.data->colorMode == 0 ? BLUE : LIGHTGRAY, config.data->colorMode == 0 ? WHITE : DARKGRAY)) config.data->colorMode = 0;
        if (DoButton({230, (float)startY, 60, 30}, "Blue", config.data->colorMode == 1 ? BLUE : LIGHTGRAY, config.data->colorMode == 1 ? WHITE : DARKGRAY)) config.data->colorMode = 1;
        if (DoButton({300, (float)startY, 60, 30}, "Purple", config.data->colorMode == 2 ? BLUE : LIGHTGRAY, config.data->colorMode == 2 ? WHITE : DARKGRAY)) config.data->colorMode = 2;
        if (DoButton({370, (float)startY, 60, 30}, "Red", config.data->colorMode == 3 ? BLUE : LIGHTGRAY, config.data->colorMode == 3 ? WHITE : DARKGRAY)) config.data->colorMode = 3;
        
        DrawText("Ensure main VTuber window is running!", 20, 420, 12, GRAY);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
