#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

// horizontal pollen drift particles for Pollen
class PollenParticleSystem
{
public:
    explicit PollenParticleSystem (juce::Random& rng);

    // call from Timer callback, dt in seconds
    void update (float dt, juce::Rectangle<float> bounds);
    void draw   (juce::Graphics&, juce::Rectangle<float> bounds) const;
    void reset  (juce::Rectangle<float> bounds);

private:
    struct Particle
    {
        float x, y;
        float vx;       // horizontal drift speed (px/sec, positive = rightward)
        float age;
        float size;     // 1.5-3px
        int   index;    // for sine phase offset
        bool  isGolden; // golden vs luminous green
    };

    void spawnAt (juce::Rectangle<float> bounds, float startX);

    std::vector<Particle> particles;
    juce::Random& rng;

    static constexpr int kTargetCount = 9;
};
