#pragma once

#include "FaceTracker.h"
#include "ParticleSystem.h"
#include "FireHead.h"
#include "SharedConfig.h"
#include "MicInput.h"
#include "raylib.h"

/**
 * App — Main application controller.
 * 
 * Manages the Raylib window, composites the webcam background with
 * the fire particle overlay, and orchestrates the FaceTracker,
 * ParticleSystem, and FireHead components.
 */
class App {
public:
    App();
    ~App();

    /**
     * Run the main application loop.
     */
    void Run();

private:
    void Init();
    void Update(float dt);
    void Draw();
    void Shutdown();

    void UpdateCameraTexture();
    void DrawUI(bool isStreamMode);

    // Window settings
    static constexpr int SCREEN_WIDTH  = 800;
    static constexpr int SCREEN_HEIGHT = 600;
    static constexpr int TARGET_FPS    = 60;

    // Components
    FaceTracker    m_faceTracker;
    ParticleSystem m_particles{3000};
    FireHead       m_fireHead;
    MicInput       m_mic;

    // Camera background texture
    Texture2D      m_cameraTexture = {0};
    unsigned char* m_cameraPixels  = nullptr;
    bool           m_cameraTextureReady = false;
    int            m_camWidth  = 640;
    int            m_camHeight = 480;

    // UI state
    bool m_showDebug    = false;
    bool m_showCamera   = true;
    bool m_darkMode     = false;
    bool m_drawMesh     = false;
    float m_cameraAlpha = 0.4f; // Camera background transparency

    // Suit state
    Texture2D m_suitTexture = {0};
    float m_suitSwayAngle   = 0.0f;
    float m_lastFaceX       = 0.5f;

    // IPC
    SharedConfig m_sharedConfig;
};
