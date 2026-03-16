#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "BotanicalLookAndFeel.h"
#include "ParticleSystem.h"

class PollenEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit PollenEditor (PollenProcessor&);
    ~PollenEditor() override;

    void paint              (juce::Graphics&) override;
    void paintOverChildren  (juce::Graphics&) override;
    void resized            () override;

private:
    void timerCallback() override;

    // ── painting helpers ──────────────────────────────────────────────────
    void paintBackground   (juce::Graphics&);
    void paintPollenCloud  (juce::Graphics&);
    void paintSectionDivider (juce::Graphics&, float y);
    void paintFooter       (juce::Graphics&);

    // ── processor & state ─────────────────────────────────────────────────
    PollenProcessor& proc;
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

    // idle cloud: a few softly drifting dots when no audio is playing
    struct IdleDot { float x, y, vx, vy, env, phase; };
    IdleDot idleDots[4] = {};

    // local snapshot of grain display (copied from processor each timer tick)
    PollenProcessor::GrainDisplay grainSnap[16] = {};

    PollenParticleSystem particles;

    // ── look & feel ───────────────────────────────────────────────────────
    PollenLookAndFeel laf;

    // ── controls ──────────────────────────────────────────────────────────
    juce::Slider scatterKnob, floatKnob, grainSizeKnob, densityKnob, spreadKnob, mixKnob;
    juce::Slider macro1Knob, macro2Knob;
    juce::Slider natureKnob;
    juce::Label  scatterLabel, floatLabel, grainSizeLabel, densityLabel, spreadLabel, mixLabel;
    juce::Label  macro1Label, macro2Label;
    juce::Label  natureLabel;
    juce::ComboBox presetBox;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> scatterAttach, floatAttach, grainSizeAttach,
                                densityAttach, spreadAttach, mixAttach,
                                macro1Attach, macro2Attach,
                                natureAttach;

    static constexpr int   kW = 700, kH = 480;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PollenEditor)
};
