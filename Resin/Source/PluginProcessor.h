#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "NatureLayer.h"

class ResinProcessor : public juce::AudioProcessor
{
public:
    ResinProcessor();
    ~ResinProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Resin"; }
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

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // 4x oversampling, stereo
    juce::dsp::Oversampling<float> oversampler { 2, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false };

    // filters at original sample rate
    using IIRFilter = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;

    IIRFilter preEmphFilter;   // +4dB high shelf @ 3kHz before saturation
    IIRFilter deEmphFilter;    // -4dB high shelf @ 3kHz after saturation
    IIRFilter toneFilter;      // variable LPF 4kHz–18kHz

    // DC blockers (one per channel, stereo)
    float dcX[2] = {}, dcY[2] = {};

    NatureLayer natureLayer;
    float smoothNature = 0.0f;

    // block-level smoothed parameters (exponential, ~50ms)
    float smoothDrive = 0.3f, smoothAge = 0.3f, smoothTone = 0.7f;
    float smoothOut   = 0.7f, smoothMix  = 1.0f;

    bool isPrepared = false;
    double currentSampleRate = 44100.0;
    float lastToneSmooth = -1.0f; // track for filter coefficient updates

    // waveshaper core (inlined for performance)
    static float saturate (float x, float driveGain, float age) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResinProcessor)
};
