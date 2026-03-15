#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <random>
#include <vector>

// ambient + amber-drip seasonal particles for Resin
class ParticleSystem
{
public:
    explicit ParticleSystem (juce::Random& rng);

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
        bool  isAmberDrip;
        bool  isGreen;     // false = warmAmber
    };

    float opacity (const Particle& p) const noexcept;
    void  spawnAmbient   (juce::Rectangle<float> bounds);
    void  spawnAmberDrip (juce::Rectangle<float> bounds);

    std::vector<Particle> particles;
    juce::Random& rng;
    float spawnTimer = 0.0f;

    static constexpr int  kTargetAmbient = 7;
    static constexpr float kDripInterval = 2.8f;
};
