#include "PluginProcessor.h"
#include "PluginEditor.h"


CorollaProcessor::CorollaProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CorollaState", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout CorollaProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = juce::NormalisableRange<float> (0.0f, 1.0f);

    layout.add (std::make_unique<juce::AudioParameterFloat> ("bloom",   "Bloom",   pct, 0.3f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("unfurl",  "Unfurl",  pct, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("shimmer", "Shimmer", pct, 0.2f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("tilt",    "Tilt",    pct, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("mix",     "Mix",     pct, 0.4f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro1",  "Bloom",   pct, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro2",  "Unfurl",  pct, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("nature",  "Nature",  pct, 0.0f));

    return layout;
}

void CorollaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = (juce::uint32) getTotalNumOutputChannels();

    for (int i = 0; i < kNumResonators; ++i)
    {
        resonators[i].prepare (spec);
        *resonators[i].state = *juce::dsp::IIR::Coefficients<float>::makeBandPass (
            sampleRate, kCenterFreqs[i], 2.0f);
        bloomEnvelope[i] = 0.0f;
    }

    // initialise smoothed params to avoid startup transient
    smoothBloom   = *apvts.getRawParameterValue ("bloom");
    smoothUnfurl  = *apvts.getRawParameterValue ("unfurl");
    smoothShimmer = *apvts.getRawParameterValue ("shimmer");
    smoothTilt    = *apvts.getRawParameterValue ("tilt");
    smoothMix     = *apvts.getRawParameterValue ("mix");

    lastShimmer = -1.0f;
    lastTilt    = -1.0f;

    natureLayer.prepare (sampleRate, samplesPerBlock);
    smoothNature = *apvts.getRawParameterValue ("nature");

    isPrepared = true;
}

void CorollaProcessor::releaseResources()
{
    for (int i = 0; i < kNumResonators; ++i)
        resonators[i].reset();
    isPrepared = false;
}

void CorollaProcessor::updateFilterCoefficients (float shimmer, float tilt)
{
    // shimmer: shift each resonator freq up by shimmer * 2 semitones
    float semitoneShift = shimmer * 2.0f;
    float freqMult = std::pow (2.0f, semitoneShift / 12.0f);

    for (int i = 0; i < kNumResonators; ++i)
    {
        float freq = kCenterFreqs[i] * freqMult;
        freq = juce::jlimit (20.0f, (float) currentSampleRate * 0.44f, freq);
        *resonators[i].state = *juce::dsp::IIR::Coefficients<float>::makeBandPass (
            currentSampleRate, freq, 2.0f);
    }

    lastShimmer = shimmer;
    lastTilt    = tilt;
}

void CorollaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (!isPrepared) return;
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0) return;

    juce::ScopedNoDenormals noDenormals;
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // --- read parameters ---
    float tBloom   = *apvts.getRawParameterValue ("bloom");
    float tUnfurl  = *apvts.getRawParameterValue ("unfurl");
    float tShimmer = *apvts.getRawParameterValue ("shimmer");
    float tTilt    = *apvts.getRawParameterValue ("tilt");
    float tMix     = *apvts.getRawParameterValue ("mix");
    float macro1   = *apvts.getRawParameterValue ("macro1");
    float macro2   = *apvts.getRawParameterValue ("macro2");

    // --- macro influence ---
    // macro1 (Bloom): pushes bloom level
    tBloom  += macro1 * (1.0f - tBloom);
    // macro2 (Unfurl): pushes unfurl (attack speed)
    tUnfurl += macro2 * (1.0f - tUnfurl);

    tBloom  = juce::jlimit (0.0f, 1.0f, tBloom);
    tUnfurl = juce::jlimit (0.0f, 1.0f, tUnfurl);
    tMix    = juce::jlimit (0.0f, 1.0f, tMix);

    // --- block-level parameter smoothing (~50ms) ---
    const float tc = std::exp (-6.2832f / (0.05f * (float) currentSampleRate / (float) numSamples));
    smoothBloom   += (1.0f - tc) * (tBloom   - smoothBloom);
    smoothUnfurl  += (1.0f - tc) * (tUnfurl  - smoothUnfurl);
    smoothShimmer += (1.0f - tc) * (tShimmer - smoothShimmer);
    smoothTilt    += (1.0f - tc) * (tTilt    - smoothTilt);
    smoothMix     += (1.0f - tc) * (tMix     - smoothMix);

    // update filter coefficients when shimmer changes
    if (std::abs (smoothShimmer - lastShimmer) > 0.004f ||
        std::abs (smoothTilt    - lastTilt)    > 0.004f)
    {
        updateFilterCoefficients (smoothShimmer, smoothTilt);
    }

    // --- capture dry signal ---
    juce::AudioBuffer<float> dryBuf (numChannels, numSamples);
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuf.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    // --- compute per-resonator target gains with tilt curve ---
    float targetGain[kNumResonators];
    for (int i = 0; i < kNumResonators; ++i)
    {
        float tiltCurve = std::pow (2.0f, (smoothTilt - 0.5f) * 2.0f * ((float) i - 2.5f) / 5.0f);
        targetGain[i] = smoothBloom * kHarmonicWeights[i] * tiltCurve;
        targetGain[i] = juce::jlimit (0.0f, 1.0f, targetGain[i]);
    }

    // --- bloom envelope attack time constant (unfurl: 0.05s - 8s) ---
    // unfurl=0 → fast (0.05s), unfurl=1 → slow (8s)
    float attackTime = 0.05f + smoothUnfurl * smoothUnfurl * 7.95f;
    float envTC = std::exp (-1.0f / (attackTime * (float) currentSampleRate / (float) numSamples));

    // --- process resonators and accumulate wet signal ---
    juce::AudioBuffer<float> wetBuf (numChannels, numSamples);
    wetBuf.clear();

    for (int ri = 0; ri < kNumResonators; ++ri)
    {
        // update bloom envelope (one-pole smoother)
        bloomEnvelope[ri] = envTC * bloomEnvelope[ri] + (1.0f - envTC) * targetGain[ri];

        // copy dry into a temp buffer for this resonator
        juce::AudioBuffer<float> resBuf (numChannels, numSamples);
        for (int ch = 0; ch < numChannels; ++ch)
            resBuf.copyFrom (ch, 0, dryBuf, ch, 0, numSamples);

        // process bandpass
        auto block = juce::dsp::AudioBlock<float> (resBuf);
        resonators[ri].process (juce::dsp::ProcessContextReplacing<float> (block));

        // accumulate with bloom envelope gain
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* wet = wetBuf.getWritePointer (ch);
            auto* res = resBuf.getReadPointer (ch);
            for (int s = 0; s < numSamples; ++s)
                wet[s] += res[s] * bloomEnvelope[ri];
        }
    }

    // --- mix dry + wet with tanh soft limiter ---
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* out = buffer.getWritePointer (ch);
        auto* dry = dryBuf.getReadPointer (ch);
        auto* wet = wetBuf.getReadPointer (ch);
        for (int s = 0; s < numSamples; ++s)
        {
            float mixed = dry[s] + smoothMix * wet[s];
            out[s] = std::tanh (mixed * 0.9f) * (1.0f / std::tanh (0.9f));
        }
    }

    // --- nature layer (morning birdsong) ---
    float tNature = *apvts.getRawParameterValue ("nature");
    smoothNature += (1.0f - tc) * (tNature - smoothNature);
    natureLayer.process (buffer, smoothNature);

    // --- update bloom display (RMS of envelope state) ---
    float envAvg = 0.0f;
    for (int i = 0; i < kNumResonators; ++i)
        envAvg += bloomEnvelope[i] * kHarmonicWeights[i];
    envAvg /= (float) kNumResonators;
    bloomDisplay.store (juce::jlimit (0.0f, 1.0f, envAvg), std::memory_order_relaxed);

    // --- write scope buffer ---
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

juce::AudioProcessorEditor* CorollaProcessor::createEditor()
{
    return new CorollaEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CorollaProcessor();
}
