#include "PluginProcessor.h"
#include "PluginEditor.h"

PollenProcessor::PollenProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PollenProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro1", "Macro 1",
                    juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro2", "Macro 2",
                    juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    return layout;
}

void PollenProcessor::prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/)
{
    isPrepared = true;
}

void PollenProcessor::releaseResources()
{
    isPrepared = false;
}

void PollenProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
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

juce::AudioProcessorEditor* PollenProcessor::createEditor()
{
    return new PollenEditor (*this);
}

bool PollenProcessor::hasEditor() const { return true; }
const juce::String PollenProcessor::getName() const { return "Pollen"; }
bool PollenProcessor::acceptsMidi() const { return false; }
bool PollenProcessor::producesMidi() const { return false; }
bool PollenProcessor::isMidiEffect() const { return false; }
double PollenProcessor::getTailLengthSeconds() const { return 0.0; }
int PollenProcessor::getNumPrograms() { return 1; }
int PollenProcessor::getCurrentProgram() { return 0; }
void PollenProcessor::setCurrentProgram (int) {}
const juce::String PollenProcessor::getProgramName (int) { return {}; }
void PollenProcessor::changeProgramName (int, const juce::String&) {}

void PollenProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PollenProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PollenProcessor();
}
