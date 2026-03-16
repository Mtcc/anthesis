#include "PluginProcessor.h"
#include "PluginEditor.h"


AureoleProcessor::AureoleProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AureoleState", createParameterLayout())
{
    lfoPhase[0] = 0.0f;
    lfoPhase[1] = juce::MathConstants<float>::pi;
    lfoPhase[2] = juce::MathConstants<float>::halfPi;
    lfoPhase[3] = juce::MathConstants<float>::pi * 1.5f;
}

juce::AudioProcessorValueTreeState::ParameterLayout AureoleProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto pct = juce::NormalisableRange<float> (0.0f, 1.0f);

    layout.add (std::make_unique<juce::AudioParameterFloat> ("rate",   "Rate",   pct, 0.3f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("depth",  "Depth",  pct, 0.4f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("spread", "Spread", pct, 0.8f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("warmth", "Warmth", pct, 0.4f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("mix",    "Mix",    pct, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro1", "Drift",  pct, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro2", "Warmth", pct, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("nature", "Nature", pct, 0.0f));

    return layout;
}

void AureoleProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    for (int v = 0; v < kNumVoices; ++v)
    {
        delayL[v].prepare (kMaxDelayBuf);
        delayR[v].prepare (kMaxDelayBuf);
    }

    // seed LFO phases — evenly spaced in [0, 2pi)
    lfoPhase[0] = 0.0f;
    lfoPhase[1] = juce::MathConstants<float>::pi;
    lfoPhase[2] = juce::MathConstants<float>::halfPi;
    lfoPhase[3] = juce::MathConstants<float>::pi * 1.5f;

    lpfState[0] = lpfState[1] = 0.0f;
    warmthCoeff      = 1.0f;
    lastWarmthSmooth = -1.0f;

    // prime smoothed params from current state to avoid startup transient
    smoothRate   = *apvts.getRawParameterValue ("rate");
    smoothDepth  = *apvts.getRawParameterValue ("depth");
    smoothSpread = *apvts.getRawParameterValue ("spread");
    smoothWarmth = *apvts.getRawParameterValue ("warmth");
    smoothMix    = *apvts.getRawParameterValue ("mix");

    natureLayer.prepare (sampleRate, samplesPerBlock);
    smoothNature = *apvts.getRawParameterValue ("nature");

    isPrepared = true;
}

void AureoleProcessor::releaseResources()
{
    isPrepared = false;
    for (int v = 0; v < kNumVoices; ++v)
    {
        delayL[v].buf.clear();
        delayR[v].buf.clear();
    }
    lpfState[0] = lpfState[1] = 0.0f;
}

void AureoleProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (!isPrepared) return;
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0) return;

    juce::ScopedNoDenormals noDenormals;

    const int   numSamples  = buffer.getNumSamples();
    const int   numChannels = juce::jmin (buffer.getNumChannels(), 2);
    const float twoPi       = juce::MathConstants<float>::twoPi;
    const float sr          = (float)currentSampleRate;

    // ── 1. read raw parameter values ──────────────────────────────────────
    float tRate   = *apvts.getRawParameterValue ("rate");
    float tDepth  = *apvts.getRawParameterValue ("depth");
    float tSpread = *apvts.getRawParameterValue ("spread");
    float tWarmth = *apvts.getRawParameterValue ("warmth");
    float tMix    = *apvts.getRawParameterValue ("mix");
    float macro1  = *apvts.getRawParameterValue ("macro1");
    float macro2  = *apvts.getRawParameterValue ("macro2");

    // ── 2. apply macro influence ──────────────────────────────────────────
    // Drift (macro1): quadratic for musicality — high values = slow wide ensemble
    float driftRate  = macro1 * macro1 * 0.7f;
    float driftDepth = macro1 * 0.8f;
    tRate  += driftRate  * (1.0f - tRate);
    tDepth += driftDepth * (1.0f - tDepth);

    // Warmth (macro2): additively pushes warmth toward maximum
    tWarmth += macro2 * (1.0f - tWarmth);

    tRate   = juce::jlimit (0.0f, 1.0f, tRate);
    tDepth  = juce::jlimit (0.0f, 1.0f, tDepth);
    tWarmth = juce::jlimit (0.0f, 1.0f, tWarmth);

    // ── 3. block-level exponential smoothing (~50ms time constant) ────────
    const float tc = std::exp (-twoPi / (0.05f * sr / (float)numSamples));
    smoothRate   += (1.0f - tc) * (tRate   - smoothRate);
    smoothDepth  += (1.0f - tc) * (tDepth  - smoothDepth);
    smoothSpread += (1.0f - tc) * (tSpread - smoothSpread);
    smoothWarmth += (1.0f - tc) * (tWarmth - smoothWarmth);
    smoothMix    += (1.0f - tc) * (tMix    - smoothMix);

    // ── 4. map to DSP ranges ──────────────────────────────────────────────
    // rate → 0.05–3.0 Hz (exponential feels more musical than linear)
    const float lfoRateHz = smoothRate * smoothRate * 2.95f + 0.05f;
    // depth → 0–15ms modulation swing
    const float depthMs   = smoothDepth * 15.0f;
    // spread: 0 = mono (all voices equal to both channels), 1 = full stereo
    const float spreadAmt = smoothSpread;

    // ── 5. update warmth LPF coefficient when warmth changes ──────────────
    if (std::abs (smoothWarmth - lastWarmthSmooth) > 0.003f)
    {
        float cutoff = juce::jmap (smoothWarmth, 0.0f, 1.0f, 4000.0f, 18000.0f);
        cutoff = juce::jlimit (20.0f, sr * 0.44f, cutoff);
        float omega  = twoPi * cutoff / sr;
        warmthCoeff  = omega / (1.0f + omega);
        lastWarmthSmooth = smoothWarmth;
    }

    // ── 6. capture dry buffer for mix and scope ───────────────────────────
    juce::AudioBuffer<float> dryBuf (numChannels, numSamples);
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuf.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    // ── 7. per-sample chorus processing ──────────────────────────────────
    // voices 0,2 are "left-biased"; voices 1,3 are "right-biased"
    // spread blends between equal (mono chorus) and separated (stereo width)
    auto* outL = (numChannels > 0) ? buffer.getWritePointer (0) : nullptr;
    auto* outR = (numChannels > 1) ? buffer.getWritePointer (1) : nullptr;
    const auto* inL = (numChannels > 0) ? dryBuf.getReadPointer (0) : nullptr;
    const auto* inR = (numChannels > 1) ? dryBuf.getReadPointer (1) : nullptr;

    if (outL) juce::FloatVectorOperations::fill (outL, 0.0f, numSamples);
    if (outR) juce::FloatVectorOperations::fill (outR, 0.0f, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const float inputL = inL ? inL[i] : 0.0f;
        const float inputR = inR ? inR[i] : inputL;

        for (int v = 0; v < kNumVoices; ++v)
        {
            delayL[v].write (inputL);
            delayR[v].write (inputR);

            float delayMs      = kBaseDelayMs[v] + std::sin (lfoPhase[v]) * depthMs;
            float delaySamples = juce::jmax (1.0f, delayMs * sr / 1000.0f);

            float voiceL = delayL[v].read (delaySamples);
            float voiceR = delayR[v].read (delaySamples);

            // voices 1 and 3 are right-biased; 0 and 2 are left-biased
            const bool isRight = (v == 1 || v == 3);
            float lGain = isRight ? (1.0f - spreadAmt) : 1.0f;
            float rGain = isRight ? 1.0f : (1.0f - spreadAmt);

            if (outL) outL[i] += voiceL * lGain * 0.25f;
            if (outR) outR[i] += voiceR * rGain * 0.25f;

            // advance LFO
            lfoPhase[v] += lfoRateHz * twoPi / sr;
            if (lfoPhase[v] > twoPi) lfoPhase[v] -= twoPi;
        }
    }

    // ── 8. warmth: saturation + one-pole LPF ──────────────────────────────
    // saturation scales gently with warmth for harmonic colour
    const float satDrive = 1.0f + smoothWarmth * 0.1f;
    const float satScale = 1.0f / std::tanh (satDrive);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            float w = std::tanh (wet[i] * satDrive) * satScale;
            lpfState[ch] += warmthCoeff * (w - lpfState[ch]);
            wet[i] = lpfState[ch];
        }
    }

    // ── 9. wet/dry mix ────────────────────────────────────────────────────
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto*       wet = buffer.getWritePointer (ch);
        const auto* dry = dryBuf.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            wet[i] = dry[i] + smoothMix * (wet[i] - dry[i]);
    }

    // ── 10. final soft limiter — guarantees no clipping under any parameter combo
    const float limScale = 1.0f / std::tanh (0.9f);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] = std::tanh (data[i] * 0.9f) * limScale;
    }

    // ── 11. nature texture layer (wind chimes + breeze) ──────────────────
    float tNature = *apvts.getRawParameterValue ("nature");
    smoothNature += (1.0f - tc) * (tNature - smoothNature);
    natureLayer.process (buffer, smoothNature);

    // ── 12. publish LFO phases for Halo display (editor reads on timer) ───
    for (int v = 0; v < kNumVoices; ++v)
        lfoPhaseReadout[v] = lfoPhase[v];

    // ── 13. write scope ring buffer ───────────────────────────────────────
    int wPos = scopeWritePos.load (std::memory_order_relaxed);
    const auto* ch0dry = dryBuf.getReadPointer (0);
    const auto* ch0wet = buffer.getReadPointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        scopeInput  [wPos] = ch0dry[i];
        scopeOutput [wPos] = ch0wet[i];
        wPos = (wPos + 1) & (kScopeSize - 1);
    }
    scopeWritePos.store (wPos, std::memory_order_relaxed);
}

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

juce::AudioProcessorEditor* AureoleProcessor::createEditor()
{
    return new AureoleEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AureoleProcessor();
}
