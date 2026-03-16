#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include "NatureLayer.h"

class AlluviumProcessor : public juce::AudioProcessor
{
public:
    AlluviumProcessor();
    ~AlluviumProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Alluvium"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

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

    // smoothed parameter values exposed for display in editor
    float smoothSediment = 0.3f;
    float smoothCurrent  = 0.3f;

    NatureLayer natureLayer;
    float smoothNature = 0.0f;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // delay line circular buffers: 2s @ 192kHz + headroom
    std::vector<float> delayBufL, delayBufR;
    int delayWritePosL = 0, delayWritePosR = 0;

    // wow/flutter LFO state
    float wowPhase      = 0.0f;   // 0.4 Hz wow
    float flutterPhase  = 0.0f;   // ~8 Hz flutter
    float flutterRandom = 0.0f;   // random walk on flutter rate (±2 Hz)

    // one-pole LPF in feedback path
    float lpfStateL = 0.0f, lpfStateR = 0.0f;

    // current drift state
    float driftTarget = 0.0f;
    float driftActual = 0.0f;
    float driftTimer  = 0.0f;   // time since last drift target change

    // smoothed parameters
    float smoothTime     = 0.5f;
    float smoothFeedback = 0.4f;
    float smoothMix      = 0.4f;

    bool   isPrepared        = false;
    double currentSampleRate = 44100.0;

    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlluviumProcessor)
};
