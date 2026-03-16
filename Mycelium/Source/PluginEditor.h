#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "BotanicalLookAndFeel.h"
#include "ParticleSystem.h"

class MyceliumEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit MyceliumEditor (MyceliumProcessor&);
    ~MyceliumEditor() override;

    void paint             (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized           () override;

private:
    void timerCallback() override;

    // ── painting helpers ──────────────────────────────────────────────────
    void paintBackground      (juce::Graphics&);
    void paintMyceliumNetwork (juce::Graphics&);
    void paintSectionDivider  (juce::Graphics&, float y);
    void paintFooter          (juce::Graphics&);

    // build waveform path from scope data
    juce::Path buildVinePath (const float* samples, int count,
                               juce::Rectangle<float> area, float yScale) const;

    // ── processor & state ─────────────────────────────────────────────────
    MyceliumProcessor& proc;
    juce::Random rng;

    // ── animation state ───────────────────────────────────────────────────
    float animTime    = 0.0f;
    float breathPhase = 0.0f;
    float lightDriftX = 0.0f;
    float lightDriftY = 0.0f;
    float macro1Phase = 0.0f;
    float macro2Phase = 0.0f;

    bool  isIdle    = true;
    float idleTimer = 0.0f;
    float idleBreath = 0.0f;
    float signalAmplitude = 0.0f; // smoothed amplitude for branch pulse

    // local scope snapshot (read from processor ring buffer)
    float scopeIn  [MyceliumProcessor::kScopeSize] = {};
    float scopeOut [MyceliumProcessor::kScopeSize] = {};

    MyceliumParticleSystem particles;

    // ── look & feel ───────────────────────────────────────────────────────
    MyceliumLookAndFeel laf;

    // ── controls ──────────────────────────────────────────────────────────
    juce::Slider canopyKnob, predelayKnob, decayKnob, dampingKnob, widthKnob, mixKnob;
    juce::Slider natureKnob;
    juce::Slider macro1Knob, macro2Knob;
    juce::Label  canopyLabel, predelayLabel, decayLabel, dampingLabel, widthLabel, mixLabel;
    juce::Label  natureLabel;
    juce::Label  macro1Label, macro2Label;
    juce::ComboBox presetBox;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> canopyAttach, predelayAttach, decayAttach,
                                dampingAttach, widthAttach, mixAttach,
                                natureAttach,
                                macro1Attach, macro2Attach;

    static constexpr int kW = 700, kH = 480;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MyceliumEditor)
};
