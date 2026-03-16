#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

// wind chimes + breeze texture generator for Aureole
class NatureLayer
{
public:
    NatureLayer() = default;
    void prepare (double sampleRate, int maxBlockSize);
    void process (juce::AudioBuffer<float>& buffer, float amount) noexcept;

private:
    float pinkNoise() noexcept;
    float b0=0,b1=0,b2=0,b3=0,b4=0,b5=0,b6=0;

    // breeze filter state
    float breezeLpfL=0, breezeLpfR=0;

    // 8 tuned chime oscillators at pentatonic intervals
    static constexpr int kNumChimes = 8;
    struct Chime
    {
        float phase=0;
        float freq=440;       // base Hz
        float detune=0;       // fractional detune applied at trigger
        float env=0;          // current amplitude envelope
        float decayRate=0.9998f; // per-sample decay
        float timer=0;        // seconds until next trigger
        float pan=0;          // -1 to +1
    };
    Chime chimes[kNumChimes];

    double sr = 44100.0;
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NatureLayer)
};
