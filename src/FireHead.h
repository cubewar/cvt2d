#pragma once

#include "FaceTracker.h"
#include "ParticleSystem.h"

/**
 * FireHead — Maps face tracking data to fire particle emitter.
 * 
 * Applies smoothing (low-pass filter) to prevent jitter, converts
 * normalized face coordinates to screen coordinates, and uses face
 * bounding box size as a depth proxy for fire scale.
 */
class FireHead {
public:
    FireHead();
    ~FireHead() = default;

    /**
     * Initialize with screen dimensions.
     */
    void Init(int screenWidth, int screenHeight);

    /**
     * Update fire position and scale based on latest face data.
     * @param faceData    Latest face detection result
     * @param particles   Particle system to control
     * @param dt          Delta time in seconds
     */
    void Update(const FaceData& faceData, ParticleSystem& particles, float dt);

    /**
     * Draw any debug overlay (face bounding box, etc.)
     */
    void DrawDebug(const FaceData& faceData, bool drawMesh = false);

    /**
     * Get the smoothed fire position in screen coordinates.
     */
    Vector2 GetFirePosition() const { return {m_smoothX, m_smoothY}; }

    /**
     * Get the current fire scale factor.
     */
    float GetFireScale() const { return m_smoothScale; }

    /**
     * Toggle debug visualization.
     */
    void SetDebugDraw(bool enabled) { m_debugDraw = enabled; }
    bool IsDebugDraw() const { return m_debugDraw; }

    /**
     * Set the reference face width for depth calculation.
     * This is the normalized face width when the user is at "normal" distance.
     * Default: 0.25 (face occupies ~25% of frame width at normal distance)
     */
    void SetReferenceFaceWidth(float width) { m_referenceFaceWidth = width; }

    /**
     * Smoothing factor [0, 1]. Lower = smoother but more laggy.
     */
    void SetSmoothingFactor(float factor) { m_smoothingFactor = factor; }

private:
    int   m_screenWidth  = 800;
    int   m_screenHeight = 600;

    // Smoothed output values
    float m_smoothX     = 400.0f;
    float m_smoothY     = 300.0f;
    float m_smoothScale = 1.0f;

    // Smoothing parameters
    float m_smoothingFactor   = 0.15f;  // Lerp factor per frame
    float m_referenceFaceWidth = 0.25f; // Normalized face width at "normal" distance

    // Vertical offset: fire sits above the head
    float m_verticalOffset = -0.08f; // Normalized offset (negative = above)

    bool  m_debugDraw = false;
    bool  m_hasInitialPosition = false;
};
