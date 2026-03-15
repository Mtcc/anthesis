#include "PluginProcessor.h"
#include "PluginEditor.h"

MyceliumProcessor::MyceliumProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout MyceliumProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro1", "Macro 1",
                    juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro2", "Macro 2",
                    juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    return layout;
}

void MyceliumProcessor::prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/)
{
    isPrepared = true;
}

void MyceliumProcessor::releaseResources()
{
    isPrepared = false;
}

void MyceliumProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
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

juce::AudioProcessorEditor* MyceliumProcessor::createEditor()
{
    return new MyceliumEditor (*this);
}

bool MyceliumProcessor::hasEditor() const { return true; }
const juce::String MyceliumProcessor::getName() const { return "Mycelium"; }
bool MyceliumProcessor::acceptsMidi() const { return false; }
bool MyceliumProcessor::producesMidi() const { return false; }
bool MyceliumProcessor::isMidiEffect() const { return false; }
double MyceliumProcessor::getTailLengthSeconds() const { return 0.0; }
int MyceliumProcessor::getNumPrograms() { return 1; }
int MyceliumProcessor::getCurrentProgram() { return 0; }
void MyceliumProcessor::setCurrentProgram (int) {}
const juce::String MyceliumProcessor::getProgramName (int) { return {}; }
void MyceliumProcessor::changeProgramName (int, const juce::String&) {}

void MyceliumProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void MyceliumProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MyceliumProcessor();
}
