#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class MyceliumEditor : public juce::AudioProcessorEditor
{
public:
    explicit MyceliumEditor (MyceliumProcessor&);
    ~MyceliumEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MyceliumProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MyceliumEditor)
};
