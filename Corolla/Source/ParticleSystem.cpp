#include "ParticleSystem.h"
#include "BotanicalLookAndFeel.h"

CorollaParticleSystem::CorollaParticleSystem (juce::Random& r) : rng (r) {}

float CorollaParticleSystem::opacity (const Particle& p) const noexcept
{
    constexpr float fadeIn  = 0.5f;
    constexpr float fadeOut = 0.8f;
    if (p.age < fadeIn)             return p.age / fadeIn;
    if (p.age > p.maxAge - fadeOut) return (p.maxAge - p.age) / fadeOut;
    return 0.72f;
}

void CorollaParticleSystem::spawnAmbient (juce::Rectangle<float> b)
{
    Particle p;
    // spawn in the top half of the bounds
    p.x        = b.getX() + rng.nextFloat() * b.getWidth();
    p.y        = b.getY() + rng.nextFloat() * b.getHeight() * 0.5f;
    p.vx       = (rng.nextFloat() - 0.5f) * 6.0f;
    p.vy       = 5.0f + rng.nextFloat() * 7.0f;   // falls downward
    p.age      = 0.0f;
    p.maxAge   = 4.0f + rng.nextFloat() * 3.0f;
    p.size     = 2.0f + rng.nextFloat() * 2.5f;
    p.rotation = rng.nextFloat() * juce::MathConstants<float>::twoPi;
    p.rotationSpeed = (rng.nextFloat() * 1.2f + 0.8f) * (rng.nextBool() ? 1.0f : -1.0f);
    p.isGreen  = rng.nextFloat() > 0.70f;  // 30% lavender, 70% petalPink
    particles.push_back (p);
}

void CorollaParticleSystem::spawnPetal (juce::Rectangle<float> b)
{
    Particle p;
    p.x        = b.getX() + 20.0f + rng.nextFloat() * (b.getWidth() - 40.0f);
    p.y        = b.getY() + rng.nextFloat() * b.getHeight() * 0.4f;
    p.vx       = (rng.nextFloat() - 0.5f) * 8.0f;
    p.vy       = 5.0f + rng.nextFloat() * 7.0f;   // falls downward
    p.age      = 0.0f;
    p.maxAge   = 4.5f + rng.nextFloat() * 2.5f;
    p.size     = 6.0f + rng.nextFloat() * 4.0f;   // larger than ambient
    p.rotation = rng.nextFloat() * juce::MathConstants<float>::twoPi;
    p.rotationSpeed = (rng.nextFloat() * 1.2f + 0.8f) * (rng.nextBool() ? 1.0f : -1.0f);
    p.isGreen  = rng.nextFloat() > 0.70f;
    particles.push_back (p);
}

void CorollaParticleSystem::reset (juce::Rectangle<float> bounds)
{
    particles.clear();
    for (int i = 0; i < kTargetAmbient; ++i)
        spawnAmbient (bounds);
    spawnTimer = 0.0f;
}

void CorollaParticleSystem::update (float dt, juce::Rectangle<float> bounds)
{
    for (auto& p : particles)
    {
        p.age      += dt;
        p.x        += p.vx * dt;
        p.y        += p.vy * dt;
        p.rotation += p.rotationSpeed * dt;

        // gentle spiral drift — side sway increases over lifetime
        p.vx += std::sin (p.age * 1.8f + p.rotation) * 1.2f * dt;
    }

    // remove dead or out-of-bounds particles
    particles.erase (
        std::remove_if (particles.begin(), particles.end(),
            [&] (const Particle& p)
            {
                return p.age >= p.maxAge || p.y > bounds.getBottom() + 20.0f;
            }),
        particles.end());

    // maintain ambient count
    int ambientCount = 0;
    for (auto& p : particles) if (p.size < 5.0f) ++ambientCount;
    while (ambientCount < kTargetAmbient)
    {
        spawnAmbient (bounds);
        ++ambientCount;
    }

    // spawn larger petals on timer
    spawnTimer += dt;
    if (spawnTimer >= kBurstInterval)
    {
        spawnPetal (bounds);
        spawnTimer = std::fmod (spawnTimer, kBurstInterval);
    }
}

void CorollaParticleSystem::draw (juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    for (auto& p : particles)
    {
        float op = opacity (p);
        if (op <= 0.0f) continue;
        if (!bounds.expanded (p.size * 2.0f).contains (p.x, p.y)) continue;

        // petalPink (70%) vs lavenderMist (30%)
        juce::Colour col = p.isGreen ? AC::lavenderMist() : AC::petalPink();

        // draw a rotated ellipse to give petal shape
        float pw = 4.0f * (p.size / 3.0f);
        float ph = 8.0f * (p.size / 3.0f);

        juce::Path petal;
        petal.addEllipse (-pw * 0.5f, -ph * 0.5f, pw, ph);

        juce::AffineTransform xf = juce::AffineTransform::rotation (p.rotation)
                                     .translated (p.x, p.y);

        // soft glow
        float glowR = p.size * 2.2f;
        juce::ColourGradient glow (col.withAlpha (op * 0.3f), p.x, p.y,
                                   col.withAlpha (0.0f), p.x + glowR, p.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (p.x - glowR, p.y - glowR, glowR * 2.0f, glowR * 2.0f);

        g.setColour (col.withAlpha (op * 0.80f));
        g.fillPath (petal, xf);
    }
}
