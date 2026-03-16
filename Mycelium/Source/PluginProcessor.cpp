#include "PluginProcessor.h"
#include "PluginEditor.h"

MyceliumProcessor::MyceliumProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MyceliumState", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout MyceliumProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto pct = juce::NormalisableRange<float> (0.0f, 1.0f);

    layout.add (std::make_unique<juce::AudioParameterFloat> ("canopy",   "Canopy",     pct, 0.4f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("predelay", "Pre-Delay",  pct, 0.2f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("decay",    "Decay",      pct, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("damping",  "Damping",    pct, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("width",    "Width",      pct, 0.7f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("mix",      "Mix",        pct, 0.35f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("nature",   "Nature",     pct, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro1",   "Canopy",     pct, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro2",   "Root Depth", pct, 0.0f));

    return layout;
}

// ── resizeCombs: scale all comb delay times by canopy parameter ───────────────
void MyceliumProcessor::resizeCombs (float canopyNorm)
{
    float scale = juce::jmap (canopyNorm, 0.0f, 1.0f, 0.45f, 2.2f);
    float srScale = (float)(currentSampleRate / 44100.0);

    for (int i = 0; i < 4; ++i)
    {
        int delayL = juce::jmax (1, (int)((float)kCombTimesL[i] * scale * srScale));
        int delayR = juce::jmax (1, (int)((float)kCombTimesR[i] * scale * srScale));
        combL[i].resize (delayL);
        combR[i].resize (delayR);
    }
}

void MyceliumProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // pre-delay buffers: up to 80ms
    int maxPreDelaySamples = (int)(sampleRate * (double)kMaxPreDelayMs / 1000.0) + 1;
    preDelayBufL.assign ((size_t)maxPreDelaySamples, 0.0f);
    preDelayBufR.assign ((size_t)maxPreDelaySamples, 0.0f);
    preDelayPos = 0;

    // allpass filters (separate state per channel)
    float srScale = (float)(sampleRate / 44100.0);
    for (int i = 0; i < 4; ++i)
    {
        int apDelay = juce::jmax (1, (int)((float)kAllpassTimes[i] * srScale));
        allpassL[i].prepare (apDelay);
        allpassR[i].prepare (apDelay);
    }

    // initialise smoothed params to current values
    smoothCanopy   = *apvts.getRawParameterValue ("canopy");
    smoothPredelay = *apvts.getRawParameterValue ("predelay");
    smoothDecay    = *apvts.getRawParameterValue ("decay");
    smoothDamping  = *apvts.getRawParameterValue ("damping");
    smoothWidth    = *apvts.getRawParameterValue ("width");
    smoothMix      = *apvts.getRawParameterValue ("mix");

    // force comb resize at current canopy value
    lastCanopy = -1.0f;
    resizeCombs (smoothCanopy);
    lastCanopy = smoothCanopy;

    // prime pre-delay length
    preDelayLen = juce::jmax (1, (int)(smoothPredelay * (float)(kMaxPreDelayMs) * 0.001f * (float)sampleRate));

    natureLayer.prepare (sampleRate, samplesPerBlock);
    smoothNature = *apvts.getRawParameterValue ("nature");

    isPrepared = true;
}

void MyceliumProcessor::releaseResources()
{
    isPrepared = false;
}

void MyceliumProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (!isPrepared) return;
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0) return;

    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // ── read parameters ────────────────────────────────────────────────────
    float tCanopy   = *apvts.getRawParameterValue ("canopy");
    float tPredelay = *apvts.getRawParameterValue ("predelay");
    float tDecay    = *apvts.getRawParameterValue ("decay");
    float tDamping  = *apvts.getRawParameterValue ("damping");
    float tWidth    = *apvts.getRawParameterValue ("width");
    float tMix      = *apvts.getRawParameterValue ("mix");
    float macro1    = *apvts.getRawParameterValue ("macro1");
    float macro2    = *apvts.getRawParameterValue ("macro2");

    // ── macro influence ────────────────────────────────────────────────────
    // macro1 (Canopy): pushes canopy upward
    tCanopy += macro1 * (1.0f - tCanopy);

    // macro2 (Root Depth): pushes predelay + decay together
    tPredelay += macro2 * 0.6f * (1.0f - tPredelay);
    tDecay    += macro2 * 0.4f * (1.0f - tDecay);

    tCanopy   = juce::jlimit (0.0f, 1.0f, tCanopy);
    tPredelay = juce::jlimit (0.0f, 1.0f, tPredelay);
    tDecay    = juce::jlimit (0.0f, 1.0f, tDecay);
    tDamping  = juce::jlimit (0.0f, 1.0f, tDamping);

    // ── block-level exponential smoothing (~50ms) ──────────────────────────
    const float tc = std::exp (-6.2832f / (0.05f * (float)currentSampleRate / (float)numSamples));
    smoothCanopy   += (1.0f - tc) * (tCanopy   - smoothCanopy);
    smoothPredelay += (1.0f - tc) * (tPredelay - smoothPredelay);
    smoothDecay    += (1.0f - tc) * (tDecay    - smoothDecay);
    smoothDamping  += (1.0f - tc) * (tDamping  - smoothDamping);
    smoothWidth    += (1.0f - tc) * (tWidth    - smoothWidth);
    smoothMix      += (1.0f - tc) * (tMix      - smoothMix);

    // ── comb resize when canopy shifts significantly ───────────────────────
    if (std::abs (smoothCanopy - lastCanopy) > 0.015f)
    {
        resizeCombs (smoothCanopy);
        lastCanopy = smoothCanopy;
    }

    // ── derived values ─────────────────────────────────────────────────────
    float feedback = juce::jmap (smoothDecay, 0.0f, 1.0f, 0.30f, 0.90f);
    feedback = juce::jlimit (0.0f, 0.92f, feedback); // safety clamp

    float damping  = smoothDamping;

    int newPreDelayLen = juce::jmax (1,
        (int)(smoothPredelay * (float)kMaxPreDelayMs * 0.001f * (float)currentSampleRate));
    // clamp to allocated buffer size
    newPreDelayLen = juce::jmin (newPreDelayLen, (int)preDelayBufL.size() - 1);
    preDelayLen = newPreDelayLen;

    // soft limiter normalisation constant (pre-computed)
    static const float kLimiterNorm = 1.0f / std::tanh (0.9f);

    // ── capture dry signal ─────────────────────────────────────────────────
    juce::AudioBuffer<float> dryBuf (numChannels, numSamples);
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuf.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    auto* chL = buffer.getWritePointer (0);
    auto* chR = (numChannels > 1) ? buffer.getWritePointer (1) : buffer.getWritePointer (0);
    const auto* dryL = dryBuf.getReadPointer (0);
    const auto* dryR = (numChannels > 1) ? dryBuf.getReadPointer (1) : dryBuf.getReadPointer (0);

    // ── per-sample processing ──────────────────────────────────────────────
    for (int i = 0; i < numSamples; ++i)
    {
        float inL = dryL[i];
        float inR = dryR[i];

        // pre-delay
        int readPos = (preDelayPos - preDelayLen + (int)preDelayBufL.size())
                      % (int)preDelayBufL.size();
        float pdL = preDelayBufL[(size_t)readPos];
        float pdR = preDelayBufR[(size_t)readPos];
        preDelayBufL[(size_t)preDelayPos] = inL;
        preDelayBufR[(size_t)preDelayPos] = inR;
        preDelayPos = (preDelayPos + 1) % (int)preDelayBufL.size();

        // 4 parallel comb filters per channel
        float combSumL = 0.0f, combSumR = 0.0f;
        for (int c = 0; c < 4; ++c)
        {
            combSumL += combL[c].process (pdL, feedback, damping);
            combSumR += combR[c].process (pdR, feedback, damping);
        }
        combSumL *= 0.25f;
        combSumR *= 0.25f;

        // 4 series allpass diffusers (independent state per channel)
        float apL = combSumL, apR = combSumR;
        for (int a = 0; a < 4; ++a)
            apL = allpassL[a].process (apL);
        for (int a = 0; a < 4; ++a)
            apR = allpassR[a].process (apR);

        float wetL = apL, wetR = apR;

        // width blend: M/S encode
        float mid  = (wetL + wetR) * 0.5f;
        float side = (wetL - wetR) * 0.5f;
        float outL = mid + side * smoothWidth;
        float outR = mid - side * smoothWidth;

        // wet/dry mix
        float mixedL = dryL[i] + smoothMix * (outL - dryL[i]);
        float mixedR = dryR[i] + smoothMix * (outR - dryR[i]);

        // final soft limiter
        chL[i] = std::tanh (mixedL * 0.9f) * kLimiterNorm;
        chR[i] = std::tanh (mixedR * 0.9f) * kLimiterNorm;
    }

    // ── nature texture layer ───────────────────────────────────────────────
    float tNature = *apvts.getRawParameterValue ("nature");
    smoothNature += (1.0f - tc) * (tNature - smoothNature);
    natureLayer.process (buffer, smoothNature);

    // ── write scope buffer ─────────────────────────────────────────────────
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

juce::AudioProcessorEditor* MyceliumProcessor::createEditor()
{
    return new MyceliumEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MyceliumProcessor();
}
