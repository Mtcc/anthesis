#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

// rising spore seasonal particles for Mycelium
class MyceliumParticleSystem
{
public:
    explicit MyceliumParticleSystem (juce::Random& rng);

    // call from Timer callback, dt in seconds
    void update (float dt, juce::Rectangle<float> bounds);
    void draw   (juce::Graphics&, juce::Rectangle<float> bounds) const;
    void reset  (juce::Rectangle<float> bounds);

private:
    struct Particle
    {
        float x, y;
        float vx, vy;
        float age, maxAge;
        float size;
        bool  isGreen; // false = lavenderMist, true = luminousGreen
        bool  isSporeBurst;
    };

    float opacity (const Particle& p) const noexcept;
    void  spawnAmbient    (juce::Rectangle<float> bounds);
    void  spawnSporeBurst (juce::Rectangle<float> bounds);

    std::vector<Particle> particles;
    juce::Random& rng;
    float spawnTimer = 0.0f;

    static constexpr int   kTargetAmbient  = 8;
    static constexpr float kBurstInterval  = 3.2f;
};
