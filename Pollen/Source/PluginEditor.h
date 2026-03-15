#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class PollenEditor : public juce::AudioProcessorEditor
{
public:
    explicit PollenEditor (PollenProcessor&);
    ~PollenEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PollenProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PollenEditor)
};
