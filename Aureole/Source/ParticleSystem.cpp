#include "ParticleSystem.h"
#include "BotanicalLookAndFeel.h"

AureoleParticleSystem::AureoleParticleSystem (juce::Random& r) : rng (r) {}

void AureoleParticleSystem::spawnFirefly (juce::Rectangle<float> b, float startX)
{
    Particle p;
    // if startX provided, spawn at that x; else scatter randomly across bounds
    p.x = (startX >= 0.0f) ? startX
                             : b.getX() + rng.nextFloat() * b.getWidth();
    p.y          = b.getY() + rng.nextFloat() * b.getHeight();
    p.vx         = kVxMin + rng.nextFloat() * (kVxMax - kVxMin);
    p.age        = rng.nextFloat() * juce::MathConstants<float>::twoPi; // random phase start
    p.blinkPhase = rng.nextFloat() * juce::MathConstants<float>::twoPi;
    p.blinkRate  = 1.0f + rng.nextFloat();     // 1.0–2.0 Hz
    p.size       = 2.5f + rng.nextFloat() * 1.5f;
    p.isAmber    = rng.nextFloat() > 0.45f;    // ~55% goldenHour, ~45% warmAmber
    particles.push_back (p);
}

void AureoleParticleSystem::reset (juce::Rectangle<float> bounds)
{
    particles.clear();
    // scatter initial fireflies across the full bounds
    for (int i = 0; i < kTarget; ++i)
        spawnFirefly (bounds);
}

void AureoleParticleSystem::update (float dt, juce::Rectangle<float> bounds)
{
    const float twoPi = juce::MathConstants<float>::twoPi;

    for (auto& p : particles)
    {
        p.age        += dt;
        p.blinkPhase += dt * p.blinkRate * twoPi;
        if (p.blinkPhase > twoPi) p.blinkPhase -= twoPi;

        // rightward drift with gentle sine vertical oscillation
        p.x += p.vx * dt;
        p.y += std::sin (p.age * 2.0f) * 8.0f * dt;

        // when firefly drifts off the right edge, respawn at the left
        if (p.x > bounds.getRight() + p.size * 2.0f)
        {
            p.x          = bounds.getX() - p.size * 2.0f;
            p.y          = bounds.getY() + rng.nextFloat() * bounds.getHeight();
            p.vx         = kVxMin + rng.nextFloat() * (kVxMax - kVxMin);
            p.age        = 0.0f;
            p.blinkPhase = rng.nextFloat() * twoPi;
            p.blinkRate  = 1.0f + rng.nextFloat();
            p.size       = 2.5f + rng.nextFloat() * 1.5f;
            p.isAmber    = rng.nextFloat() > 0.45f;
        }
    }

    // maintain target count — spawn new fireflies at left edge if count fell
    while ((int)particles.size() < kTarget)
        spawnFirefly (bounds, bounds.getX() - 4.0f);
}

void AureoleParticleSystem::draw (juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    for (auto& p : particles)
    {
        if (!bounds.expanded (p.size * 3.0f).contains (p.x, p.y)) continue;

        // blink: opacity pulses between 0.3 and 0.8
        float opacity = 0.3f + 0.5f * std::sin (p.blinkPhase);
        if (opacity <= 0.0f) continue;

        juce::Colour core = p.isAmber ? AC::warmAmber() : AC::goldenHour();
        float glowR = p.size * 3.5f;

        // soft radial glow halo (15px-ish radius for max size particle)
        juce::ColourGradient glow (core.withAlpha (opacity * 0.38f), p.x, p.y,
                                    core.withAlpha (0.0f), p.x + glowR, p.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (p.x - glowR, p.y - glowR, glowR * 2.0f, glowR * 2.0f);

        // bright firefly core
        g.setColour (core.withAlpha (opacity * 0.90f));
        g.fillEllipse (p.x - p.size, p.y - p.size, p.size * 2.0f, p.size * 2.0f);

        // tiny specular highlight
        g.setColour (AC::parchment().withAlpha (opacity * 0.45f));
        float hs = p.size * 0.35f;
        g.fillEllipse (p.x - hs, p.y - hs, hs * 2.0f, hs * 2.0f);
    }
}
