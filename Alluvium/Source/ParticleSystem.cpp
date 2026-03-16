#include "ParticleSystem.h"
#include "BotanicalLookAndFeel.h"

AlluviumParticleSystem::AlluviumParticleSystem (juce::Random& r) : rng (r) {}

float AlluviumParticleSystem::opacity (const Particle& p) const noexcept
{
    // linear fade from full opacity at spawn to 0 at maxAge
    return juce::jlimit (0.0f, 1.0f, 1.0f - (p.age / p.maxAge));
}

void AlluviumParticleSystem::spawnRain (juce::Rectangle<float> b)
{
    Particle p;
    p.x      = b.getX() + rng.nextFloat() * b.getWidth();
    p.y      = b.getY();   // spawn at top edge
    p.vx     = 0.0f;
    p.vy     = 30.0f + rng.nextFloat() * 30.0f;  // 30–60 px/s downward
    p.age    = 0.0f;
    // maxAge chosen so particle reaches ~65% of height before fading out
    p.maxAge = (b.getHeight() * 0.65f) / p.vy;
    p.size   = 1.0f + rng.nextFloat() * 1.0f;    // 1–2 px stroke width
    p.length = p.size * 3.0f;
    p.isGreen = rng.nextFloat() < 0.30f;          // 30% green, 70% lavender
    particles.push_back (p);
}

void AlluviumParticleSystem::reset (juce::Rectangle<float> bounds)
{
    particles.clear();
    for (int i = 0; i < kTargetAmbient; ++i)
    {
        spawnRain (bounds);
        // scatter initial positions so they don't all start at y=0
        particles.back().y = bounds.getY() + rng.nextFloat() * bounds.getHeight() * 0.65f;
        particles.back().age = particles.back().maxAge * rng.nextFloat();
    }
}

void AlluviumParticleSystem::update (float dt, juce::Rectangle<float> bounds)
{
    for (auto& p : particles)
    {
        p.age += dt;
        p.y   += p.vy * dt;
    }

    // remove dead particles
    particles.erase (
        std::remove_if (particles.begin(), particles.end(),
            [] (const Particle& p) { return p.age >= p.maxAge; }),
        particles.end());

    // maintain steady rain count
    while ((int) particles.size() < kTargetAmbient)
        spawnRain (bounds);
}

void AlluviumParticleSystem::draw (juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    for (auto& p : particles)
    {
        float op = opacity (p);
        if (op <= 0.0f) continue;

        if (!bounds.expanded (p.size * 2.0f).contains (p.x, p.y)) continue;

        juce::Colour col = p.isGreen
                         ? AC::luminousGreen().withAlpha (op * 0.80f)
                         : AC::lavenderMist().withAlpha (op * 0.70f);

        g.setColour (col);
        // draw as thin vertical line segment
        g.drawLine (p.x, p.y, p.x, p.y + p.length, p.size);
    }
}
