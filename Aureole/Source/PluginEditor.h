#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "BotanicalLookAndFeel.h"
#include "ParticleSystem.h"

class AureoleEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit AureoleEditor (AureoleProcessor&);
    ~AureoleEditor() override;

    void paint             (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized           () override;

private:
    void timerCallback() override;

    // ── painting helpers ──────────────────────────────────────────────────
    void paintBackground   (juce::Graphics&);
    void paintHalo         (juce::Graphics&);
    void paintSectionDivider (juce::Graphics&, float y);
    void paintFooter       (juce::Graphics&);

    // build vine path from scope data for waveform display inside Halo panel
    juce::Path buildVinePath (const float* samples, int count,
                               juce::Rectangle<float> area, float yScale) const;

    // ── processor & state ─────────────────────────────────────────────────
    AureoleProcessor& proc;
    juce::Random rng;

    // ── animation state ───────────────────────────────────────────────────
    float animTime    = 0.0f;
    float breathPhase = 0.0f;
    float lightDriftX = 0.0f;
    float lightDriftY = 0.0f;
    float macro1Phase = 0.0f;  // petal rotation phase for Drift knob
    float macro2Phase = 0.0f;

    bool  isIdle    = true;
    float idleTimer = 0.0f;
    float idleBreath = 0.0f;

    // current LFO phases — copied from processor each timer tick
    float lfoPhases[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    // amplitude for orbit radius pulse
    float signalAmplitude = 0.0f;

    // local scope snapshot (ring buffer copy from processor)
    float scopeIn  [AureoleProcessor::kScopeSize] = {};
    float scopeOut [AureoleProcessor::kScopeSize] = {};

    AureoleParticleSystem particles;

    // ── look & feel ───────────────────────────────────────────────────────
    AureoleLookAndFeel laf;

    // ── controls ──────────────────────────────────────────────────────────
    juce::Slider rateKnob, depthKnob, spreadKnob, warmthKnob, mixKnob;
    juce::Slider macro1Knob, macro2Knob;
    juce::Slider natureKnob;
    juce::Label  rateLabel, depthLabel, spreadLabel, warmthLabel, mixLabel;
    juce::Label  macro1Label, macro2Label;
    juce::Label  natureLabel;
    juce::ComboBox presetBox;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> rateAttach, depthAttach, spreadAttach,
                                warmthAttach, mixAttach,
                                macro1Attach, macro2Attach,
                                natureAttach;

    static constexpr int kW = 700, kH = 480;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AureoleEditor)
};
