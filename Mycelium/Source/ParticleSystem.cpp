#include "ParticleSystem.h"
#include "BotanicalLookAndFeel.h"

MyceliumParticleSystem::MyceliumParticleSystem (juce::Random& r) : rng (r) {}

float MyceliumParticleSystem::opacity (const Particle& p) const noexcept
{
    constexpr float fadeIn  = 0.5f;
    constexpr float fadeOut = 0.9f;
    if (p.age < fadeIn)             return p.age / fadeIn;
    if (p.age > p.maxAge - fadeOut) return (p.maxAge - p.age) / fadeOut;
    return 0.72f;
}

void MyceliumParticleSystem::spawnAmbient (juce::Rectangle<float> b)
{
    Particle p;
    p.x    = b.getX() + rng.nextFloat() * b.getWidth();
    p.y    = b.getY() + b.getHeight() * 0.6f + rng.nextFloat() * b.getHeight() * 0.4f;
    // upward drift, slight horizontal wander seeded by x position
    p.vx   = (rng.nextFloat() - 0.5f) * 4.0f;
    p.vy   = -(15.0f + rng.nextFloat() * 10.0f);
    p.age      = 0.0f;
    p.maxAge   = 4.0f + rng.nextFloat() * 3.0f;
    p.size     = 2.0f + rng.nextFloat() * 2.5f;
    p.isGreen  = rng.nextFloat() > 0.45f;
    p.isSporeBurst = false;
    particles.push_back (p);
}

void MyceliumParticleSystem::spawnSporeBurst (juce::Rectangle<float> b)
{
    // 3 particles spawned at once from a random point, spreading then drifting up
    float bx = b.getX() + 20.0f + rng.nextFloat() * (b.getWidth() - 40.0f);
    float by = b.getY() + 20.0f + rng.nextFloat() * (b.getHeight() - 40.0f);

    for (int k = 0; k < 3; ++k)
    {
        float spreadAngle = (float)k * juce::MathConstants<float>::twoPi / 3.0f
                            + rng.nextFloat() * 0.8f;
        float spreadSpeed = 18.0f + rng.nextFloat() * 12.0f;

        Particle p;
        p.x    = bx;
        p.y    = by;
        p.vx   = std::sin (spreadAngle) * spreadSpeed;
        p.vy   = std::cos (spreadAngle) * spreadSpeed - 12.0f; // bias upward
        p.age      = 0.0f;
        p.maxAge   = 2.5f + rng.nextFloat() * 1.5f;
        p.size     = 2.5f + rng.nextFloat() * 2.0f;
        p.isGreen  = (k == 0); // first is green, others lavender
        p.isSporeBurst = true;
        particles.push_back (p);
    }
}

void MyceliumParticleSystem::reset (juce::Rectangle<float> bounds)
{
    particles.clear();
    for (int i = 0; i < kTargetAmbient; ++i)
        spawnAmbient (bounds);
    spawnTimer = 0.0f;
}

void MyceliumParticleSystem::update (float dt, juce::Rectangle<float> bounds)
{
    // move & age
    for (auto& p : particles)
    {
        p.age += dt;
        // spiral drift: vx oscillates with age
        float spiralVx = std::sin (p.age * 2.5f) * 8.0f;
        p.x += (p.vx + spiralVx) * dt;
        p.y += p.vy * dt;

        if (p.isSporeBurst)
        {
            // burst particles slow their spread over time, then just drift up
            p.vx *= (1.0f - 1.5f * dt);
            p.vy += 4.0f * dt; // slight gravity, but upward bias from initial vy keeps them rising
        }
    }

    // remove dead or escaped particles
    particles.erase (
        std::remove_if (particles.begin(), particles.end(),
            [&] (const Particle& p)
            {
                return p.age >= p.maxAge
                    || p.y < bounds.getY() - 30.0f
                    || p.x < bounds.getX() - 30.0f
                    || p.x > bounds.getRight() + 30.0f;
            }),
        particles.end());

    // maintain ambient count
    int ambientCount = 0;
    for (auto& p : particles)
        if (!p.isSporeBurst) ++ambientCount;
    while (ambientCount < kTargetAmbient)
    {
        spawnAmbient (bounds);
        ++ambientCount;
    }

    // spore burst on timer
    spawnTimer += dt;
    if (spawnTimer >= kBurstInterval)
    {
        spawnSporeBurst (bounds);
        spawnTimer = std::fmod (spawnTimer, kBurstInterval);
    }
}

void MyceliumParticleSystem::draw (juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    for (auto& p : particles)
    {
        float op = opacity (p);
        if (op <= 0.0f) continue;

        if (!bounds.expanded (p.size * 2.5f).contains (p.x, p.y)) continue;

        juce::Colour core = p.isGreen ? AC::luminousGreen() : AC::lavenderMist();
        float glowR = p.size * 2.8f;

        juce::ColourGradient glow (core.withAlpha (op * 0.38f), p.x, p.y,
                                    core.withAlpha (0.0f), p.x + glowR, p.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (p.x - glowR, p.y - glowR, glowR * 2.0f, glowR * 2.0f);

        g.setColour (core.withAlpha (op * 0.82f));
        g.fillEllipse (p.x - p.size, p.y - p.size, p.size * 2.0f, p.size * 2.0f);
    }
}
