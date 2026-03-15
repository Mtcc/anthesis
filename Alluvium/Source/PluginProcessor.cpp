#include "PluginProcessor.h"
#include "PluginEditor.h"

AlluviumProcessor::AlluviumProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout AlluviumProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro1", "Macro 1",
                    juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro2", "Macro 2",
                    juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    return layout;
}

void AlluviumProcessor::prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/)
{
    isPrepared = true;
}

void AlluviumProcessor::releaseResources()
{
    isPrepared = false;
}

void AlluviumProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    if (!isPrepared) return;

    juce::ScopedNoDenormals noDenormals;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* data = buffer.getWritePointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            data[sample] = std::tanh (data[sample] * 0.9f) * 0.9f;
    }
}

juce::AudioProcessorEditor* AlluviumProcessor::createEditor()
{
    return new AlluviumEditor (*this);
}

bool AlluviumProcessor::hasEditor() const { return true; }
const juce::String AlluviumProcessor::getName() const { return "Alluvium"; }
bool AlluviumProcessor::acceptsMidi() const { return false; }
bool AlluviumProcessor::producesMidi() const { return false; }
bool AlluviumProcessor::isMidiEffect() const { return false; }
double AlluviumProcessor::getTailLengthSeconds() const { return 0.0; }
int AlluviumProcessor::getNumPrograms() { return 1; }
int AlluviumProcessor::getCurrentProgram() { return 0; }
void AlluviumProcessor::setCurrentProgram (int) {}
const juce::String AlluviumProcessor::getProgramName (int) { return {}; }
void AlluviumProcessor::changeProgramName (int, const juce::String&) {}

void AlluviumProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void AlluviumProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AlluviumProcessor();
}
