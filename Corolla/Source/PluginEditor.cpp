#include "PluginEditor.h"

CorollaEditor::CorollaEditor (CorollaProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (600, 400);
}

void CorollaEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF0A0F0A));

    g.setColour (juce::Colour (0xFFF0E6D3));
    g.setFont (juce::Font (36.0f, juce::Font::plain));
    g.drawText ("Corolla", getLocalBounds(), juce::Justification::centred, true);
}

void CorollaEditor::resized()
{
}
