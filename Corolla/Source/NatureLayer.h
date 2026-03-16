#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

// morning birdsong texture generator for Corolla
class NatureLayer
{
public:
    NatureLayer() = default;
    void prepare (double sampleRate, int maxBlockSize);
    void process (juce::AudioBuffer<float>& buffer, float amount) noexcept;

    static constexpr int kNumBirds = 4;
    struct Bird
    {
        float phase       = 0.0f;  // sine oscillator phase
        float freq        = 1200.0f; // current frequency Hz
        float freqSlope   = 0.0f;  // Hz/sample (glide up or down)
        float env         = 0.0f;  // current amplitude envelope
        float decayRate   = 0.995f;
        float timer       = 0.0f;  // seconds until next chirp
        float pan         = 0.0f;  // -1..+1
        int   trillLeft   = 0;     // remaining rapid trill repetitions
        float trillPause  = 0.0f;  // pause between trill pulses
        float baseFreqMin = 800.0f;
        float baseFreqMax = 1600.0f;
    };

private:
    float pinkNoise() noexcept;
    float b0=0,b1=0,b2=0,b3=0,b4=0,b5=0,b6=0;

    // soft background air (pink → LPF)
    float airLpfL=0, airLpfR=0;

    Bird birds[kNumBirds];

    static void triggerBird (Bird& b, juce::Random& rng, double sr);

    double sr = 44100.0;
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NatureLayer)
};
