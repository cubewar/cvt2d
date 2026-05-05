#include "ParticleSystem.h"
#include "rlgl.h"
#include <cstdlib>
#include <cstring>
#include <algorithm>

// --- Helper: random float in [min, max] ---
static float RandFloat(float min, float max) {
    return min + (float)rand() / (float)RAND_MAX * (max - min);
}

// Smooth interpolation helper
static float SmoothLerp(float a, float b, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    return a + (b - a) * t;
}

static unsigned char LerpByte(unsigned char a, unsigned char b, float t) {
    return (unsigned char)(a + (int)(b - a) * std::max(0.0f, std::min(1.0f, t)));
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

void ParticleSystem::SetPhaseColors(Color inner, Color mid, Color outer)
{
    m_innerR = inner.r; m_innerG = inner.g; m_innerB = inner.b;
    m_midR   = mid.r;   m_midG   = mid.g;   m_midB   = mid.b;
    m_outerR = outer.r; m_outerG = outer.g; m_outerB = outer.b;
}

void ParticleSystem::SetSeedShape(float strength)
{
    m_seedShapeMulti = strength;
}

void ParticleSystem::SetCoreSizeMulti(float multi)
{
    m_coreSizeMulti = multi;
}

void ParticleSystem::SetParticleOpacity(float opacity)
{
    m_particleOpacity = opacity;
}

void ParticleSystem::SetLifeRatios(float inner, float mid, float outer)
{
    m_innerRatio = inner;
    m_midRatio = mid;
    m_outerRatio = outer;
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

    // --- Velocity: upward with spread ---
    float halfSpread = m_spreadAngle * 0.5f * DEG2RAD;
    float angle = -PI / 2.0f + RandFloat(-halfSpread, halfSpread); // -PI/2 = up
    float speed = RandFloat(m_baseSpeedMin, m_baseSpeedMax) * m_fireScale * m_speedMulti;
    p.velocity = {cosf(angle) * speed, sinf(angle) * speed};

    // --- Seed shape: particles near the center (straight up) live longer ---
    // angle is centered at -PI/2 (straight up). 
    // deviation = how far from straight up this particle goes (0 = center, 1 = edge)
    float deviation = fabsf(angle - (-PI / 2.0f)) / halfSpread; // 0..1
    deviation = std::min(1.0f, deviation);
    // Center particles (deviation≈0) get the LONGEST life, edge particles (deviation≈1) get shorter
    float lifeScale = SmoothLerp(m_seedShapeMulti, 0.5f, deviation);

    // --- Position: slight offset, narrower for center particles ---
    float offsetRange = 12.0f * m_fireScale * (0.3f + 0.7f * deviation); // center = tight, edges = spread
    float offsetX = RandFloat(-offsetRange, offsetRange);
    float offsetY = RandFloat(-4.0f, 4.0f) * m_fireScale;
    p.position = {m_emitterX + offsetX, m_emitterY + offsetY};

    // --- Life & size ---
    p.maxLife  = RandFloat(m_baseLifeMin, m_baseLifeMax) * m_lifeMulti * lifeScale;
    p.life     = p.maxLife;
    // Size: center particles slightly smaller for a tighter core
    float sizeScale = SmoothLerp(1.8f, 2.8f, deviation);
    p.size     = RandFloat(m_baseSizeMin, m_baseSizeMax) * sizeScale * m_fireScale;

    // --- Rotation ---
    p.rotation = RandFloat(0, 360.0f);
    p.rotSpeed = RandFloat(-90.0f, 90.0f); // Slower rotation for less visual noise

    p.alive = true;
}

Color ParticleSystem::GetFireColor(float distRatio, float lifeRatio) const
{
    // distRatio: 0.0 = at emitter center, 1.0 = at max fire reach
    //   This drives COLOR so all particles at the same height share the same hue.
    // lifeRatio: 0.0 = about to die, 1.0 = just born
    //   This drives OPACITY only (fade out as particles age).

    unsigned char r, g, b;
    float baseAlpha;

    // --- Color from spatial distance (seamless gradient) ---
    // Use smoothstep-style thresholds based on distance from center
    float innerEdge = m_innerRatio;   // e.g. 0.25 — within this = inner color
    float midEdge   = m_midRatio;     // e.g. 0.55 — within this = mid color zone
    // Beyond midEdge = outer color, fading to transparent

    if (distRatio < innerEdge) {
        // Core zone: blend inner → mid as we move outward
        float t = distRatio / std::max(0.001f, innerEdge);
        r = LerpByte(m_innerR, m_midR, t);
        g = LerpByte(m_innerG, m_midG, t);
        b = LerpByte(m_innerB, m_midB, t);
        baseAlpha = SmoothLerp(220.0f, 200.0f, t);
    } else if (distRatio < midEdge) {
        // Mid zone: blend mid → outer
        float t = (distRatio - innerEdge) / std::max(0.001f, midEdge - innerEdge);
        r = LerpByte(m_midR, m_outerR, t);
        g = LerpByte(m_midG, m_outerG, t);
        b = LerpByte(m_midB, m_outerB, t);
        baseAlpha = SmoothLerp(200.0f, 140.0f, t);
    } else {
        // Outer zone: pure outer color, fading out
        float t = (distRatio - midEdge) / std::max(0.001f, 1.0f - midEdge);
        t = std::min(1.0f, t);
        r = m_outerR;
        g = m_outerG;
        b = m_outerB;
        baseAlpha = SmoothLerp(140.0f, 0.0f, t);
    }

    // --- Alpha from lifetime (fade out dying particles) ---
    float lifeFade = 1.0f;
    if (lifeRatio < 0.3f) {
        lifeFade = lifeRatio / 0.3f; // Smooth fade in last 30% of life
    }
    float a = baseAlpha * lifeFade;

    // Apply master opacity
    a = std::min(255.0f, a * m_particleOpacity);

    unsigned char finalA = (unsigned char)std::max(0.0f, a);

    if (m_smokeBlend > 0.0f) {
        float smokeVal = 40.0f + 60.0f * lifeRatio;
        unsigned char smokeR = (unsigned char)smokeVal;
        unsigned char smokeG = (unsigned char)smokeVal;
        unsigned char smokeB = (unsigned char)smokeVal;
        unsigned char smokeA = (unsigned char)(lifeRatio * 120.0f);

        r = LerpByte(r, smokeR, m_smokeBlend);
        g = LerpByte(g, smokeG, m_smokeBlend);
        b = LerpByte(b, smokeB, m_smokeBlend);
        finalA = LerpByte(finalA, smokeA, m_smokeBlend);
    }

    return {r, g, b, finalA};
}

void ParticleSystem::Update(float dt)
{
    // Smoothly transition smoke blend
    if (m_isSmoke) {
        m_smokeBlend = std::min(1.0f, m_smokeBlend + dt * 2.0f);
    } else {
        m_smokeBlend = std::max(0.0f, m_smokeBlend - dt * 2.0f);
    }

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
    // --- Core glow (small white-ish center) ---
    float coreBlend = m_coreSizeMulti * (1.0f - m_smokeBlend);
    if (coreBlend > 0.0f && m_textureLoaded) {
        // Draw the core with ALPHA blending so it doesn't blow out
        BeginBlendMode(BLEND_ALPHA);
        float coreSize = std::min(30.0f, 18.0f * m_fireScale) * coreBlend;
        
        Rectangle source = {0.0f, 0.0f, (float)m_glowTexture.width, (float)m_glowTexture.height};
        
        // Warm glow matching the inner phase color (distRatio=0 = center)
        Color coreColor = {m_innerR, m_innerG, m_innerB, (unsigned char)(180.0f * m_particleOpacity)};
        Rectangle destOuter = {m_emitterX, m_emitterY + 8.0f, coreSize * 3.0f, coreSize * 3.0f};
        Vector2 originOuter = {destOuter.width/2.0f, destOuter.height/2.0f};
        DrawTexturePro(m_glowTexture, source, destOuter, originOuter, 0.0f, coreColor);
        
        // Small bright center spot
        Color brightCore = {m_innerR, m_innerG, m_innerB, (unsigned char)(220 * m_particleOpacity)};
        Rectangle destInner = {m_emitterX, m_emitterY + 4.0f, coreSize * 1.5f, coreSize * 1.5f};
        Vector2 originInner = {destInner.width/2.0f, destInner.height/2.0f};
        DrawTexturePro(m_glowTexture, source, destInner, originInner, 0.0f, brightCore);
        
        EndBlendMode();
    }

    // --- Particles: Use ALPHA blending to prevent white blowout ---
    // Color is driven by DISTANCE from emitter (spatial gradient),
    // so all particles at the same height share the same color = seamless.
    BeginBlendMode(BLEND_ALPHA);

    // Estimate the fire's maximum reach for normalizing distance.
    // Based on average speed * average life * scale, this gives a stable reference.
    float maxReach = (m_baseSpeedMax * 0.7f) * (m_baseLifeMax * m_lifeMulti * m_seedShapeMulti) * m_fireScale;
    maxReach = std::max(50.0f, maxReach); // Safety floor

    for (const auto& p : m_particles) {
        if (!p.alive) continue;

        // Distance from emitter center (primarily vertical, slight horizontal)
        float dx = p.position.x - m_emitterX;
        float dy = p.position.y - m_emitterY;
        float dist = sqrtf(dx * dx + dy * dy);
        float distRatio = std::min(1.0f, dist / maxReach);

        float lifeRatio = p.life / p.maxLife;
        Color col = GetFireColor(distRatio, lifeRatio);

        if (m_textureLoaded) {
            float drawSize = p.size * 2.0f;
            Rectangle src  = {0, 0, (float)m_glowTexture.width, (float)m_glowTexture.height};
            Rectangle dst  = {p.position.x, p.position.y, drawSize, drawSize};
            Vector2 origin = {drawSize / 2.0f, drawSize / 2.0f};
            DrawTexturePro(m_glowTexture, src, dst, origin, p.rotation, col);
        } else {
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
