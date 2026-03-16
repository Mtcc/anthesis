#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "BotanicalLookAndFeel.h"
#include "ParticleSystem.h"

class ResinEditor : public juce::AudioProcessorEditor,
                    private juce::Timer
{
public:
    explicit ResinEditor (ResinProcessor&);
    ~ResinEditor() override;

    void paint              (juce::Graphics&) override;
    void paintOverChildren  (juce::Graphics&) override;
    void resized            () override;

private:
    void timerCallback() override;

    // ── painting helpers ──────────────────────────────────────────────────
    void paintBackground     (juce::Graphics&);
    void paintAmberGarden    (juce::Graphics&);
    void paintSectionDivider (juce::Graphics&, float y);
    void paintFooter         (juce::Graphics&);

    // draw organic vine path from scope data
    juce::Path buildVinePath (const float* samples, int count,
                               juce::Rectangle<float> area, float yScale) const;

    // ── processor & state ─────────────────────────────────────────────────
    ResinProcessor& proc;
    juce::Random rng;

    // ── animation state ───────────────────────────────────────────────────
    float animTime      = 0.0f;
    float breathPhase   = 0.0f;
    float lightDriftX   = 0.0f;
    float lightDriftY   = 0.0f;
    float macro1Phase   = 0.0f; // petal phase offset for Flow knob
    float macro2Phase   = 0.0f;

    bool  isIdle        = true;
    float idleTimer     = 0.0f;
    float idleBreath    = 0.0f;

    // local scope snapshot (read from processor ring buffer)
    float scopeIn  [ResinProcessor::kScopeSize] = {};
    float scopeOut [ResinProcessor::kScopeSize] = {};
    int   scopeReadPos = 0;

    ParticleSystem particles;

    // ── look & feel ───────────────────────────────────────────────────────
    BotanicalLookAndFeel laf;

    // ── controls ──────────────────────────────────────────────────────────
    juce::Slider driveKnob, ageKnob, toneKnob, outputKnob, mixKnob, natureKnob;
    juce::Slider macro1Knob, macro2Knob;  // macro (larger)
    juce::Label  driveLabel, ageLabel, toneLabel, outputLabel, mixLabel, natureLabel;
    juce::Label  macro1Label, macro2Label;
    juce::ComboBox presetBox;

    // APVTS attachments
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> driveAttach, ageAttach, toneAttach,
                                outputAttach, mixAttach, natureAttach,
                                macro1Attach, macro2Attach;

    static constexpr int kW = 700, kH = 480;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResinEditor)
};
