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
    
    // --- Emotion mapping ---
    particles.SetEmotionScore(faceData.emotionScore);
    
    // --- 3D Rotation (Pitch, Yaw, Roll) ---
    // Here we can use faceData.roll to tilt the fire's emission angle,
    // and faceData.pitch/yaw to skew the particles.
    // (Hook ready for when ParticleSystem supports directed emission)
}

// --- MediaPipe Face Mesh connection groups (landmark indices) ---
// Each array defines a connected polyline through landmark indices.

static const int FACE_OVAL[] = {
    10,338,297,332,284,251,389,356,454,323,361,288,397,365,379,378,
    400,377,152,148,176,149,150,136,172,58,132,93,234,127,162,21,
    54,103,67,109,10
};
static const int FACE_OVAL_LEN = sizeof(FACE_OVAL)/sizeof(int);

static const int LEFT_EYE[] = {33,7,163,144,145,153,154,155,133,173,157,158,159,160,161,246,33};
static const int LEFT_EYE_LEN = sizeof(LEFT_EYE)/sizeof(int);

static const int RIGHT_EYE[] = {362,382,381,380,374,373,390,249,263,466,388,387,386,385,384,398,362};
static const int RIGHT_EYE_LEN = sizeof(RIGHT_EYE)/sizeof(int);

static const int LEFT_BROW[] = {70,63,105,66,107,55,65,52,53,46};
static const int LEFT_BROW_LEN = sizeof(LEFT_BROW)/sizeof(int);

static const int RIGHT_BROW[] = {300,293,334,296,336,285,295,282,283,276};
static const int RIGHT_BROW_LEN = sizeof(RIGHT_BROW)/sizeof(int);

static const int LIPS_OUTER[] = {61,146,91,181,84,17,314,405,321,375,291,409,270,269,267,0,37,39,40,185,61};
static const int LIPS_OUTER_LEN = sizeof(LIPS_OUTER)/sizeof(int);

static const int LIPS_INNER[] = {78,191,80,81,82,13,312,311,310,415,308,324,318,402,317,14,87,178,88,95,78};
static const int LIPS_INNER_LEN = sizeof(LIPS_INNER)/sizeof(int);

static const int NOSE_BRIDGE[] = {168,6,197,195,5,4,1};
static const int NOSE_BRIDGE_LEN = sizeof(NOSE_BRIDGE)/sizeof(int);

static const int NOSE_BOTTOM[] = {98,240,64,48,115,220,45,4,275,440,344,278,294,460,327,98};
static const int NOSE_BOTTOM_LEN = sizeof(NOSE_BOTTOM)/sizeof(int);

// Helper to draw a polyline through landmark indices
static void DrawLandmarkPolyline(const FaceData& face, const int* indices, int count,
                                  int screenW, int screenH, Color color)
{
    for (int i = 0; i < count - 1; i++) {
        int a = indices[i], b = indices[i + 1];
        if (a >= face.landmarkCount || b >= face.landmarkCount) continue;
        float x1 = face.landmarks[a][0] * screenW;
        float y1 = face.landmarks[a][1] * screenH;
        float x2 = face.landmarks[b][0] * screenW;
        float y2 = face.landmarks[b][1] * screenH;
        DrawLine((int)x1, (int)y1, (int)x2, (int)y2, color);
    }
}

void FireHead::DrawDebug(const FaceData& faceData, bool drawMesh)
{
    if (!m_debugDraw && !drawMesh) return;

    if (faceData.detected) {
        float x1 = (faceData.centerX - faceData.width / 2.0f) * m_screenWidth;
        float y1 = (faceData.centerY - faceData.height / 2.0f) * m_screenHeight;
        float w  = faceData.width * m_screenWidth;
        float h  = faceData.height * m_screenHeight;

        if (m_debugDraw) {
            // Draw face bounding box
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

            // Head pose angles (show when landmarks are available)
            if (faceData.hasLandmarks) {
                char poseBuf[128];
                snprintf(poseBuf, sizeof(poseBuf), "Pitch: %.1f  Yaw: %.1f  Roll: %.1f",
                         faceData.pitch, faceData.yaw, faceData.roll);
                DrawText(poseBuf, 10, m_screenHeight - 40, 16, {100, 200, 255, 220});
            }
        }

        if (drawMesh) {
            if (faceData.hasLandmarks) {
                // --- Draw real MediaPipe face mesh ---
                Color meshColor = Fade(GREEN, 0.6f);
                Color eyeColor  = Fade({100, 255, 200, 255}, 0.7f);
                Color lipColor  = Fade({255, 100, 150, 255}, 0.7f);
                Color browColor = Fade({200, 200, 100, 255}, 0.6f);
                Color noseColor = Fade({150, 200, 255, 255}, 0.5f);

                DrawLandmarkPolyline(faceData, FACE_OVAL, FACE_OVAL_LEN, m_screenWidth, m_screenHeight, meshColor);
                DrawLandmarkPolyline(faceData, LEFT_EYE, LEFT_EYE_LEN, m_screenWidth, m_screenHeight, eyeColor);
                DrawLandmarkPolyline(faceData, RIGHT_EYE, RIGHT_EYE_LEN, m_screenWidth, m_screenHeight, eyeColor);
                DrawLandmarkPolyline(faceData, LEFT_BROW, LEFT_BROW_LEN, m_screenWidth, m_screenHeight, browColor);
                DrawLandmarkPolyline(faceData, RIGHT_BROW, RIGHT_BROW_LEN, m_screenWidth, m_screenHeight, browColor);
                DrawLandmarkPolyline(faceData, LIPS_OUTER, LIPS_OUTER_LEN, m_screenWidth, m_screenHeight, lipColor);
                DrawLandmarkPolyline(faceData, LIPS_INNER, LIPS_INNER_LEN, m_screenWidth, m_screenHeight, lipColor);
                DrawLandmarkPolyline(faceData, NOSE_BRIDGE, NOSE_BRIDGE_LEN, m_screenWidth, m_screenHeight, noseColor);
                DrawLandmarkPolyline(faceData, NOSE_BOTTOM, NOSE_BOTTOM_LEN, m_screenWidth, m_screenHeight, noseColor);

                // Draw small dots on key landmarks
                const int keyPoints[] = {1, 4, 5, 33, 133, 263, 362, 61, 291, 13, 14, 152, 10};
                for (int idx : keyPoints) {
                    if (idx < faceData.landmarkCount) {
                        float px = faceData.landmarks[idx][0] * m_screenWidth;
                        float py = faceData.landmarks[idx][1] * m_screenHeight;
                        DrawCircle((int)px, (int)py, 2, meshColor);
                    }
                }
            } else {
                // Fallback: fake wireframe grid (no landmark model loaded)
                int cols = 5;
                int rows = 5;
                for (int r = 0; r <= rows; r++) {
                    float y = y1 + h * ((float)r / rows);
                    DrawLine((int)x1, (int)y, (int)(x1 + w), (int)y, Fade(GREEN, 0.5f));
                }
                for (int c = 0; c <= cols; c++) {
                    float x = x1 + w * ((float)c / cols);
                    DrawLine((int)x, (int)y1, (int)x, (int)(y1 + h), Fade(GREEN, 0.5f));
                }
            }
        }
    } else if (m_debugDraw) {
        DrawText("No face detected", 10, m_screenHeight - 60, 16, RED);
    }
}
