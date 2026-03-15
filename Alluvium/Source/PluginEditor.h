#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class AlluviumEditor : public juce::AudioProcessorEditor
{
public:
    explicit AlluviumEditor (AlluviumProcessor&);
    ~AlluviumEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AlluviumProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlluviumEditor)
};
