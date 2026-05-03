#include "ParticleSystem.h"
#include "rlgl.h"
#include <cstdlib>
#include <cstring>
#include <algorithm>

// --- Helper: random float in [min, max] ---
static float RandFloat(float min, float max) {
    return min + (float)rand() / (float)RAND_MAX * (max - min);
}

// --- Generate a soft radial gradient texture for particles ---
static Image GenerateGlowImage(int size)
{
    Image img = GenImageColor(size, size, BLANK);
    Color* pixels = (Color*)img.data;

    float center = size / 2.0f;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = (x - center) / center;
            float dy = (y - center) / center;
            float dist = sqrtf(dx * dx + dy * dy);
            // Soft falloff: quadratic with a smooth edge for realistic fire glow
            float alpha = 1.0f - dist;
            alpha = std::max(0.0f, alpha);
            alpha = alpha * alpha; // Quadratic falloff for softer edges
            
            unsigned char a = (unsigned char)(alpha * 255.0f);
            pixels[y * size + x] = {255, 255, 255, a};
        }
    }
    return img;
}

ParticleSystem::ParticleSystem(int maxParticles)
    : m_maxParticles(maxParticles)
{
    m_particles.resize(maxParticles);
}

void ParticleSystem::LoadResources()
{
    // Generate the glow texture procedurally
    Image glowImg = GenerateGlowImage(64);
    m_glowTexture = LoadTextureFromImage(glowImg);
    SetTextureFilter(m_glowTexture, TEXTURE_FILTER_BILINEAR);
    UnloadImage(glowImg);
    m_textureLoaded = true;
}

void ParticleSystem::UnloadResources()
{
    if (m_textureLoaded) {
        UnloadTexture(m_glowTexture);
        m_textureLoaded = false;
    }
}

void ParticleSystem::SetEmitterPosition(float x, float y)
{
    m_emitterX = x;
    m_emitterY = y;
}

void ParticleSystem::SetFireScale(float scale)
{
    m_fireScale = std::max(0.1f, scale);
}

void ParticleSystem::SetEmissionRate(float particlesPerSecond)
{
    m_emissionRate = particlesPerSecond;
}

void ParticleSystem::SetEmotionScore(float score)
{
    m_emotionScore = std::max(0.0f, std::min(1.0f, score));
}

void ParticleSystem::SetColorMode(int mode)
{
    m_colorMode = mode;
}

void ParticleSystem::SetParticleSpeedMultiplier(float multi)
{
    m_speedMulti = multi;
}

void ParticleSystem::SetParticleLifeMultiplier(float multi)
{
    m_lifeMulti = multi;
}

int ParticleSystem::FindDeadParticle()
{
    // Search from last used position (round-robin for efficiency)
    for (int i = m_lastUsedParticle; i < m_maxParticles; i++) {
        if (!m_particles[i].alive) {
            m_lastUsedParticle = i;
            return i;
        }
    }
    for (int i = 0; i < m_lastUsedParticle; i++) {
        if (!m_particles[i].alive) {
            m_lastUsedParticle = i;
            return i;
        }
    }
    // All alive — overwrite the oldest (index 0 as fallback)
    m_lastUsedParticle = 0;
    return 0;
}

void ParticleSystem::EmitParticle()
{
    int idx = FindDeadParticle();
    Particle& p = m_particles[idx];

    // --- Position: slight random offset from emitter center ---
    float offsetX = RandFloat(-12.0f, 12.0f) * m_fireScale;
    float offsetY = RandFloat(-6.0f, 6.0f) * m_fireScale;
    p.position = {m_emitterX + offsetX, m_emitterY + offsetY};

    // --- Velocity: upward with spread ---
    float halfSpread = m_spreadAngle * 0.5f * DEG2RAD;
    float angle = -PI / 2.0f + RandFloat(-halfSpread, halfSpread); // -PI/2 = up
    float speed = RandFloat(m_baseSpeedMin, m_baseSpeedMax) * m_fireScale * m_speedMulti;
    p.velocity = {cosf(angle) * speed, sinf(angle) * speed};

    // --- Life & size ---
    p.maxLife  = RandFloat(m_baseLifeMin, m_baseLifeMax) * m_lifeMulti;
    p.life     = p.maxLife;
    // Base size increased for anime fire style
    p.size     = RandFloat(m_baseSizeMin * 2.5f, m_baseSizeMax * 2.5f) * m_fireScale;

    // --- Rotation ---
    p.rotation = RandFloat(0, 360.0f);
    p.rotSpeed = RandFloat(-180.0f, 180.0f);

    p.alive = true;
}

Color ParticleSystem::GetFireColor(float lifeRatio) const
{
    // Realistic fire gradient
    unsigned char r, g, b, a;

    if (lifeRatio > 0.8f) {
        // Inner core: bright white-yellow
        float t = (lifeRatio - 0.8f) / 0.2f;
        r = 255;
        g = (unsigned char)(255 - t * 20);
        b = (unsigned char)(200 * t);
        a = 255;
    } else if (lifeRatio > 0.6f) {
        // Yellow to orange
        float t = (lifeRatio - 0.6f) / 0.2f;
        r = 255;
        g = (unsigned char)(180 + t * 75);
        b = (unsigned char)(30 * t);
        a = 255;
    } else if (lifeRatio > 0.3f) {
        // Orange to red
        float t = (lifeRatio - 0.3f) / 0.3f;
        r = (unsigned char)(200 + t * 55);
        g = (unsigned char)(60 + t * 120);
        b = (unsigned char)(10 * t);
        a = (unsigned char)(200 + t * 55);
    } else {
        // Red to dark, fading out
        float t = lifeRatio / 0.3f;
        r = (unsigned char)(80 + t * 120);
        g = (unsigned char)(20 * t + 10);
        b = 5;
        a = (unsigned char)(t * 200);
    }

    // Apply emotion shifts
    float excitedShift = (m_emotionScore > 0.5f) ? (m_emotionScore - 0.5f) * 2.0f : 0.0f; // 0.0 to 1.0
    float chillShift   = (m_emotionScore < 0.5f) ? (0.5f - m_emotionScore) * 2.0f : 0.0f; // 0.0 to 1.0

    if (m_colorMode == 0) {
        if (excitedShift > 0.0f) {
            // Blend towards bright white/blue
            r = (unsigned char)std::min(255, r + (int)(excitedShift * 35));
            g = (unsigned char)std::min(255, g + (int)(excitedShift * 110));
            b = (unsigned char)std::min(255, b + (int)(excitedShift * 200));
        } else if (chillShift > 0.0f) {
            // Blend towards deep red
            g = (unsigned char)std::max(0, g - (int)(chillShift * 120));
            b = (unsigned char)std::max(0, b - (int)(chillShift * 30));
        }
    } else if (m_colorMode == 1) { // Blue
        unsigned char oldR = r; r = b; b = oldR; // Swap Red and Blue
        g = std::min(255, g + 50);
    } else if (m_colorMode == 2) { // Purple
        b = r; // Match red and blue
    } else if (m_colorMode == 3) { // Red
        g = (unsigned char)std::max(0, g - 120);
        b = (unsigned char)std::max(0, b - 50);
    }

    return {r, g, b, a};
}

void ParticleSystem::Update(float dt)
{
    // --- Emit new particles ---
    m_emissionAccum += m_emissionRate * m_fireScale * dt;
    while (m_emissionAccum >= 1.0f) {
        EmitParticle();
        m_emissionAccum -= 1.0f;
    }

    // --- Update existing particles ---
    float time = (float)GetTime();
    for (auto& p : m_particles) {
        if (!p.alive) continue;

        p.life -= dt;
        if (p.life <= 0.0f) {
            p.alive = false;
            continue;
        }

        // Gravity (negative = upward in screen coords)
        p.velocity.y -= 50.0f * dt * m_speedMulti;

        // Turbulence: sinusoidal horizontal sway
        float turbForce = sinf(time * 5.0f + p.position.y * 0.05f) * m_turbulence * m_speedMulti;
        p.velocity.x += turbForce * dt;

        // Damping
        p.velocity.x *= 0.98f;
        p.velocity.y *= 0.99f;

        // Integrate position
        p.position.x += p.velocity.x * dt;
        p.position.y += p.velocity.y * dt;

        // Shrink over lifetime
        float lifeRatio = p.life / p.maxLife;
        // Particles shrink in the last 30% of life
        if (lifeRatio < 0.3f) {
            p.size *= 0.995f;
        }

        // Rotation
        p.rotation += p.rotSpeed * dt;
    }
}

void ParticleSystem::Draw()
{
    // Draw the core dot behind the particles, matching the base fire color
    BeginBlendMode(BLEND_ADDITIVE);
    float coreSize = 35.0f * m_fireScale;
    Color coreColor = GetFireColor(0.9f); // Get the core color
    coreColor.a = 200; // Slightly transparent glow
    DrawCircleGradient((int)m_emitterX, (int)m_emitterY + 10.0f, coreSize,
                       coreColor, {coreColor.r, coreColor.g, coreColor.b, 0});
    // Inner bright spot
    DrawCircleGradient((int)m_emitterX, (int)m_emitterY + 5.0f, coreSize * 0.5f,
                       {255, 255, 255, 200}, {coreColor.r, coreColor.g, coreColor.b, 0});
    EndBlendMode();

    // Use Additive blending for realistic glowing fire
    BeginBlendMode(BLEND_ADDITIVE);

    for (const auto& p : m_particles) {
        if (!p.alive) continue;

        float lifeRatio = p.life / p.maxLife;
        Color col = GetFireColor(lifeRatio);

        if (m_textureLoaded) {
            // Draw textured particle with the glow sprite
            float drawSize = p.size * 2.0f;
            Rectangle src  = {0, 0, (float)m_glowTexture.width, (float)m_glowTexture.height};
            Rectangle dst  = {p.position.x, p.position.y, drawSize, drawSize};
            Vector2 origin = {drawSize / 2.0f, drawSize / 2.0f};
            DrawTexturePro(m_glowTexture, src, dst, origin, p.rotation, col);
        } else {
            // Fallback: simple circle
            DrawCircleV(p.position, p.size, col);
        }
    }

    EndBlendMode();
}

int ParticleSystem::GetAliveCount() const
{
    int count = 0;
    for (const auto& p : m_particles) {
        if (p.alive) count++;
    }
    return count;
}
