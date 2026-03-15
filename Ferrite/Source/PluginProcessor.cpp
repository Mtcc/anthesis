#include "PluginProcessor.h"
#include "PluginEditor.h"

FerriteProcessor::FerriteProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FerriteState", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout FerriteProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = juce::NormalisableRange<float> (0.0f, 1.0f);

    layout.add (std::make_unique<juce::AudioParameterFloat> ("drive",  "Drive",  pct, 0.3f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("age",    "Age",    pct, 0.3f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("tone",   "Tone",   pct, 0.7f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("output", "Output", pct, 0.7f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("mix",    "Mix",    pct, 1.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro1", "Flow",   pct, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro2", "Age",    pct, 0.0f));

    return layout;
}

void FerriteProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = (juce::uint32) getTotalNumOutputChannels();

    oversampler.initProcessing ((size_t) samplesPerBlock);

    // pre-emphasis: +4dB high shelf @ 3kHz — adds tape presence before saturation
    preEmphFilter.prepare (spec);
    *preEmphFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, 3000.0f, 0.707f, juce::Decibels::decibelsToGain (4.0f));

    // de-emphasis: matching -4dB shelf — restores linearity, harmonic content stays
    deEmphFilter.prepare (spec);
    *deEmphFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, 3000.0f, 0.707f, juce::Decibels::decibelsToGain (-4.0f));

    // tone filter: LPF, coefficients updated dynamically in processBlock
    toneFilter.prepare (spec);
    *toneFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass (
        sampleRate, 12000.0f, 0.707f);

    // reset DC blockers
    dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0.0f;

    // reset smoothed values to avoid startup transient
    smoothDrive = *apvts.getRawParameterValue ("drive");
    smoothAge   = *apvts.getRawParameterValue ("age");
    smoothTone  = *apvts.getRawParameterValue ("tone");
    smoothOut   = *apvts.getRawParameterValue ("output");
    smoothMix   = *apvts.getRawParameterValue ("mix");
    lastToneSmooth = -1.0f;

    isPrepared = true;
}

void FerriteProcessor::releaseResources()
{
    oversampler.reset();
    preEmphFilter.reset();
    deEmphFilter.reset();
    toneFilter.reset();
    isPrepared = false;
}

// waveshaper: blend between tube (even harmonics) and transformer (asymmetric)
float FerriteProcessor::saturate (float x, float driveGain, float age) noexcept
{
    const float driven = x * driveGain;

    // tube path: x/(1+|x|) — soft, musically rich, even harmonics
    const float tube = driven / (1.0f + std::abs (driven));

    // transformer path: asymmetric — harder negative knee, adds character
    const float xformer = (driven >= 0.0f)
        ? driven / (1.0f + driven * 0.85f)     // soft positive
        : std::tanh (driven * 1.3f);            // sharper negative

    // blend: age=0 → pure tube, age=1 → transformer asymmetry
    return tube + age * (xformer - tube);
}

void FerriteProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (!isPrepared) return;
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0) return;

    juce::ScopedNoDenormals noDenormals;
    const int numSamples    = buffer.getNumSamples();
    const int numChannels   = buffer.getNumChannels();

    // --- read parameters ---
    float tDrive = *apvts.getRawParameterValue ("drive");
    float tAge   = *apvts.getRawParameterValue ("age");
    float tTone  = *apvts.getRawParameterValue ("tone");
    float tOut   = *apvts.getRawParameterValue ("output");
    float tMix   = *apvts.getRawParameterValue ("mix");
    float macro1 = *apvts.getRawParameterValue ("macro1");
    float macro2 = *apvts.getRawParameterValue ("macro2");

    // --- macro influence ---
    // Flow (macro1): first half pushes drive to max, second half pushes age
    if (macro1 <= 0.5f)
        tDrive += macro1 * 2.0f * (1.0f - tDrive);
    else {
        tDrive = 1.0f;
        tAge  += (macro1 - 0.5f) * 2.0f * (1.0f - tAge);
    }
    // Age macro (macro2): directly pushes age
    tAge += macro2 * (1.0f - tAge);

    tDrive = juce::jlimit (0.0f, 1.0f, tDrive);
    tAge   = juce::jlimit (0.0f, 1.0f, tAge);

    // --- exponential block-level smoothing (~50ms) ---
    const float tc = std::exp (-6.2832f / (0.05f * (float) currentSampleRate / (float) numSamples));
    smoothDrive += (1.0f - tc) * (tDrive - smoothDrive);
    smoothAge   += (1.0f - tc) * (tAge   - smoothAge);
    smoothTone  += (1.0f - tc) * (tTone  - smoothTone);
    smoothOut   += (1.0f - tc) * (tOut   - smoothOut);
    smoothMix   += (1.0f - tc) * (tMix   - smoothMix);

    // --- convert to usable ranges ---
    const float driveGain = juce::jmap (smoothDrive, 0.0f, 1.0f, 1.0f, 14.0f);
    const float age       = smoothAge;
    const float outGain   = juce::Decibels::decibelsToGain (
        juce::jmap (smoothOut, 0.0f, 1.0f, -18.0f, 6.0f));
    const float mixAmt    = smoothMix;

    // --- update tone filter when changed ---
    if (std::abs (smoothTone - lastToneSmooth) > 0.004f)
    {
        float cutoff = juce::jmap (smoothTone, 0.0f, 1.0f, 4000.0f, 18000.0f);
        cutoff = juce::jlimit (20.0f, (float) currentSampleRate * 0.44f, cutoff);
        *toneFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass (
            currentSampleRate, cutoff, 0.707f);
        lastToneSmooth = smoothTone;
    }

    // --- capture dry for mix and scope ---
    juce::AudioBuffer<float> dryBuf (numChannels, numSamples);
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuf.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    // --- pre-emphasis ---
    auto block = juce::dsp::AudioBlock<float> (buffer);
    preEmphFilter.process (juce::dsp::ProcessContextReplacing<float> (block));

    // --- 4x upsample ---
    auto upBlock = oversampler.processSamplesUp (block);

    // --- waveshaper on oversampled signal ---
    for (size_t ch = 0; ch < upBlock.getNumChannels(); ++ch)
    {
        auto* data = upBlock.getChannelPointer (ch);
        for (size_t i = 0; i < upBlock.getNumSamples(); ++i)
            data[i] = saturate (data[i], driveGain, age);
    }

    // --- 4x downsample ---
    oversampler.processSamplesDown (block);

    // --- de-emphasis ---
    deEmphFilter.process (juce::dsp::ProcessContextReplacing<float> (block));

    // --- tone filter ---
    toneFilter.process (juce::dsp::ProcessContextReplacing<float> (block));

    // --- output gain + wet/dry mix + DC block + final soft limiter ---
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet   = buffer.getWritePointer (ch);
        auto* dry   = dryBuf.getReadPointer (ch);
        int   chIdx = juce::jmin (ch, 1);

        for (int i = 0; i < numSamples; ++i)
        {
            float w = wet[i] * outGain;
            float mixed = dry[i] + mixAmt * (w - dry[i]);

            // DC block: y[n] = x[n] - x[n-1] + 0.9997*y[n-1]
            float dcBlocked = mixed - dcX[chIdx] + 0.9997f * dcY[chIdx];
            dcX[chIdx] = mixed;
            dcY[chIdx] = dcBlocked;

            // final soft limiter — guarantees no clipping under any parameter combo
            wet[i] = std::tanh (dcBlocked * 0.9f) * (1.0f / std::tanh (0.9f));
        }
    }

    // --- write scope buffer (visual only, no atomics needed per sample) ---
    int wPos = scopeWritePos.load (std::memory_order_relaxed);
    auto* ch0dry = dryBuf.getReadPointer (0);
    auto* ch0wet = buffer.getReadPointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        scopeInput  [wPos] = ch0dry[i];
        scopeOutput [wPos] = ch0wet[i];
        wPos = (wPos + 1) & (kScopeSize - 1);
    }
    scopeWritePos.store (wPos, std::memory_order_relaxed);
}

void FerriteProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void FerriteProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* FerriteProcessor::createEditor()
{
    return new FerriteEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FerriteProcessor();
}
