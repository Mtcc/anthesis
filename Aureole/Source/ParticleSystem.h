#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

// firefly drift seasonal particles for Aureole
// particles move rightward in gentle sine curves, blink (opacity pulses 1-2Hz)
class AureoleParticleSystem
{
public:
    explicit AureoleParticleSystem (juce::Random& rng);

    // call from Timer callback, dt in seconds
    void update (float dt, juce::Rectangle<float> bounds);
    void draw   (juce::Graphics&, juce::Rectangle<float> bounds) const;
    void reset  (juce::Rectangle<float> bounds);

private:
    struct Particle
    {
        float x, y;
        float vx;          // rightward drift speed (px/sec)
        float age;         // used for sine vertical motion
        float blinkPhase;  // drives opacity pulse
        float blinkRate;   // Hz, 1.0–2.0
        float size;        // 2.5–4px
        bool  isAmber;     // true = WarmAmber, false = GoldenHour
    };

    void spawnFirefly (juce::Rectangle<float> bounds, float startX = -1.0f);

    std::vector<Particle> particles;
    juce::Random& rng;

    static constexpr int   kTarget = 12;     // steady-state firefly count
    static constexpr float kVxMin  = 15.0f;  // px/sec minimum rightward speed
    static constexpr float kVxMax  = 25.0f;  // px/sec maximum rightward speed
};
