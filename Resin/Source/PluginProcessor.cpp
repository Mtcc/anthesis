#include "PluginProcessor.h"
#include "PluginEditor.h"

ResinProcessor::ResinProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout ResinProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro1", "Macro 1",
                    juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro2", "Macro 2",
                    juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    return layout;
}

void ResinProcessor::prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/)
{
    isPrepared = true;
}

void ResinProcessor::releaseResources()
{
    isPrepared = false;
}

void ResinProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
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

juce::AudioProcessorEditor* ResinProcessor::createEditor()
{
    return new ResinEditor (*this);
}

bool ResinProcessor::hasEditor() const { return true; }
const juce::String ResinProcessor::getName() const { return "Resin"; }
bool ResinProcessor::acceptsMidi() const { return false; }
bool ResinProcessor::producesMidi() const { return false; }
bool ResinProcessor::isMidiEffect() const { return false; }
double ResinProcessor::getTailLengthSeconds() const { return 0.0; }
int ResinProcessor::getNumPrograms() { return 1; }
int ResinProcessor::getCurrentProgram() { return 0; }
void ResinProcessor::setCurrentProgram (int) {}
const juce::String ResinProcessor::getProgramName (int) { return {}; }
void ResinProcessor::changeProgramName (int, const juce::String&) {}

void ResinProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void ResinProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ResinProcessor();
}
