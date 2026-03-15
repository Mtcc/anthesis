#include "PluginProcessor.h"
#include "PluginEditor.h"

AureoleProcessor::AureoleProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout AureoleProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro1", "Macro 1",
                    juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro2", "Macro 2",
                    juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    return layout;
}

void AureoleProcessor::prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/)
{
    isPrepared = true;
}

void AureoleProcessor::releaseResources()
{
    isPrepared = false;
}

void AureoleProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
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

juce::AudioProcessorEditor* AureoleProcessor::createEditor()
{
    return new AureoleEditor (*this);
}

bool AureoleProcessor::hasEditor() const { return true; }
const juce::String AureoleProcessor::getName() const { return "Aureole"; }
bool AureoleProcessor::acceptsMidi() const { return false; }
bool AureoleProcessor::producesMidi() const { return false; }
bool AureoleProcessor::isMidiEffect() const { return false; }
double AureoleProcessor::getTailLengthSeconds() const { return 0.0; }
int AureoleProcessor::getNumPrograms() { return 1; }
int AureoleProcessor::getCurrentProgram() { return 0; }
void AureoleProcessor::setCurrentProgram (int) {}
const juce::String AureoleProcessor::getProgramName (int) { return {}; }
void AureoleProcessor::changeProgramName (int, const juce::String&) {}

void AureoleProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void AureoleProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AureoleProcessor();
}
