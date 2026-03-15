#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class AureoleEditor : public juce::AudioProcessorEditor
{
public:
    explicit AureoleEditor (AureoleProcessor&);
    ~AureoleEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AureoleProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AureoleEditor)
};
