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

// Helper function for a float slider
bool DoSliderFloat(Rectangle rect, float* value, float min, float max, const char* label, Color color) {
    bool changed = false;
    
    // Draw label
    DrawText(label, rect.x, rect.y - 18, 16, color);
    
    // Draw track
    DrawRectangleRec(rect, LIGHTGRAY);
    DrawRectangleLinesEx(rect, 1, DARKGRAY);
    
    // Handle input (make clickable area slightly larger)
    Vector2 mousePos = GetMousePosition();
    if (CheckCollisionPointRec(mousePos, {rect.x - 5, rect.y - 5, rect.width + 10, rect.height + 10})) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float normalized = (mousePos.x - rect.x) / rect.width;
            normalized = std::max(0.0f, std::min(1.0f, normalized));
            *value = min + normalized * (max - min);
            changed = true;
        }
    }
    
    // Draw knob
    float normalized = (*value - min) / (max - min);
    Rectangle knob = {rect.x + normalized * rect.width - 5, rect.y - 2, 10, rect.height + 4};
    DrawRectangleRec(knob, GRAY);
    DrawRectangleLinesEx(knob, 1, DARKGRAY);
    
    // Draw value text
    DrawText(TextFormat("%.2f", *value), rect.x + rect.width + 10, rect.y + rect.height/2 - 8, 16, DARKGRAY);
    
    return changed;
}

int main() {
    SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);
    InitWindow(500, 750, "VTuber Control Panel");
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

        if (IsKeyPressed(KEY_ONE)) { config.data->colorMode = 0; config.data->colorR = 255; config.data->colorG = 40; config.data->colorB = 10; } // Normal
        if (IsKeyPressed(KEY_TWO)) { config.data->colorMode = 1; config.data->colorR = 0; config.data->colorG = 50; config.data->colorB = 255; } // Blue
        if (IsKeyPressed(KEY_THREE)) { config.data->colorMode = 2; config.data->colorR = 150; config.data->colorG = 20; config.data->colorB = 255; } // Purple
        if (IsKeyPressed(KEY_FOUR)) { config.data->colorMode = 3; config.data->colorR = 255; config.data->colorG = 20; config.data->colorB = 10; } // Red
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

        int startY = 120;
        int spacing = 35;

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

        // 5. RGB Sliders
        startY += 50;
        DoSliderFloat({20, (float)startY, 300, 15}, &config.data->colorR, 0.0f, 255.0f, "Fire Color R", RED);
        
        startY += 40;
        DoSliderFloat({20, (float)startY, 300, 15}, &config.data->colorG, 0.0f, 255.0f, "Fire Color G", GREEN);

        startY += 40;
        DoSliderFloat({20, (float)startY, 300, 15}, &config.data->colorB, 0.0f, 255.0f, "Fire Color B", BLUE);

        // 6. Core Size & Particle Overlap Opacity
        startY += 50;
        DoSliderFloat({20, (float)startY, 300, 15}, &config.data->coreSizeMulti, 0.0f, 3.0f, "White Core Size (Inner glowing dot)", DARKGRAY);
        
        startY += 40;
        DoSliderFloat({20, (float)startY, 300, 15}, &config.data->particleOpacity, 0.0f, 2.0f, "Particle Overlap (Controls white blowout)", DARKGRAY);

        // 7. Voice Multiplier
        startY += 40;
        DoSliderFloat({20, (float)startY, 300, 15}, &config.data->voiceMultiplier, 0.0f, 10.0f, "Mic Voice Reaction (Multiplier)", DARKGRAY);

        // Colors (Presets)
        startY += 40;
        DrawText("Presets [1-4]:", 20, startY + 5, 16, DARKGRAY);
        
        if (DoButton({150, (float)startY, 70, 30}, "Normal", config.data->colorMode == 0 ? BLUE : LIGHTGRAY, config.data->colorMode == 0 ? WHITE : DARKGRAY)) { config.data->colorMode = 0; config.data->colorR = 255; config.data->colorG = 40; config.data->colorB = 10; }
        if (DoButton({230, (float)startY, 60, 30}, "Blue", config.data->colorMode == 1 ? BLUE : LIGHTGRAY, config.data->colorMode == 1 ? WHITE : DARKGRAY)) { config.data->colorMode = 1; config.data->colorR = 0; config.data->colorG = 50; config.data->colorB = 255; }
        if (DoButton({300, (float)startY, 60, 30}, "Purple", config.data->colorMode == 2 ? BLUE : LIGHTGRAY, config.data->colorMode == 2 ? WHITE : DARKGRAY)) { config.data->colorMode = 2; config.data->colorR = 150; config.data->colorG = 20; config.data->colorB = 255; }
        if (DoButton({370, (float)startY, 60, 30}, "Red", config.data->colorMode == 3 ? BLUE : LIGHTGRAY, config.data->colorMode == 3 ? WHITE : DARKGRAY)) { config.data->colorMode = 3; config.data->colorR = 255; config.data->colorG = 20; config.data->colorB = 10; }
        
        DrawText("Ensure main VTuber window is running!", 20, 710, 12, GRAY);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
