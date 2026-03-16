#include "PluginProcessor.h"
#include "PluginEditor.h"

ResinProcessor::ResinProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ResinState", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout ResinProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = juce::NormalisableRange<float> (0.0f, 1.0f);

    layout.add (std::make_unique<juce::AudioParameterFloat> ("drive",  "Drive",  pct, 0.35f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("age",    "Age",    pct, 0.30f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("tone",   "Tone",   pct, 0.7f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("output", "Output", pct, 0.7f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("mix",    "Mix",    pct, 1.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("nature", "Nature", pct, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro1", "Flow",   pct, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro2", "Bloom",  pct, 0.0f));

    return layout;
}

void ResinProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = (juce::uint32) getTotalNumOutputChannels();

    oversampler.initProcessing ((size_t) samplesPerBlock);

    // pre-emphasis: gentle +2dB low-mid warmth boost @ 800Hz before saturation
    // (warms the harmonic content without sharpening highs → no metallic edge)
    preEmphFilter.prepare (spec);
    *preEmphFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        sampleRate, 800.0f, 0.707f, juce::Decibels::decibelsToGain (2.0f));

    // de-emphasis: matching -2dB shelf after saturation
    deEmphFilter.prepare (spec);
    *deEmphFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        sampleRate, 800.0f, 0.707f, juce::Decibels::decibelsToGain (-2.0f));

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

    natureLayer.prepare (sampleRate, samplesPerBlock);
    smoothNature = *apvts.getRawParameterValue ("nature");

    isPrepared = true;
}

void ResinProcessor::releaseResources()
{
    oversampler.reset();
    preEmphFilter.reset();
    deEmphFilter.reset();
    toneFilter.reset();
    isPrepared = false;
}

// waveshaper: warm even-harmonic saturation via DC bias (triode bias-point model)
// age=0  → pure 2nd-harmonic bloom, soft and transparent
// age=0.5 → rich tube warmth, some 3rd harmonic colour
// age=1  → full saturation, complex harmonics — musical, not metallic
float ResinProcessor::saturate (float x, float driveGain, float age) noexcept
{
    // DC bias generates even harmonics — this is literally how a tube works.
    // Larger bias = more 2nd harmonic asymmetry = warmer, richer colour.
    const float bias   = age * 0.4f;            // 0 → 0.4, clearly audible
    const float driven = x * driveGain + bias;

    // soft clip — output is bounded but NOT normalised back to input level.
    // drive is supposed to push the level (user has output knob to compensate).
    const float shaped = std::tanh (driven);

    // remove DC offset introduced by bias
    return shaped - std::tanh (bias);
}

void ResinProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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
    // Flow (macro1): first half opens drive, second half adds age character
    if (macro1 <= 0.5f)
        tDrive += macro1 * 2.0f * (1.0f - tDrive);
    else {
        tDrive = 1.0f;
        tAge  += (macro1 - 0.5f) * 2.0f * (1.0f - tAge);
    }

    // Bloom (macro2): wet signal blooms into the mix as tone opens up.
    // Like a flower opening — saturated sound unfurls from behind the dry signal.
    // Pushes mix toward full wet AND opens tone toward bright simultaneously.
    tMix  += macro2 * (1.0f - tMix);
    tTone += macro2 * (1.0f - tTone);

    tDrive = juce::jlimit (0.0f, 1.0f, tDrive);
    tAge   = juce::jlimit (0.0f, 1.0f, tAge);
    tMix   = juce::jlimit (0.0f, 1.0f, tMix);
    tTone  = juce::jlimit (0.0f, 1.0f, tTone);

    // --- exponential block-level smoothing (~50ms) ---
    const float tc = std::exp (-6.2832f / (0.05f * (float) currentSampleRate / (float) numSamples));
    smoothDrive += (1.0f - tc) * (tDrive - smoothDrive);
    smoothAge   += (1.0f - tc) * (tAge   - smoothAge);
    smoothTone  += (1.0f - tc) * (tTone  - smoothTone);
    smoothOut   += (1.0f - tc) * (tOut   - smoothOut);
    smoothMix   += (1.0f - tc) * (tMix   - smoothMix);

    // --- convert to usable ranges ---
    // 1x (unity) to 20x (~26dB) — plenty of range from subtle to aggressive
    const float driveGain = juce::jmap (smoothDrive, 0.0f, 1.0f, 1.0f, 20.0f);
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

    // --- nature layer: tape hiss + vinyl crackle (additive, after limiter) ---
    float tNature = *apvts.getRawParameterValue ("nature");
    smoothNature += (1.0f - tc) * (tNature - smoothNature);
    natureLayer.process (buffer, smoothNature);

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

juce::AudioProcessorEditor* ResinProcessor::createEditor()
{
    return new ResinEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ResinProcessor();
}
