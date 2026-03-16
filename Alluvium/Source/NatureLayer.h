#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

// running water / babbling stream texture generator for Alluvium
class NatureLayer
{
public:
    NatureLayer() = default;
    void prepare (double sampleRate, int maxBlockSize);
    void process (juce::AudioBuffer<float>& buffer, float amount) noexcept;

private:
    float pinkNoise() noexcept;
    float b0=0,b1=0,b2=0,b3=0,b4=0,b5=0,b6=0;

    // 3 slowly-sweeping bandpass formants (simulate stream burbling)
    struct Formant
    {
        float lpf1L=0, lpf2L=0; // two LPFs per channel (L)
        float lpf1R=0, lpf2R=0; // (R)
        float lfoPhase=0;
        float lfoRate=0;        // Hz
        float centerFreq=0;     // Hz base
        float sweepRange=0;     // Hz peak-to-peak sweep
    };
    static constexpr int kNumFormants = 3;
    Formant formants[kNumFormants];

    // Low rumble for depth
    float rumbleLpfL=0, rumbleLpfR=0;

    double sr = 44100.0;
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NatureLayer)
};
