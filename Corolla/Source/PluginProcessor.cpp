#include "PluginProcessor.h"
#include "PluginEditor.h"

CorollaProcessor::CorollaProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout CorollaProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro1", "Macro 1",
                    juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro2", "Macro 2",
                    juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    return layout;
}

void CorollaProcessor::prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/)
{
    isPrepared = true;
}

void CorollaProcessor::releaseResources()
{
    isPrepared = false;
}

void CorollaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
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

juce::AudioProcessorEditor* CorollaProcessor::createEditor()
{
    return new CorollaEditor (*this);
}

bool CorollaProcessor::hasEditor() const { return true; }
const juce::String CorollaProcessor::getName() const { return "Corolla"; }
bool CorollaProcessor::acceptsMidi() const { return false; }
bool CorollaProcessor::producesMidi() const { return false; }
bool CorollaProcessor::isMidiEffect() const { return false; }
double CorollaProcessor::getTailLengthSeconds() const { return 0.0; }
int CorollaProcessor::getNumPrograms() { return 1; }
int CorollaProcessor::getCurrentProgram() { return 0; }
void CorollaProcessor::setCurrentProgram (int) {}
const juce::String CorollaProcessor::getProgramName (int) { return {}; }
void CorollaProcessor::changeProgramName (int, const juce::String&) {}

void CorollaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void CorollaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CorollaProcessor();
}
