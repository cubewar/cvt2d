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
    DrawText(label, (int)rect.x, (int)rect.y - 18, 16, color);
    
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
    DrawText(TextFormat("%.2f", *value), (int)(rect.x + rect.width + 10), (int)(rect.y + rect.height/2 - 8), 16, DARKGRAY);
    
    return changed;
}

int main() {
    SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);
    InitWindow(1000, 900, "VTuber Control Panel"); // Wider window for more columns
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

        if (IsKeyPressed(KEY_ONE)) { 
            config.data->innerR = 255; config.data->innerG = 255; config.data->innerB = 255;
            config.data->midR   = 255; config.data->midG   = 150; config.data->midB   = 50;
            config.data->outerR = 255; config.data->outerG   = 30;  config.data->outerB  = 10;
        } // Normal
        if (IsKeyPressed(KEY_TWO)) { 
            config.data->innerR = 200; config.data->innerG = 255; config.data->innerB = 255;
            config.data->midR   = 50;  config.data->midG   = 150; config.data->midB   = 255;
            config.data->outerR = 0;   config.data->outerG   = 50;  config.data->outerB  = 255;
        } // Blue
        // -----------------------------------

        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("VTUBER CONTROL PANEL", 20, 20, 20, DARKGRAY);
        DrawText("--------------------", 20, 40, 20, DARKGRAY);

        // Stream Mode Button + Hotkey Info
        Color smColor = config.data->streamMode ? GREEN : RED;
        if (DoButton({20.0f, 70.0f, 150.0f, 30.0f}, "Stream Mode", smColor, WHITE)) {
            config.data->streamMode = !config.data->streamMode;
        }
        DrawText(TextFormat("[S] Status: %s", config.data->streamMode ? "ON" : "OFF"), 180, 75, 16, smColor);

        // Column 1: Core Params
        int startY = 130;
        int spacing = 35;
        int col1X = 20;

        DrawText("GLOBAL SETTINGS", col1X, startY - 25, 18, BLUE);
        DrawText(TextFormat("Fire Size: %.2fx", config.data->fireSizeMultiplier), col1X, startY + 5, 16, DARKGRAY);
        if (DoButton({(float)col1X + 220.0f, (float)startY, 30.0f, 30.0f}, "-")) config.data->fireSizeMultiplier = std::max(0.1f, config.data->fireSizeMultiplier - 0.1f);
        if (DoButton({(float)col1X + 260.0f, (float)startY, 30.0f, 30.0f}, "+")) config.data->fireSizeMultiplier = std::min(5.0f, config.data->fireSizeMultiplier + 0.1f);
        
        startY += spacing;
        DrawText(TextFormat("Particle Speed: %.2fx", config.data->particleSpeedMulti), col1X, startY + 5, 16, DARKGRAY);
        if (DoButton({(float)col1X + 220.0f, (float)startY, 30.0f, 30.0f}, "-")) config.data->particleSpeedMulti = std::max(0.1f, config.data->particleSpeedMulti - 0.1f);
        if (DoButton({(float)col1X + 260.0f, (float)startY, 30.0f, 30.0f}, "+")) config.data->particleSpeedMulti = std::min(5.0f, config.data->particleSpeedMulti + 0.1f);
        
        startY += spacing;
        DrawText(TextFormat("Disappear Time: %.2fx", config.data->particleLifeMulti), col1X, startY + 5, 16, DARKGRAY);
        if (DoButton({(float)col1X + 220.0f, (float)startY, 30.0f, 30.0f}, "-")) config.data->particleLifeMulti = std::max(0.1f, config.data->particleLifeMulti - 0.1f);
        if (DoButton({(float)col1X + 260.0f, (float)startY, 30.0f, 30.0f}, "+")) config.data->particleLifeMulti = std::min(5.0f, config.data->particleLifeMulti + 0.1f);
        
        startY += spacing;
        DrawText(TextFormat("Body Offset Y: %.1f", config.data->suitOffsetY), col1X, startY + 5, 16, DARKGRAY);
        if (DoButton({(float)col1X + 220.0f, (float)startY, 30.0f, 30.0f}, "-")) config.data->suitOffsetY -= 5.0f;
        if (DoButton({(float)col1X + 260.0f, (float)startY, 30.0f, 30.0f}, "+")) config.data->suitOffsetY += 5.0f;

        startY += 50;
        DoSliderFloat({(float)col1X, (float)startY, 300.0f, 15.0f}, &config.data->coreSizeMulti, 0.0f, 3.0f, "White Core Size", DARKGRAY);
        startY += 40;
        DoSliderFloat({(float)col1X, (float)startY, 300.0f, 15.0f}, &config.data->particleOpacity, 0.0f, 2.0f, "Particle Density (Blowout)", DARKGRAY);
        startY += 40;
        DoSliderFloat({(float)col1X, (float)startY, 300.0f, 15.0f}, &config.data->voiceMultiplier, 0.0f, 10.0f, "Mic Sensitivity", DARKGRAY);
        startY += 40;
        DoSliderFloat({(float)col1X, (float)startY, 300.0f, 15.0f}, &config.data->seedShape, 1.0f, 5.0f, "Seed Shape (Middle Elongation)", MAROON);

        startY += 50;
        DrawText("BACKGROUND COLOR", col1X, startY - 20, 18, PURPLE);
        DoSliderFloat({(float)col1X, (float)startY, 300.0f, 10.0f}, &config.data->bgColorR, 0.0f, 255.0f, "Back R", RED);
        startY += 30;
        DoSliderFloat({(float)col1X, (float)startY, 300.0f, 10.0f}, &config.data->bgColorG, 0.0f, 255.0f, "Back G", GREEN);
        startY += 30;
        DoSliderFloat({(float)col1X, (float)startY, 300.0f, 10.0f}, &config.data->bgColorB, 0.0f, 255.0f, "Back B", BLUE);

        // Column 2: Color Phases
        int col2X = 500;
        startY = 130;

        DrawText("FIRE COLOR PHASES", col2X, startY - 25, 18, RED);
        
        DrawText("INNER (Core)", col2X, startY, 16, DARKGRAY);
        startY += 25;
        DoSliderFloat({(float)col2X, (float)startY, 300.0f, 10.0f}, &config.data->innerR, 0.0f, 255.0f, "Inner R", RED);
        startY += 30;
        DoSliderFloat({(float)col2X, (float)startY, 300.0f, 10.0f}, &config.data->innerG, 0.0f, 255.0f, "Inner G", GREEN);
        startY += 30;
        DoSliderFloat({(float)col2X, (float)startY, 300.0f, 10.0f}, &config.data->innerB, 0.0f, 255.0f, "Inner B", BLUE);

        startY += 50;
        DrawText("MID (Flame)", col2X, startY, 16, DARKGRAY);
        startY += 25;
        DoSliderFloat({(float)col2X, (float)startY, 300.0f, 10.0f}, &config.data->midR, 0.0f, 255.0f, "Mid R", RED);
        startY += 30;
        DoSliderFloat({(float)col2X, (float)startY, 300.0f, 10.0f}, &config.data->midG, 0.0f, 255.0f, "Mid G", GREEN);
        startY += 30;
        DoSliderFloat({(float)col2X, (float)startY, 300.0f, 10.0f}, &config.data->midB, 0.0f, 255.0f, "Mid B", BLUE);

        startY += 50;
        DrawText("OUTER (Edges)", col2X, startY, 16, DARKGRAY);
        startY += 25;
        DoSliderFloat({(float)col2X, (float)startY, 300.0f, 10.0f}, &config.data->outerR, 0.0f, 255.0f, "Outer R", RED);
        startY += 30;
        DoSliderFloat({(float)col2X, (float)startY, 300.0f, 10.0f}, &config.data->outerG, 0.0f, 255.0f, "Outer G", GREEN);
        startY += 30;
        DoSliderFloat({(float)col2X, (float)startY, 300.0f, 10.0f}, &config.data->outerB, 0.0f, 255.0f, "Outer B", BLUE);

        startY += 60;
        DrawText("Thresholds (Life Ratio)", col2X, startY - 20, 18, DARKGRAY);
        DoSliderFloat({(float)col2X, (float)startY, 300.0f, 10.0f}, &config.data->innerRatio, 0.0f, 1.0f, "Inner Threshold", DARKGRAY);
        startY += 40;
        DoSliderFloat({(float)col2X, (float)startY, 300.0f, 10.0f}, &config.data->midRatio, 0.0f, 1.0f, "Mid Threshold", DARKGRAY);
        startY += 40;
        DoSliderFloat({(float)col2X, (float)startY, 300.0f, 10.0f}, &config.data->outerRatio, 0.0f, 1.0f, "Outer Threshold", DARKGRAY);

        DrawText("Presets:", col1X, 860, 16, DARKGRAY);
        if (DoButton({(float)col1X + 80.0f, 850.0f, 100.0f, 30.0f}, "Normal Fire")) { 
            config.data->innerR = 255; config.data->innerG = 255; config.data->innerB = 255;
            config.data->midR   = 255; config.data->midG   = 150; config.data->midB   = 50;
            config.data->outerR = 255; config.data->outerG   = 30;  config.data->outerB  = 10;
        }
        if (DoButton({(float)col1X + 200.0f, 850.0f, 100.0f, 30.0f}, "Blue Fire")) { 
            config.data->innerR = 200; config.data->innerG = 255; config.data->innerB = 255;
            config.data->midR   = 50;  config.data->midG   = 150; config.data->midB   = 255;
            config.data->outerR = 0;   config.data->outerG   = 50;  config.data->outerB  = 255;
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
