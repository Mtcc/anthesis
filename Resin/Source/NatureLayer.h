#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

// tape hiss + vinyl crackle texture generator for Resin
class NatureLayer
{
public:
    NatureLayer() = default;
    void prepare (double sampleRate, int maxBlockSize);
    // fills buffer additively — call after existing DSP, before output
    void process (juce::AudioBuffer<float>& buffer, float amount) noexcept;

private:
    float pinkNoise() noexcept;

    // pink noise state (Paul Kellet)
    float b0=0,b1=0,b2=0,b3=0,b4=0,b5=0,b6=0;

    // hiss filter state (HPF removes sub-100Hz, LPF removes harsh top)
    float hissLpfL=0, hissLpfR=0, hissHpfL=0, hissHpfH2L=0; // L channel
    float hissLpfR2=0, hissHpfR=0, hissHpfH2R=0;             // R channel

    // crackle state
    float crackleEnvL=0, crackleEnvR=0;
    float crackleTimerL=0, crackleTimerR=0;
    float crackleLpfL=0, crackleLpfR=0;

    double sr = 44100.0;
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NatureLayer)
};
