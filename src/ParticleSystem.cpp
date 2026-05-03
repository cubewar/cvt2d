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

void ParticleSystem::SetCustomColor(unsigned char r, unsigned char g, unsigned char b)
{
    m_customR = r;
    m_customG = g;
    m_customB = b;
}

void ParticleSystem::SetCoreSizeMulti(float multi)
{
    m_coreSizeMulti = multi;
}

void ParticleSystem::SetParticleSpeedMultiplier(float multi)
{
    m_speedMulti = multi;
}

void ParticleSystem::SetParticleLifeMultiplier(float multi)
{
    m_lifeMulti = multi;
}

void ParticleSystem::SetSmokeMode(bool smoke)
{
    m_isSmoke = smoke;
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
    unsigned char r, g, b, a;

    if (m_isSmoke) {
        // Smoke Mode: Grey/Dark, less opaque
        float val = 40.0f + 60.0f * lifeRatio;
        r = g = b = (unsigned char)val;
        a = (unsigned char)(lifeRatio * 150.0f);
        return {r, g, b, a};
    }

    // Dynamic fire gradient based entirely on user's Main Color (m_customR/G/B)
    float mainR = m_customR;
    float mainG = m_customG;
    float mainB = m_customB;

    if (lifeRatio > 0.7f) {
        // Inner color: Main + bright offset (towards white)
        r = (unsigned char)std::min(255.0f, mainR + 200.0f);
        g = (unsigned char)std::min(255.0f, mainG + 200.0f);
        b = (unsigned char)std::min(255.0f, mainB + 200.0f);
        a = 200;
    } else if (lifeRatio > 0.4f) {
        // Mid color: Main + slight offset
        r = (unsigned char)std::min(255.0f, mainR + 100.0f);
        g = (unsigned char)std::min(255.0f, mainG + 100.0f);
        b = (unsigned char)std::min(255.0f, mainB + 100.0f);
        a = 150;
    } else if (lifeRatio > 0.15f) {
        // Outer color: Pure Main Color
        r = (unsigned char)mainR;
        g = (unsigned char)mainG;
        b = (unsigned char)mainB;
        a = 100;
    } else {
        // Fade out
        float fade = lifeRatio / 0.15f;
        r = (unsigned char)(mainR * fade);
        g = (unsigned char)(mainG * fade);
        b = (unsigned char)(mainB * fade);
        a = (unsigned char)(fade * 300);
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
    // But limit its size so it doesn't cover everything when scale is large
    if (!m_isSmoke && m_coreSizeMulti > 0.0f) {
        BeginBlendMode(BLEND_ADDITIVE);
        float coreSize = std::min(40.0f, 25.0f * m_fireScale) * m_coreSizeMulti;
        Color coreColor = GetFireColor(0.9f); // Get the core color
        coreColor.a = 200; // Slightly transparent glow
        DrawCircleGradient((int)m_emitterX, (int)m_emitterY + 10.0f, coreSize,
                           coreColor, {coreColor.r, coreColor.g, coreColor.b, 0});
        // Inner bright spot
        DrawCircleGradient((int)m_emitterX, (int)m_emitterY + 5.0f, coreSize * 0.5f,
                           {255, 255, 255, 200}, {coreColor.r, coreColor.g, coreColor.b, 0});
        EndBlendMode();
    }

    // Use Additive blending for realistic glowing fire, Alpha for smoke
    BeginBlendMode(m_isSmoke ? BLEND_ALPHA : BLEND_ADDITIVE);

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
