#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "NatureLayer.h"

class MyceliumProcessor : public juce::AudioProcessor
{
public:
    MyceliumProcessor();
    ~MyceliumProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Mycelium"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // scope ring buffer — read by editor, written by audio thread
    static constexpr int kScopeSize = 512;
    float scopeInput[kScopeSize]  = {};
    float scopeOutput[kScopeSize] = {};
    std::atomic<int> scopeWritePos { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ── Schroeder reverb building blocks ─────────────────────────────────

    struct CombFilter
    {
        std::vector<float> buf;
        int pos = 0;
        float filterStore = 0.0f;

        void prepare (int delaySamples)
        {
            buf.assign ((size_t)(delaySamples + 1), 0.0f);
            pos = 0;
            filterStore = 0.0f;
        }

        float process (float input, float feedback, float damping) noexcept
        {
            float output = buf[(size_t)pos];
            filterStore = output * (1.0f - damping) + filterStore * damping;
            buf[(size_t)pos] = input + filterStore * feedback;
            if (++pos >= (int)buf.size()) pos = 0;
            return output;
        }

        void resize (int newSize)
        {
            buf.assign ((size_t)(newSize + 1), 0.0f);
            pos = 0;
            filterStore = 0.0f;
        }
    };

    struct AllpassFilter
    {
        std::vector<float> buf;
        int pos = 0;

        void prepare (int delaySamples)
        {
            buf.assign ((size_t)(delaySamples + 1), 0.0f);
            pos = 0;
        }

        float process (float input, float g = 0.5f) noexcept
        {
            float delayed = buf[(size_t)pos];
            float w = input + g * delayed;
            buf[(size_t)pos] = w;
            if (++pos >= (int)buf.size()) pos = 0;
            return delayed - g * w;
        }
    };

    // 4 comb filters per channel, stereo
    CombFilter combL[4], combR[4];

    // 4 series allpass diffusers per channel (separate state for true stereo)
    AllpassFilter allpassL[4], allpassR[4];

    // pre-delay circular buffer (up to 80ms per channel)
    static constexpr int kMaxPreDelayMs = 80;
    std::vector<float> preDelayBufL, preDelayBufR;
    int preDelayPos = 0;
    int preDelayLen = 1;

    // base comb delay times at 44100 Hz
    static constexpr int kCombTimesL[4] = { 1557, 1617, 1491, 1422 };
    static constexpr int kCombTimesR[4] = { 1617, 1557, 1422, 1491 };
    static constexpr int kAllpassTimes[4] = { 225, 341, 441, 556 };

    // block-level smoothed parameters (~50ms exponential)
    float smoothCanopy   = 0.4f;
    float smoothPredelay = 0.2f;
    float smoothDecay    = 0.5f;
    float smoothDamping  = 0.5f;
    float smoothWidth    = 0.7f;
    float smoothMix      = 0.35f;
    float smoothNature   = 0.0f;

    NatureLayer natureLayer;

    float lastCanopy = -1.0f; // forces comb resize on first prepareToPlay

    bool isPrepared = false;
    double currentSampleRate = 44100.0;

    void resizeCombs (float canopyNorm);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MyceliumProcessor)
};
