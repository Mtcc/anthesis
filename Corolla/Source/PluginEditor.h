#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class CorollaEditor : public juce::AudioProcessorEditor
{
public:
    explicit CorollaEditor (CorollaProcessor&);
    ~CorollaEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    CorollaProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CorollaEditor)
};
