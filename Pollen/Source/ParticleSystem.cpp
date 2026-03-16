#include "ParticleSystem.h"
#include "BotanicalLookAndFeel.h"

PollenParticleSystem::PollenParticleSystem (juce::Random& r) : rng (r) {}

void PollenParticleSystem::spawnAt (juce::Rectangle<float> b, float startX)
{
    Particle p;
    p.x       = startX;
    p.y       = b.getY() + rng.nextFloat() * b.getHeight();
    p.vx      = 20.0f + rng.nextFloat() * 20.0f; // 20-40 px/sec rightward
    p.age     = rng.nextFloat() * 10.0f;          // stagger ages so not all in sync
    p.size    = 1.5f + rng.nextFloat() * 1.5f;    // 1.5-3px
    p.index   = (int) particles.size();
    p.isGolden = rng.nextFloat() > 0.5f;
    particles.push_back (p);
}

void PollenParticleSystem::reset (juce::Rectangle<float> bounds)
{
    particles.clear();
    for (int i = 0; i < kTargetCount; ++i)
    {
        // scatter initial positions across the full width
        float startX = bounds.getX() + rng.nextFloat() * bounds.getWidth();
        spawnAt (bounds, startX);
        particles.back().index = i;
    }
}

void PollenParticleSystem::update (float dt, juce::Rectangle<float> bounds)
{
    for (auto& p : particles)
    {
        p.age += dt;
        p.x   += p.vx * dt;

        // gentle vertical sine drift
        float vy = std::sin (p.age * 1.5f + (float) p.index * 1.2f) * 4.0f;
        p.y += vy * dt;

        // clamp y to bounds so particles don't drift off vertically
        p.y = juce::jlimit (bounds.getY() + p.size, bounds.getBottom() - p.size, p.y);

        // respawn at left edge when particle exits right edge
        if (p.x > bounds.getRight() + p.size)
        {
            p.x  = bounds.getX() - p.size;
            p.y  = bounds.getY() + rng.nextFloat() * bounds.getHeight();
            p.vx = 20.0f + rng.nextFloat() * 20.0f;
        }
    }

    // maintain count (in case particles somehow get removed)
    while ((int) particles.size() < kTargetCount)
        spawnAt (bounds, bounds.getX() - 4.0f);
}

void PollenParticleSystem::draw (juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    for (auto& p : particles)
    {
        if (!bounds.expanded (p.size * 2.0f).contains (p.x, p.y)) continue;

        juce::Colour core = p.isGolden ? AC::goldenHour() : AC::luminousGreen();

        // soft glow halo
        float glowR = p.size * 3.0f;
        juce::ColourGradient glow (core.withAlpha (0.35f), p.x, p.y,
                                    core.withAlpha (0.0f), p.x + glowR, p.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (p.x - glowR, p.y - glowR, glowR * 2.0f, glowR * 2.0f);

        // bright core dot
        g.setColour (core.withAlpha (0.80f));
        g.fillEllipse (p.x - p.size, p.y - p.size, p.size * 2.0f, p.size * 2.0f);
    }
}
