#include "App.h"
#include <cstdio>
#include <cstring>
#include <iostream>

App::App() {}

App::~App() {}

void App::Init() {
  // --- Raylib window ---
  SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "VTuber Fire Head Tracker");
  SetTargetFPS(TARGET_FPS);

  // --- Particle system ---
  m_particles.LoadResources();
  m_particles.SetEmitterPosition(SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f);

  // --- FireHead ---
  m_fireHead.Init(SCREEN_WIDTH, SCREEN_HEIGHT);

  // --- Suit Texture ---
  m_suitTexture = LoadTexture("assets/suit.png");

  // --- IPC Control Panel ---
  m_sharedConfig.Init(true);
  LaunchControlPanel();

  // Mic init
  if (!m_mic.Start()) {
    std::cerr << "[App] WARNING: Failed to start microphone capture."
              << std::endl;
  }

  // --- Camera pixel buffer ---
  m_cameraPixels = new unsigned char[640 * 480 * 4]();

  // --- Face tracker ---
  if (!m_faceTracker.Init(0, "assets/models")) {
    std::cerr << "[App] WARNING: Face tracker initialization failed!"
              << std::endl;
    std::cerr << "[App] The fire will stay at screen center." << std::endl;
  } else {
    m_faceTracker.Start();
  }

  std::cout << "\n=== VTuber Fire Head Tracker ===" << std::endl;
  std::cout << "Controls:" << std::endl;
  std::cout << "  D     - Toggle debug overlay" << std::endl;
  std::cout << "  C     - Toggle camera background" << std::endl;
  std::cout << "  B     - Toggle dark/camera background" << std::endl;
  std::cout << "  M     - Toggle tracking mesh" << std::endl;
  std::cout << "  UP    - Increase camera opacity" << std::endl;
  std::cout << "  DOWN  - Decrease camera opacity" << std::endl;
  std::cout << "  ESC   - Exit" << std::endl;
  std::cout << "================================\n" << std::endl;
}

void App::UpdateCameraTexture() {
  if (!m_faceTracker.IsRunning() || !m_showCamera)
    return;

  int w, h;
  if (m_faceTracker.GetFrameRGBA(m_cameraPixels, w, h)) {
    if (!m_cameraTextureReady) {
      // First frame — create texture
      Image img = {0};
      img.data = m_cameraPixels;
      img.width = w;
      img.height = h;
      img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
      img.mipmaps = 1;

      m_cameraTexture = LoadTextureFromImage(img);
      SetTextureFilter(m_cameraTexture, TEXTURE_FILTER_BILINEAR);
      m_camWidth = w;
      m_camHeight = h;
      m_cameraTextureReady = true;
    } else {
      // Update existing texture with new pixel data
      UpdateTexture(m_cameraTexture, m_cameraPixels);
    }
  }
}

void App::Update(float dt) {
  // --- Input ---
  if (IsKeyPressed(KEY_D))
    m_showDebug = !m_showDebug;
  if (IsKeyPressed(KEY_C))
    m_showCamera = !m_showCamera;
  if (IsKeyPressed(KEY_B))
    m_darkMode = !m_darkMode;
  if (IsKeyPressed(KEY_M))
    m_drawMesh = !m_drawMesh;
  if (IsKeyDown(KEY_UP))
    m_cameraAlpha = std::min(1.0f, m_cameraAlpha + 0.5f * dt);
  if (IsKeyDown(KEY_DOWN))
    m_cameraAlpha = std::max(0.0f, m_cameraAlpha - 0.5f * dt);

  // Override with shared config if available
  bool isStreamMode = false;
  float fireScaleMulti = 1.0f;
  float suitOffsetY = 50.0f;
  if (m_sharedConfig.data) {
    isStreamMode = m_sharedConfig.data->streamMode;
    fireScaleMulti = m_sharedConfig.data->fireSizeMultiplier;
    m_particles.SetPhaseColors(
        {(unsigned char)m_sharedConfig.data->innerR, (unsigned char)m_sharedConfig.data->innerG, (unsigned char)m_sharedConfig.data->innerB, 255},
        {(unsigned char)m_sharedConfig.data->midR,   (unsigned char)m_sharedConfig.data->midG,   (unsigned char)m_sharedConfig.data->midB,   255},
        {(unsigned char)m_sharedConfig.data->outerR, (unsigned char)m_sharedConfig.data->outerG, (unsigned char)m_sharedConfig.data->outerB, 255}
    );
    m_particles.SetSeedShape(m_sharedConfig.data->seedShape);
    m_particles.SetCoreSizeMulti(m_sharedConfig.data->coreSizeMulti);
    m_particles.SetParticleSpeedMultiplier(
        m_sharedConfig.data->particleSpeedMulti);
    m_particles.SetParticleLifeMultiplier(
        m_sharedConfig.data->particleLifeMulti);
    suitOffsetY = m_sharedConfig.data->suitOffsetY;
  }

  // In stream mode, forcefully hide UI and camera to avoid showing real face
  if (isStreamMode) {
    m_showDebug = false;
    m_showCamera = false;
    m_drawMesh = false;
  }

  m_fireHead.SetDebugDraw(m_showDebug);

  // --- Update camera texture ---
  UpdateCameraTexture();

  // --- Get face data and update fire position ---
  FaceData face = m_faceTracker.GetFaceData();
  m_fireHead.Update(face, m_particles, dt);

  m_particles.SetSmokeMode(!face.detected);

  // --- Emotion-based fire color override ---
  if (face.detected && face.hasLandmarks) {
    // Smoothly transition toward detected emotion
    if (face.emotion != Emotion::NORMAL) {
      m_currentEmotion = face.emotion;
      m_emotionBlend += (face.emotionBlend - m_emotionBlend) * dt * 4.0f;
      m_emotionBlend = std::min(1.0f, m_emotionBlend);
    } else {
      // Fade back to normal
      m_emotionBlend -= dt * 2.0f;
      if (m_emotionBlend <= 0.0f) {
        m_emotionBlend = 0.0f;
        m_currentEmotion = Emotion::NORMAL;
      }
    }
  } else {
    // No face — fade to normal
    m_emotionBlend = std::max(0.0f, m_emotionBlend - dt * 2.0f);
  }

  // Apply emotion color override by blending with base colors
  if (m_emotionBlend > 0.01f) {
    // Emotion target colors
    unsigned char eInnerR, eInnerG, eInnerB;
    unsigned char eMidR, eMidG, eMidB;
    unsigned char eOuterR, eOuterG, eOuterB;

    switch (m_currentEmotion) {
      case Emotion::SURPRISED: // Blue
        eInnerR = 200; eInnerG = 230; eInnerB = 255;
        eMidR   =  60; eMidG   = 140; eMidB   = 255;
        eOuterR =  10; eOuterG =  40; eOuterB = 220;
        break;
      case Emotion::HAPPY: // Purple
        eInnerR = 230; eInnerG = 190; eInnerB = 255;
        eMidR   = 160; eMidG   =  60; eMidB   = 255;
        eOuterR = 100; eOuterG =  20; eOuterB = 200;
        break;
      default: // NORMAL — no override
        eInnerR = eInnerG = eInnerB = 0;
        eMidR = eMidG = eMidB = 0;
        eOuterR = eOuterG = eOuterB = 0;
        break;
    }

    if (m_currentEmotion != Emotion::NORMAL) {
      // Get current base colors from shared config (or defaults)
      unsigned char bInnerR = 255, bInnerG = 240, bInnerB = 180;
      unsigned char bMidR = 255, bMidG = 140, bMidB = 20;
      unsigned char bOuterR = 220, bOuterG = 30, bOuterB = 5;
      if (m_sharedConfig.data) {
        bInnerR = (unsigned char)m_sharedConfig.data->innerR;
        bInnerG = (unsigned char)m_sharedConfig.data->innerG;
        bInnerB = (unsigned char)m_sharedConfig.data->innerB;
        bMidR   = (unsigned char)m_sharedConfig.data->midR;
        bMidG   = (unsigned char)m_sharedConfig.data->midG;
        bMidB   = (unsigned char)m_sharedConfig.data->midB;
        bOuterR = (unsigned char)m_sharedConfig.data->outerR;
        bOuterG = (unsigned char)m_sharedConfig.data->outerG;
        bOuterB = (unsigned char)m_sharedConfig.data->outerB;
      }

      float t = m_emotionBlend;
      auto lerp = [](unsigned char a, unsigned char b, float t) -> unsigned char {
        return (unsigned char)(a + (int)(b - a) * t);
      };

      m_particles.SetPhaseColors(
        {lerp(bInnerR, eInnerR, t), lerp(bInnerG, eInnerG, t), lerp(bInnerB, eInnerB, t), 255},
        {lerp(bMidR, eMidR, t),     lerp(bMidG, eMidG, t),     lerp(bMidB, eMidB, t),     255},
        {lerp(bOuterR, eOuterR, t), lerp(bOuterG, eOuterG, t), lerp(bOuterB, eOuterB, t), 255}
      );
    }
  }

  // Apply fire scale override and microphone volume boost
  float micVol = m_mic.GetVolume();        // 0.0 to 1.0
  float voiceMulti = m_sharedConfig.data ? m_sharedConfig.data->voiceMultiplier : 3.0f;
  float micBoost = 1.0f + (micVol * voiceMulti);
  m_particles.SetFireScale(m_fireHead.GetFireScale() * fireScaleMulti *
                           micBoost);

  // Apply particle opacity and life ratios
  if (m_sharedConfig.data) {
    m_particles.SetParticleOpacity(m_sharedConfig.data->particleOpacity);
    m_particles.SetLifeRatios(m_sharedConfig.data->innerRatio, 
                              m_sharedConfig.data->midRatio, 
                              m_sharedConfig.data->outerRatio);
  }

  // --- Update particles ---
  m_particles.Update(dt);

  // --- Update suit sway ---
  if (face.detected && dt > 0.0f) {
    float velocity = (face.centerX - m_lastFaceX) / dt;
    float targetSway = std::max(
        -30.0f, std::min(30.0f, velocity * 15.0f)); // Clamp to -30..30 degrees
    m_suitSwayAngle += (targetSway - m_suitSwayAngle) * dt * 5.0f;
    m_lastFaceX = face.centerX;
  } else {
    m_suitSwayAngle += (0.0f - m_suitSwayAngle) * dt * 5.0f;
  }
}

void App::DrawUI(bool isStreamMode) {
  if (isStreamMode)
    return; // Do not draw FPS or Title in stream mode

  // Title bar
  DrawText("VTuber Fire Head", 10, 10, 24, {255, 200, 100, 200});

  // FPS counter
  char fpsBuf[64];
  snprintf(fpsBuf, sizeof(fpsBuf), "FPS: %d", GetFPS());
  DrawText(fpsBuf, SCREEN_WIDTH - 100, 10, 18, {200, 200, 200, 180});

  if (m_showDebug) {
    // Particle count
    char particleBuf[64];
    snprintf(particleBuf, sizeof(particleBuf), "Particles: %d",
             m_particles.GetAliveCount());
    DrawText(particleBuf, 10, 40, 16, {200, 200, 200, 180});

    // Camera alpha
    char alphaBuf[64];
    snprintf(alphaBuf, sizeof(alphaBuf), "Camera Alpha: %.2f", m_cameraAlpha);
    DrawText(alphaBuf, 10, 60, 16, {200, 200, 200, 180});

    // Fire scale
    char scaleBuf[64];
    snprintf(scaleBuf, sizeof(scaleBuf), "Fire Scale: %.2f",
             m_fireHead.GetFireScale());
    DrawText(scaleBuf, 10, 80, 16, {200, 200, 200, 180});

    // Detected emotion
    const char* emotionNames[] = {"Normal", "Surprised!", "Happy!"};
    Color emotionColors[] = {{200,200,200,200}, {100,180,255,255}, {200,150,255,255}};
    int eidx = (int)m_currentEmotion;
    char emotionBuf[64];
    snprintf(emotionBuf, sizeof(emotionBuf), "Emotion: %s (%.0f%%)",
             emotionNames[eidx], m_emotionBlend * 100.0f);
    DrawText(emotionBuf, 10, 100, 16, emotionColors[eidx]);

    // Controls reminder
    DrawText("[D] Debug  [C] Camera  [B] Dark BG  [UP/DOWN] Opacity", 10,
             SCREEN_HEIGHT - 30, 14, {150, 150, 150, 150});
  }
}

void App::Draw() {
  BeginDrawing();

  // --- Background ---
  if (m_sharedConfig.data) {
    ClearBackground({(unsigned char)m_sharedConfig.data->bgColorR, 
                     (unsigned char)m_sharedConfig.data->bgColorG, 
                     (unsigned char)m_sharedConfig.data->bgColorB, 255});
  } else {
    ClearBackground(BLACK); 
  }

  // --- Camera background ---
  if (m_showCamera && m_cameraTextureReady) {
    // Scale camera to fill the window while maintaining aspect ratio
    float scaleX = (float)SCREEN_WIDTH / m_camWidth;
    float scaleY = (float)SCREEN_HEIGHT / m_camHeight;
    float scale = std::max(scaleX, scaleY); // Fill (may crop)

    float drawW = m_camWidth * scale;
    float drawH = m_camHeight * scale;
    float drawX = (SCREEN_WIDTH - drawW) / 2.0f;
    float drawY = (SCREEN_HEIGHT - drawH) / 2.0f;

    unsigned char alpha = (unsigned char)(m_cameraAlpha * 255);
    DrawTextureEx(m_cameraTexture, {drawX, drawY}, 0.0f, scale,
                  {255, 255, 255, alpha});
  }

  // --- Draw suit ---
  if (m_suitTexture.id != 0) {
    float scale = m_fireHead.GetFireScale() * 0.8f; // Scale suit with fire
    Vector2 firePos = m_fireHead.GetFirePosition();

    float suitOffsetY = -50.0f;
    if (m_sharedConfig.data)
      suitOffsetY = m_sharedConfig.data->suitOffsetY;

    // Shrink source by 1 pixel to avoid edge bleed glitching
    Rectangle source = {1.0f, 1.0f, (float)m_suitTexture.width - 2.0f,
                        (float)m_suitTexture.height - 2.0f};
    // Position suit below the fire so the collar lines up
    Rectangle dest = {firePos.x, firePos.y + suitOffsetY * scale,
                      (float)m_suitTexture.width * scale,
                      (float)m_suitTexture.height * scale};
    // Rotate around top center (collar area)
    Vector2 origin = {(float)m_suitTexture.width * scale / 2.0f, 20.0f * scale};

    DrawTexturePro(m_suitTexture, source, dest, origin, m_suitSwayAngle, WHITE);
  }

  // --- Fire particle system ---
  m_particles.Draw();

  // --- Debug overlay ---
  FaceData face = m_faceTracker.GetFaceData();
  m_fireHead.DrawDebug(face, m_drawMesh);

  // --- UI ---
  bool isStreamMode = false;
  if (m_sharedConfig.data)
    isStreamMode = m_sharedConfig.data->streamMode;
  DrawUI(isStreamMode);

  EndDrawing();
}

void App::Shutdown() {
  m_faceTracker.Stop();
  m_mic.Stop();
  if (m_cameraPixels) {
    delete[] m_cameraPixels;
    m_cameraPixels = nullptr;
  }
  m_particles.UnloadResources();

  if (m_cameraTextureReady) {
    UnloadTexture(m_cameraTexture);
  }

  if (m_suitTexture.id != 0) {
    UnloadTexture(m_suitTexture);
  }

  CloseWindow();
}

void App::Run() {
  Init();

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();
    Update(dt);
    Draw();
  }

  Shutdown();
}
