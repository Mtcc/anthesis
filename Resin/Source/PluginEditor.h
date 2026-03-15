#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class ResinEditor : public juce::AudioProcessorEditor
{
public:
    explicit ResinEditor (ResinProcessor&);
    ~ResinEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ResinProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResinEditor)
};
