#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "NatureLayer.h"

// linear interpolating delay line — no JUCE DSP dependency
struct DelayLine
{
    std::vector<float> buf;
    int writePos = 0;

    void prepare (int maxSamples)
    {
        buf.assign ((size_t)(maxSamples + 1), 0.0f);
        writePos = 0;
    }

    void write (float x)
    {
        buf[(size_t)writePos] = x;
        if (++writePos >= (int)buf.size()) writePos = 0;
    }

    float read (float delaySamples) const
    {
        float readPos = (float)writePos - delaySamples - 1.0f;
        while (readPos < 0.0f) readPos += (float)buf.size();
        int i0 = (int)readPos % (int)buf.size();
        int i1 = (i0 + 1) % (int)buf.size();
        float frac = readPos - (float)(int)readPos;
        return buf[(size_t)i0] + frac * (buf[(size_t)i1] - buf[(size_t)i0]);
    }
};

class AureoleProcessor : public juce::AudioProcessor
{
public:
    AureoleProcessor();
    ~AureoleProcessor() override = default;

    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock   (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName()    const override { return "Aureole"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int  getNumPrograms()                             override { return 1; }
    int  getCurrentProgram()                          override { return 0; }
    void setCurrentProgram (int)                      override {}
    const juce::String getProgramName (int)           override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // scope ring buffer — written by audio thread, read by editor
    static constexpr int kScopeSize = 512;
    float scopeInput  [kScopeSize] = {};
    float scopeOutput [kScopeSize] = {};
    std::atomic<int> scopeWritePos { 0 };

    // LFO phases for Halo display — written each processBlock, read by editor timer
    float lfoPhaseReadout[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // 4-voice stereo chorus: 2 delay lines per voice (L + R)
    static constexpr int kNumVoices   = 4;
    static constexpr int kMaxDelayBuf = 10000; // covers 50ms @ 192kHz

    DelayLine delayL[kNumVoices];
    DelayLine delayR[kNumVoices];

    // per-voice LFO state — initialised in prepareToPlay
    float lfoPhase[kNumVoices];

    // base delay times per voice (ms)
    static constexpr float kBaseDelayMs[kNumVoices] = { 7.0f, 11.0f, 15.0f, 23.0f };

    // one-pole LPF for warmth filter (one state per channel)
    float lpfState[2]      = { 0.0f, 0.0f };
    float warmthCoeff      = 1.0f;
    float lastWarmthSmooth = -1.0f;

    // block-level exponentially smoothed parameters (~50ms)
    float smoothRate   = 0.3f;
    float smoothDepth  = 0.4f;
    float smoothSpread = 0.8f;
    float smoothWarmth = 0.4f;
    float smoothMix    = 0.5f;

    NatureLayer natureLayer;
    float smoothNature = 0.0f;

    bool   isPrepared        = false;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AureoleProcessor)
};
