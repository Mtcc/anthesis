#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include "NatureLayer.h"

class PollenProcessor : public juce::AudioProcessor
{
public:
    PollenProcessor();
    ~PollenProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Pollen"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

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
    float scopeInput  [kScopeSize] = {};
    float scopeOutput [kScopeSize] = {};
    std::atomic<int> scopeWritePos { 0 };

    // grain display data — written by audio thread, read by editor on timer tick
    struct GrainDisplay
    {
        float xNorm;  // 0-1, position in buffer
        float yNorm;  // 0-1, speed deviation mapped
        float env;    // envelope value 0-1
        bool  active;
    };
    GrainDisplay grainDisplay[16] = {};

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // granular grain state
    struct Grain
    {
        bool  active   = false;
        float readPos  = 0.0f;
        float speed    = 1.0f;
        float pan      = 0.0f;
        float age      = 0.0f;
        float duration = 0.1f;
        float gainL    = 0.5f;
        float gainR    = 0.5f;
    };

    static float grainEnvelope (float phase) noexcept;
    static float readBuf (const std::vector<float>& buf, float pos) noexcept;

    // circular input buffer (mono, allocated in prepareToPlay)
    std::vector<float> inputBuf;
    int   bufWritePos = 0;

    // grain pool — fixed-size, no dynamic allocation on audio thread
    Grain grains[16];

    // grain scheduling
    float spawnTimer = 0.0f;

    // block-level smoothed parameters
    float smoothScatter  = 0.3f;
    float smoothFloat    = 0.2f;
    float smoothGrainSize = 0.5f;
    float smoothDensity  = 0.6f;
    float smoothSpread   = 0.5f;
    float smoothMix      = 0.6f;

    NatureLayer natureLayer;
    float smoothNature = 0.0f;

    bool   isPrepared      = false;
    double currentSampleRate = 44100.0;

    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PollenProcessor)
};
