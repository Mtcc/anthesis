#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

// petal-fall particle system for Corolla — elliptical petals slowly spiral down
class CorollaParticleSystem
{
public:
    explicit CorollaParticleSystem (juce::Random& rng);

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
        float rotation;
        float rotationSpeed;
        bool  isGreen;   // false = petalPink, true = lavenderMist
    };

    float opacity (const Particle& p) const noexcept;
    void  spawnAmbient (juce::Rectangle<float> bounds);
    void  spawnPetal   (juce::Rectangle<float> bounds);

    std::vector<Particle> particles;
    juce::Random& rng;
    float spawnTimer = 0.0f;

    static constexpr int   kTargetAmbient = 6;
    static constexpr float kBurstInterval = 3.5f;
};
