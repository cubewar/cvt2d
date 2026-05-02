#include "FireHead.h"
#include <cmath>
#include <algorithm>

// Lerp helper
static float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

FireHead::FireHead() {}

void FireHead::Init(int screenWidth, int screenHeight)
{
    m_screenWidth  = screenWidth;
    m_screenHeight = screenHeight;
    m_smoothX = screenWidth / 2.0f;
    m_smoothY = screenHeight / 2.0f;
}

void FireHead::Update(const FaceData& faceData, ParticleSystem& particles, float dt)
{
    if (!faceData.detected) {
        // No face detected — keep emitting at last known position
        // but slowly reduce emission rate
        particles.SetEmitterPosition(m_smoothX, m_smoothY);
        return;
    }

    // --- Convert normalized face coordinates to screen coordinates ---
    // The fire emitter is placed at the top of the head (above center)
    float targetX = faceData.centerX * m_screenWidth;
    float targetY = (faceData.centerY + m_verticalOffset) * m_screenHeight;

    // --- Depth-based scaling ---
    // Face width as a proxy for depth:
    //   faceData.width / m_referenceFaceWidth = scale factor
    //   > 1.0 means closer (bigger face), < 1.0 means further
    float depthScale = faceData.width / m_referenceFaceWidth;
    depthScale = std::max(0.3f, std::min(3.0f, depthScale)); // Clamp to reasonable range

    // --- Apply smoothing (exponential moving average) ---
    // Use frame-rate-independent smoothing
    float smoothFactor = 1.0f - powf(1.0f - m_smoothingFactor, dt * 60.0f);

    if (!m_hasInitialPosition) {
        // Snap to first detected position
        m_smoothX     = targetX;
        m_smoothY     = targetY;
        m_smoothScale = depthScale;
        m_hasInitialPosition = true;
    } else {
        m_smoothX     = Lerp(m_smoothX,     targetX,    smoothFactor);
        m_smoothY     = Lerp(m_smoothY,     targetY,    smoothFactor);
        m_smoothScale = Lerp(m_smoothScale,  depthScale, smoothFactor * 0.5f); // Scale changes more slowly
    }

    // --- Update particle system ---
    particles.SetEmitterPosition(m_smoothX, m_smoothY);
    particles.SetFireScale(m_smoothScale);
}

void FireHead::DrawDebug(const FaceData& faceData)
{
    if (!m_debugDraw) return;

    if (faceData.detected) {
        // Draw face bounding box
        float x1 = (faceData.centerX - faceData.width / 2.0f) * m_screenWidth;
        float y1 = (faceData.centerY - faceData.height / 2.0f) * m_screenHeight;
        float w  = faceData.width * m_screenWidth;
        float h  = faceData.height * m_screenHeight;

        DrawRectangleLines((int)x1, (int)y1, (int)w, (int)h, GREEN);

        // Draw face center
        DrawCircle((int)(faceData.centerX * m_screenWidth),
                   (int)(faceData.centerY * m_screenHeight),
                   4, GREEN);

        // Draw emitter position
        DrawCircle((int)m_smoothX, (int)m_smoothY, 6, RED);

        // Info text
        char buf[256];
        snprintf(buf, sizeof(buf), "Face: (%.2f, %.2f) Size: %.2f Scale: %.2f",
                 faceData.centerX, faceData.centerY, faceData.width, m_smoothScale);
        DrawText(buf, 10, m_screenHeight - 60, 16, GREEN);
    } else {
        DrawText("No face detected", 10, m_screenHeight - 60, 16, RED);
    }
}
