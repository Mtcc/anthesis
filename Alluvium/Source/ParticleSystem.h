#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

// edge rain particles for Alluvium — thin vertical streaks from top
class AlluviumParticleSystem
{
public:
    explicit AlluviumParticleSystem (juce::Random& rng);

    // call from timer callback, dt in seconds
    void update (float dt, juce::Rectangle<float> bounds);
    void draw   (juce::Graphics&, juce::Rectangle<float> bounds) const;
    void reset  (juce::Rectangle<float> bounds);

private:
    struct Particle
    {
        float x, y;
        float vx, vy;
        float age, maxAge;
        float size;    // base size for stroke width
        float length;  // visual streak length = size * 3
        bool  isGreen; // false = lavenderMist (70%), true = luminousGreen (30%)
    };

    float opacity (const Particle& p) const noexcept;
    void  spawnRain (juce::Rectangle<float> bounds);

    std::vector<Particle> particles;
    juce::Random& rng;

    static constexpr int kTargetAmbient = 10;
};
