#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

// rain + distant insects texture generator for Mycelium
class NatureLayer
{
public:
    NatureLayer() = default;
    void prepare (double sampleRate, int maxBlockSize);
    void process (juce::AudioBuffer<float>& buffer, float amount) noexcept;

private:
    float pinkNoise() noexcept;

    // pink noise state (Paul Kellet 7-filter approximation)
    float b0=0,b1=0,b2=0,b3=0,b4=0,b5=0,b6=0;

    // rain hiss filter states
    float hissHpfL=0, hissHpfR=0, hissHpfH2L=0, hissHpfH2R=0;

    // raindrop pool
    static constexpr int kMaxDrops = 16;
    struct Drop {
        float envL=0, envR=0;
        float lpfStateL=0, lpfStateR=0;
        float decayRate=0.98f;
        float gainL=0.5f, gainR=0.5f;
    };
    Drop drops[kMaxDrops];
    float rainTimer = 0.0f;

    // cricket state
    struct Cricket {
        float phase=0, chirpPhase=0;
        float freq=4000, chirpRate=18;
        float env=0, decayRate=0.9997f;
        float timer=0;
    };
    Cricket crickets[3];

    double sr = 44100.0;
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NatureLayer)
};
