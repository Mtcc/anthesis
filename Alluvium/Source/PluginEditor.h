#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "BotanicalLookAndFeel.h"
#include "ParticleSystem.h"

class AlluviumEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit AlluviumEditor (AlluviumProcessor&);
    ~AlluviumEditor() override;

    void paint             (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized           () override;

private:
    void timerCallback() override;

    // painting helpers
    void paintBackground  (juce::Graphics&);
    void paintStream      (juce::Graphics&);
    void paintSectionDivider (juce::Graphics&, float y);
    void paintFooter      (juce::Graphics&);

    // ── processor & state ─────────────────────────────────────────────────
    AlluviumProcessor& proc;
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

    // local scope snapshot
    float scopeIn  [AlluviumProcessor::kScopeSize] = {};
    float scopeOut [AlluviumProcessor::kScopeSize] = {};

    AlluviumParticleSystem particles;

    // ── look & feel ───────────────────────────────────────────────────────
    AlluviumLookAndFeel laf;

    // ── controls ──────────────────────────────────────────────────────────
    juce::Slider timeKnob, feedbackKnob, sedimentKnob, currentKnob, mixKnob, natureKnob;
    juce::Slider macro1Knob, macro2Knob;
    juce::Label  timeLabel, feedbackLabel, sedimentLabel, currentLabel, mixLabel, natureLabel;
    juce::Label  macro1Label, macro2Label;
    juce::ComboBox presetBox;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> timeAttach, feedbackAttach, sedimentAttach,
                                currentAttach, mixAttach, natureAttach,
                                macro1Attach, macro2Attach;

    static constexpr int kW = 700, kH = 480;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlluviumEditor)
};
