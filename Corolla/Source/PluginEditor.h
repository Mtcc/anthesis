#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "BotanicalLookAndFeel.h"
#include "ParticleSystem.h"

class CorollaEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit CorollaEditor (CorollaProcessor&);
    ~CorollaEditor() override;

    void paint             (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized           () override;

private:
    void timerCallback() override;

    // ── painting helpers ──────────────────────────────────────────────────
    void paintBackground     (juce::Graphics&);
    void paintBloomGarden    (juce::Graphics&);
    void paintSectionDivider (juce::Graphics&, float y);
    void paintFooter         (juce::Graphics&);

    // draw the flower bloom display
    void drawBloomFlower (juce::Graphics&, juce::Rectangle<float> area, float bloomLevel);

    // organic vine path from scope data
    juce::Path buildVinePath (const float* samples, int count,
                               juce::Rectangle<float> area, float yScale) const;

    // ── processor & state ─────────────────────────────────────────────────
    CorollaProcessor& proc;
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

    // current bloom level read from processor
    float bloomLevel = 0.0f;

    // local scope snapshot
    float scopeIn  [CorollaProcessor::kScopeSize] = {};
    float scopeOut [CorollaProcessor::kScopeSize] = {};

    CorollaParticleSystem particles;

    // ── look & feel ───────────────────────────────────────────────────────
    CorollaLookAndFeel laf;

    // ── controls ──────────────────────────────────────────────────────────
    juce::Slider bloomKnob, unfurlKnob, shimmerKnob, tiltKnob, mixKnob;
    juce::Slider macro1Knob, macro2Knob;
    juce::Slider natureKnob;
    juce::Label  bloomLabel, unfurlLabel, shimmerLabel, tiltLabel, mixLabel;
    juce::Label  macro1Label, macro2Label;
    juce::Label  natureLabel;
    juce::ComboBox presetBox;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> bloomAttach, unfurlAttach, shimmerAttach,
                                tiltAttach, mixAttach,
                                macro1Attach, macro2Attach;
    std::unique_ptr<Attachment> natureAttach;

    static constexpr int kW = 700, kH = 480;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CorollaEditor)
};
