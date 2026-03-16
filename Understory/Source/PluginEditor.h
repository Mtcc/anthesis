#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "BotanicalLookAndFeel.h"

class UnderstoryEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit UnderstoryEditor (UnderstoryProcessor&);
    ~UnderstoryEditor() override;

    void paint             (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized           () override;

private:
    void timerCallback() override;

    // ── painting helpers ──────────────────────────────────────────────────
    void paintBackground   (juce::Graphics&);
    void paintScope        (juce::Graphics&);
    void paintSectionDivider (juce::Graphics&, float y);
    void paintFooter       (juce::Graphics&);

    // ── processor & state ─────────────────────────────────────────────────
    UnderstoryProcessor& proc;
    juce::Random rng;

    // ── animation state ───────────────────────────────────────────────────
    float animTime    = 0.0f;
    float breathPhase = 0.0f;
    float macro1Phase = 0.0f;
    float macro2Phase = 0.0f;

    // local scope snapshot
    float scopeData[UnderstoryProcessor::kScopeSize] = {};

    // ── look & feel ───────────────────────────────────────────────────────
    UnderstoryLookAndFeel laf;

    // ── type selector ─────────────────────────────────────────────────────
    juce::ComboBox typeBox;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<ComboAttachment> typeAttach;

    // ── knobs ─────────────────────────────────────────────────────────────
    juce::Slider densityKnob, toneKnob, textureKnob, widthKnob, levelKnob;
    juce::Slider macro1Knob, macro2Knob;

    juce::Label  densityLabel, toneLabel, textureLabel, widthLabel, levelLabel;
    juce::Label  macro1Label, macro2Label;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> densityAttach, toneAttach, textureAttach,
                                widthAttach, levelAttach,
                                macro1Attach, macro2Attach;

    static constexpr int kW = 700, kH = 480;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UnderstoryEditor)
};
