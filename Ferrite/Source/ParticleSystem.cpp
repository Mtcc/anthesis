#include "ParticleSystem.h"
#include "BotanicalLookAndFeel.h"

ParticleSystem::ParticleSystem (juce::Random& r) : rng (r) {}

float ParticleSystem::opacity (const Particle& p) const noexcept
{
    constexpr float fadeIn  = 0.6f;
    constexpr float fadeOut = 0.8f;
    if (p.age < fadeIn)             return p.age / fadeIn;
    if (p.age > p.maxAge - fadeOut) return (p.maxAge - p.age) / fadeOut;
    return 0.78f;
}

void ParticleSystem::spawnAmbient (juce::Rectangle<float> b)
{
    Particle p;
    p.x        = b.getX() + rng.nextFloat() * b.getWidth();
    p.y        = b.getY() + rng.nextFloat() * b.getHeight();
    // slow random drift, slightly upward bias
    p.vx       = (rng.nextFloat() - 0.5f) * 8.0f;
    p.vy       = -rng.nextFloat() * 6.0f - 2.0f;
    p.age      = 0.0f;
    p.maxAge   = 3.5f + rng.nextFloat() * 2.5f;
    p.size     = 2.5f + rng.nextFloat() * 2.5f;
    p.isAmberDrip = false;
    p.isGreen  = rng.nextFloat() > 0.45f;
    particles.push_back (p);
}

void ParticleSystem::spawnAmberDrip (juce::Rectangle<float> b)
{
    Particle p;
    p.x        = b.getX() + 20.0f + rng.nextFloat() * (b.getWidth() - 40.0f);
    p.y        = b.getY() + 10.0f;
    p.vx       = (rng.nextFloat() - 0.5f) * 6.0f;
    p.vy       = 12.0f + rng.nextFloat() * 8.0f; // falls downward
    p.age      = 0.0f;
    p.maxAge   = 3.5f;
    p.size     = 3.5f + rng.nextFloat() * 2.0f;
    p.isAmberDrip = true;
    p.isGreen  = false;
    particles.push_back (p);
}

void ParticleSystem::reset (juce::Rectangle<float> bounds)
{
    particles.clear();
    for (int i = 0; i < kTargetAmbient; ++i)
        spawnAmbient (bounds);
    spawnTimer = 0.0f;
}

void ParticleSystem::update (float dt, juce::Rectangle<float> bounds)
{
    // move & age particles
    for (auto& p : particles)
    {
        p.age += dt;
        p.x   += p.vx * dt;
        p.y   += p.vy * dt;

        if (p.isAmberDrip)
        {
            // subtle gravity acceleration
            p.vy += 5.0f * dt;
            // slight side sway
            p.vx += std::sin (p.age * 2.1f) * 0.5f * dt;
        }
    }

    // remove dead
    particles.erase (
        std::remove_if (particles.begin(), particles.end(),
            [] (const Particle& p) { return p.age >= p.maxAge; }),
        particles.end());

    // maintain ambient count
    int ambientCount = 0;
    for (auto& p : particles) if (!p.isAmberDrip) ++ambientCount;
    while (ambientCount < kTargetAmbient)
    {
        spawnAmbient (bounds);
        ++ambientCount;
    }

    // amber drips on timer
    spawnTimer += dt;
    if (spawnTimer >= kDripInterval)
    {
        spawnAmberDrip (bounds);
        spawnTimer = std::fmod (spawnTimer, kDripInterval);
    }
}

void ParticleSystem::draw (juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    for (auto& p : particles)
    {
        float op = opacity (p);
        if (op <= 0.0f) continue;

        // clip to bounds with a margin
        if (!bounds.expanded (p.size * 2.0f).contains (p.x, p.y)) continue;

        juce::Colour core = p.isGreen ? AC::luminousGreen() : AC::warmAmber();
        float glowR = p.size * 2.8f;

        juce::ColourGradient glow (core.withAlpha (op * 0.4f), p.x, p.y,
                                    core.withAlpha (0.0f), p.x + glowR, p.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (p.x - glowR, p.y - glowR, glowR * 2.0f, glowR * 2.0f);

        g.setColour (core.withAlpha (op * 0.85f));
        g.fillEllipse (p.x - p.size, p.y - p.size, p.size * 2.0f, p.size * 2.0f);
    }
}
