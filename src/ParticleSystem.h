#pragma once

#include "raylib.h"
#include <cmath>
#include <vector>

/**
 * ParticleSystem — 2D fire particle simulation.
 *
 * Pre-allocates a fixed pool of particles. The emitter spawns particles
 * that rise upward with turbulence, transitioning through a fire color
 * gradient (white core → yellow → orange → red → transparent) as they age.
 *
 * Rendered with additive blending for a glowing fire effect.
 */

struct Particle {
  Vector2 position = {0, 0};
  Vector2 velocity = {0, 0};
  float life = 0.0f; // Remaining life [0, maxLife]
  float maxLife = 1.0f;
  float size = 8.0f; // Radius
  float rotation = 0.0f;
  float rotSpeed = 0.0f;
  bool alive = false;
};

class ParticleSystem {
public:
  ParticleSystem(int maxParticles = 2000);
  ~ParticleSystem() = default;

  /**
   * Load the particle glow texture (soft radial gradient).
   * Must be called after Raylib InitWindow.
   */
  void LoadResources();

  /**
   * Unload textures. Call before Raylib CloseWindow.
   */
  void UnloadResources();

  /**
   * Set emitter position in screen coordinates.
   */
  void SetEmitterPosition(float x, float y);

  /**
   * Set the fire scale (1.0 = normal, >1 = bigger fire = closer).
   */
  void SetFireScale(float scale);

  /**
   * Set the emotion score to change the fire color (0.0 = chill, 1.0 =
   * excited).
   */
  void SetEmotionScore(float score);

  /**
   * Set RGB colors for each fire phase.
   */
  void SetPhaseColors(Color inner, Color mid, Color outer);

  /**
   * Set how much the middle particles are elongated (seed shape).
   */
  void SetSeedShape(float strength);

  /**
   * Set the size multiplier for the white inner core.
   */
  void SetCoreSizeMulti(float multi);

  /**
   * Set the master particle opacity multiplier.
   */
  void SetParticleOpacity(float opacity);

  /**
   * Set the life ratio thresholds for the inner, mid, and outer fire phases.
   */
  void SetLifeRatios(float inner, float mid, float outer);

  /**
   * Set particle speed multiplier.
   */
  void SetParticleSpeedMultiplier(float multi);

  /**
   * Set particle life (disappear time) multiplier.
   */
  void SetParticleLifeMultiplier(float multi);

  /**
   * Enable or disable smoke mode (used when no face is detected).
   */
  void SetSmokeMode(bool smoke);

  /**
   * Set how many particles to emit per second.
   */
  void SetEmissionRate(float particlesPerSecond);

  /**
   * Update all particles (physics, lifecycle) and emit new ones.
   * @param dt  Delta time in seconds
   */
  void Update(float dt);

  /**
   * Draw all alive particles with additive blending.
   */
  void Draw();

  /**
   * Get the current number of alive particles (for debug).
   */
  int GetAliveCount() const;

private:
  int FindDeadParticle();
  void EmitParticle();
  Color GetFireColor(float distRatio, float lifeRatio) const;

  std::vector<Particle> m_particles;
  int m_maxParticles;

  // Emitter state
  float m_emitterX = 400.0f;
  float m_emitterY = 300.0f;
  float m_fireScale = 1.0f;
  float m_emotionScore = 0.5f;
  unsigned char m_innerR = 255, m_innerG = 255, m_innerB = 255;
  unsigned char m_midR   = 255, m_midG   = 150, m_midB   = 50;
  unsigned char m_outerR = 255, m_outerG = 50,  m_outerB = 20;

  float m_coreSizeMulti = 1.0f;
  float m_particleOpacity = 1.0f;
  float m_innerRatio = 0.85f;
  float m_midRatio = 0.6f;
  float m_outerRatio = 0.15f;
  bool m_isSmoke = false;
  float m_smokeBlend = 0.0f;
  float m_speedMulti = 1.0f;
  float m_lifeMulti = 1.0f;
  float m_emissionRate = 800.0f; // particles/sec
  float m_emissionAccum = 0.0f;  // Fractional particle accumulator

  // Particle shape tuning
  float m_seedShapeMulti = 1.5f; // How much longer center particles live
  
  // Particle spawn parameters (base values, scaled by m_fireScale)
  float m_baseSpeedMin = 80.0f;
  float m_baseSpeedMax = 200.0f;
  float m_baseSizeMin = 4.0f;
  float m_baseSizeMax = 18.0f;
  float m_baseLifeMin = 0.3f;
  float m_baseLifeMax = 1.2f;
  float m_spreadAngle = 80.0f; // Degrees, total cone width
  float m_turbulence = 120.0f; // Horizontal sway force

  // Glow texture
  Texture2D m_glowTexture = {0};
  bool m_textureLoaded = false;

  // Internal index for round-robin particle search
  int m_lastUsedParticle = 0;
};
