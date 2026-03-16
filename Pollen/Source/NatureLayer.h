#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

// rustling leaves + wind-through-grass texture generator for Pollen
class NatureLayer
{
public:
    NatureLayer() = default;
    void prepare (double sampleRate, int maxBlockSize);
    void process (juce::AudioBuffer<float>& buffer, float amount) noexcept;

private:
    float pinkNoise() noexcept;
    float b0=0,b1=0,b2=0,b3=0,b4=0,b5=0,b6=0;

    // HPF filter state for leaf texture (remove low-end, keep 2-12kHz rustle)
    float hpfL1=0, hpfL2=0;  // L channel two-pole HPF
    float hpfR1=0, hpfR2=0;  // R channel two-pole HPF

    // Gust amplitude envelope state
    float gustLevel   = 0.4f;
    float gustTarget  = 0.4f;
    float gustTimer   = 0.0f;

    // Occasional single sharp rustle (a leaf dropping)
    float rustleEnvL  = 0.0f, rustleEnvR  = 0.0f;
    float rustleTimer = 0.0f;
    float rustleDecay = 0.98f;

    double sr = 44100.0;
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NatureLayer)
};
