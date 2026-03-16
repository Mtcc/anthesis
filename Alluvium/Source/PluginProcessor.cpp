#include "PluginProcessor.h"
#include "PluginEditor.h"

AlluviumProcessor::AlluviumProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AlluviumState", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout AlluviumProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto pct = juce::NormalisableRange<float> (0.0f, 1.0f);

    layout.add (std::make_unique<juce::AudioParameterFloat> ("time",      "Time",      pct, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("feedback",  "Feedback",
                    juce::NormalisableRange<float> (0.0f, 0.95f), 0.4f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("sediment",  "Sediment",  pct, 0.3f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("current",   "Current",   pct, 0.3f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("pingpong",  "Ping-Pong", pct, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("mix",       "Mix",       pct, 0.4f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro1",    "Sediment",  pct, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro2",    "Current",   pct, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("nature",    "Nature",    pct, 0.0f));

    return layout;
}

void AlluviumProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // allocate delay buffers: 2s * sampleRate + 4 samples headroom
    int bufSize = (int)(2.0 * sampleRate) + 4;
    delayBufL.assign (bufSize, 0.0f);
    delayBufR.assign (bufSize, 0.0f);
    delayWritePosL = 0;
    delayWritePosR = 0;

    // reset LFO state
    wowPhase     = 0.0f;
    flutterPhase = 0.0f;
    flutterRandom = 0.0f;

    // reset filter state
    lpfStateL = 0.0f;
    lpfStateR = 0.0f;

    // reset drift
    driftTarget = 0.0f;
    driftActual = 0.0f;
    driftTimer  = 0.0f;

    // seed smoothed values from current parameters
    smoothTime     = *apvts.getRawParameterValue ("time");
    smoothFeedback = *apvts.getRawParameterValue ("feedback");
    smoothSediment = *apvts.getRawParameterValue ("sediment");
    smoothCurrent  = *apvts.getRawParameterValue ("current");
    smoothMix      = *apvts.getRawParameterValue ("mix");

    natureLayer.prepare (sampleRate, samplesPerBlock);
    smoothNature = *apvts.getRawParameterValue ("nature");

    isPrepared = true;
}

void AlluviumProcessor::releaseResources()
{
    delayBufL.clear();
    delayBufR.clear();
    isPrepared = false;
}

void AlluviumProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (!isPrepared) return;
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0) return;

    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    const float sr        = (float) currentSampleRate;
    const float twoPi     = juce::MathConstants<float>::twoPi;

    // read parameters
    float tTime     = *apvts.getRawParameterValue ("time");
    float tFeedback = *apvts.getRawParameterValue ("feedback");
    float tSediment = *apvts.getRawParameterValue ("sediment");
    float tCurrent  = *apvts.getRawParameterValue ("current");
    float tMix      = *apvts.getRawParameterValue ("mix");
    float macro1    = *apvts.getRawParameterValue ("macro1");
    float macro2    = *apvts.getRawParameterValue ("macro2");
    float pingpong  = *apvts.getRawParameterValue ("pingpong");

    // macro influence
    // macro1 (Sediment): pushes sediment toward 1.0
    tSediment += macro1 * (1.0f - tSediment);
    // macro2 (Current): pushes current toward 1.0
    tCurrent  += macro2 * (1.0f - tCurrent);

    tSediment = juce::jlimit (0.0f, 1.0f, tSediment);
    tCurrent  = juce::jlimit (0.0f, 1.0f, tCurrent);
    tFeedback = juce::jlimit (0.0f, 0.92f, tFeedback);  // hard safety clamp

    // block-level exponential smoothing (~50ms)
    const float tc = std::exp (-twoPi / (0.05f * sr / (float) numSamples));
    smoothTime     += (1.0f - tc) * (tTime     - smoothTime);
    smoothFeedback += (1.0f - tc) * (tFeedback - smoothFeedback);
    smoothSediment += (1.0f - tc) * (tSediment - smoothSediment);
    smoothCurrent  += (1.0f - tc) * (tCurrent  - smoothCurrent);
    smoothMix      += (1.0f - tc) * (tMix      - smoothMix);

    // feedback must never reach 1.0
    const float fb = juce::jlimit (0.0f, 0.92f, smoothFeedback);

    // lpf darkens with sediment
    const float lpfCoeff = 1.0f - smoothSediment * 0.75f;

    // saturation gain
    const float satGain = 1.0f + smoothSediment * 0.5f;
    const float satNorm = 1.0f / std::tanh (satGain);  // normalise output

    const bool isPingPong = (pingpong >= 0.5f);

    // capture dry signal for mix
    juce::AudioBuffer<float> dryBuf (numChannels, numSamples);
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuf.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    // get channel pointers (mono or stereo)
    auto* chanL = buffer.getWritePointer (0);
    auto* chanR = (numChannels > 1) ? buffer.getWritePointer (1) : nullptr;
    auto* dryL  = dryBuf.getReadPointer (0);
    auto* dryR  = (numChannels > 1) ? dryBuf.getReadPointer (1) : nullptr;

    const int delayBufSize = (int) delayBufL.size();

    for (int i = 0; i < numSamples; ++i)
    {
        // ── wow/flutter LFOs ────────────────────────────────────────────────
        float wowMod     = smoothSediment * 0.012f * std::sin (wowPhase);
        float flutterMod = smoothSediment * 0.004f * std::sin (flutterPhase);

        // ── drift ───────────────────────────────────────────────────────────
        driftTimer += 1.0f / sr;
        if (driftTimer >= 2.0f)
        {
            driftTimer  = 0.0f;
            driftTarget = (rng.nextFloat() * 2.0f - 1.0f);  // [-1, 1]
        }
        // smooth drift toward target (time constant ~0.4s)
        driftActual += (1.0f / (sr * 0.4f)) * (driftTarget - driftActual);

        float driftMod = driftActual * sr * 0.005f * smoothCurrent;  // ±5ms max

        // ── compute effective delay in samples ──────────────────────────────
        // exponential mapping: t = 10ms * pow(200, norm) → range 10ms–2000ms
        float baseSeconds = 0.01f * std::pow (200.0f, smoothTime);
        float baseSamples = baseSeconds * sr;

        float effectiveSamples = baseSamples * (1.0f + wowMod + flutterMod) + driftMod;
        effectiveSamples = juce::jlimit (1.0f, (float)(delayBufSize - 1), effectiveSamples);

        // ── fractional read from delay buffer ───────────────────────────────
        auto readDelayed = [&] (const std::vector<float>& buf, int writePos, float delaySamples) -> float
        {
            float readIdx = (float) writePos - delaySamples;
            while (readIdx < 0.0f) readIdx += (float) delayBufSize;
            int   ri0  = (int) readIdx % delayBufSize;
            int   ri1  = (ri0 + 1) % delayBufSize;
            float frac = readIdx - std::floor (readIdx);
            return buf[ri0] + frac * (buf[ri1] - buf[ri0]);
        };

        float readL = readDelayed (delayBufL, delayWritePosL, effectiveSamples);
        float readR = (chanR != nullptr)
                    ? readDelayed (delayBufR, delayWritePosR, effectiveSamples)
                    : readL;

        // ── one-pole LPF on feedback signal ─────────────────────────────────
        lpfStateL += lpfCoeff * (readL - lpfStateL);
        lpfStateR += lpfCoeff * (readR - lpfStateR);

        // ── saturation on feedback ───────────────────────────────────────────
        float satL = std::tanh (lpfStateL * satGain) * satNorm;
        float satR = std::tanh (lpfStateR * satGain) * satNorm;

        // ── ping-pong: swap feedback channels ───────────────────────────────
        float fbInL = isPingPong ? satR : satL;
        float fbInR = isPingPong ? satL : satR;

        // ── write into delay buffer ──────────────────────────────────────────
        float inL = chanL[i];
        float inR = (chanR != nullptr) ? chanR[i] : inL;

        delayBufL[delayWritePosL] = inL + fbInL * fb;
        delayBufR[delayWritePosR] = inR + fbInR * fb;

        delayWritePosL = (delayWritePosL + 1) % delayBufSize;
        delayWritePosR = (delayWritePosR + 1) % delayBufSize;

        // ── wet/dry mix + output soft limiter ───────────────────────────────
        float wetL = dryL[i] + smoothMix * (readL - dryL[i]);
        float wetR = (dryR != nullptr) ? dryR[i] + smoothMix * (readR - dryR[i]) : wetL;

        // soft limiter
        chanL[i] = std::tanh (wetL * 0.9f) * (1.0f / std::tanh (0.9f));
        if (chanR != nullptr)
            chanR[i] = std::tanh (wetR * 0.9f) * (1.0f / std::tanh (0.9f));

        // ── advance LFO phases ───────────────────────────────────────────────
        wowPhase     += twoPi * 0.4f / sr;
        if (wowPhase > twoPi) wowPhase -= twoPi;

        flutterPhase += twoPi * (8.0f + flutterRandom) / sr;
        if (flutterPhase > twoPi) flutterPhase -= twoPi;

        // random walk on flutter rate (±0.3 Hz/s, bounded ±2 Hz)
        flutterRandom += (rng.nextFloat() - 0.5f) * 0.6f / sr;
        flutterRandom = juce::jlimit (-2.0f, 2.0f, flutterRandom);
    }

    // ── nature texture layer ──────────────────────────────────────────────
    float tNature = *apvts.getRawParameterValue ("nature");
    smoothNature += (1.0f - tc) * (tNature - smoothNature);
    natureLayer.process (buffer, smoothNature);

    // ── write scope buffer ────────────────────────────────────────────────
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

void AlluviumProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void AlluviumProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* AlluviumProcessor::createEditor()
{
    return new AlluviumEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AlluviumProcessor();
}
