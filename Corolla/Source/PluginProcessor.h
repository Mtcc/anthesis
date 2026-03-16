#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "NatureLayer.h"

class CorollaProcessor : public juce::AudioProcessor
{
public:
    CorollaProcessor();
    ~CorollaProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Corolla"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

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

    // bloom display level for UI flower (0.0-1.0), updated each block
    std::atomic<float> bloomDisplay { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // 6 stereo bandpass resonators
    using IIRFilter = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;

    static constexpr int kNumResonators = 6;
    IIRFilter resonators[kNumResonators];

    // fixed center frequencies — broad harmonic colour coverage
    static constexpr float kCenterFreqs[kNumResonators]    = { 200.0f, 400.0f, 600.0f, 800.0f, 1200.0f, 2000.0f };

    // harmonic weight falloff (higher harmonics are quieter)
    static constexpr float kHarmonicWeights[kNumResonators] = { 1.0f, 0.7f, 0.5f, 0.35f, 0.25f, 0.18f };

    // bloom envelope state: one-pole smoother per resonator
    float bloomEnvelope[kNumResonators] = {};

    NatureLayer natureLayer;

    // block-level smoothed parameters
    float smoothBloom   = 0.3f;
    float smoothUnfurl  = 0.5f;
    float smoothShimmer = 0.2f;
    float smoothTilt    = 0.5f;
    float smoothMix     = 0.4f;
    float smoothNature  = 0.0f;

    bool   isPrepared = false;
    double currentSampleRate = 44100.0;

    // cached values to track coefficient update need
    float lastShimmer = -1.0f;
    float lastTilt    = -1.0f;

    void updateFilterCoefficients (float shimmer, float tilt);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CorollaProcessor)
};
