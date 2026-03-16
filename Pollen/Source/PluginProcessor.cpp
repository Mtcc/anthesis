#include "PluginProcessor.h"
#include "PluginEditor.h"

PollenProcessor::PollenProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PollenState", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PollenProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto pct = juce::NormalisableRange<float> (0.0f, 1.0f);

    layout.add (std::make_unique<juce::AudioParameterFloat> ("scatter",     "Scatter",    pct, 0.30f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("grain_float", "Float",      pct, 0.20f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("grain_size",  "Grain Size", pct, 0.50f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("density",     "Density",    pct, 0.60f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("spread",      "Spread",     pct, 0.50f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("mix",         "Mix",        pct, 0.60f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro1",      "Scatter",    pct, 0.00f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro2",      "Float",      pct, 0.00f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("nature", "Nature", pct, 0.0f));

    return layout;
}

float PollenProcessor::grainEnvelope (float phase) noexcept
{
    // raised cosine (hann window) — zero at both ends, peak at 0.5
    return 0.5f * (1.0f - std::cos (phase * juce::MathConstants<float>::twoPi));
}

float PollenProcessor::readBuf (const std::vector<float>& buf, float pos) noexcept
{
    int size = (int) buf.size();
    if (size == 0) return 0.0f;

    // wrap pos into valid range
    float wrapped = std::fmod (pos, (float) size);
    if (wrapped < 0.0f) wrapped += (float) size;

    int   i0   = (int) wrapped % size;
    int   i1   = (i0 + 1) % size;
    float frac = wrapped - std::floor (wrapped);
    return buf[i0] + frac * (buf[i1] - buf[i0]);
}

void PollenProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // 4 seconds of mono circular buffer
    int bufSize = (int) (sampleRate * 4.0);
    inputBuf.assign (bufSize, 0.0f);
    bufWritePos = 0;

    // clear all grains
    for (auto& g : grains)
        g = Grain{};

    spawnTimer = 0.0f;

    // seed smoothed values to avoid startup transients
    smoothScatter    = *apvts.getRawParameterValue ("scatter");
    smoothFloat      = *apvts.getRawParameterValue ("grain_float");
    smoothGrainSize  = *apvts.getRawParameterValue ("grain_size");
    smoothDensity    = *apvts.getRawParameterValue ("density");
    smoothSpread     = *apvts.getRawParameterValue ("spread");
    smoothMix        = *apvts.getRawParameterValue ("mix");

    // clear scope
    std::fill (std::begin (scopeInput),  std::end (scopeInput),  0.0f);
    std::fill (std::begin (scopeOutput), std::end (scopeOutput), 0.0f);
    scopeWritePos.store (0, std::memory_order_relaxed);

    // clear display
    for (auto& gd : grainDisplay)
        gd = GrainDisplay{};

    natureLayer.prepare (sampleRate, samplesPerBlock);
    smoothNature = *apvts.getRawParameterValue ("nature");

    isPrepared = true;

    juce::ignoreUnused (samplesPerBlock);
}

void PollenProcessor::releaseResources()
{
    isPrepared = false;
}

void PollenProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (!isPrepared) return;
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0) return;

    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    const int bufSize     = (int) inputBuf.size();

    // --- read parameters ---
    float tScatter   = *apvts.getRawParameterValue ("scatter");
    float tFloat     = *apvts.getRawParameterValue ("grain_float");
    float tGrainSize = *apvts.getRawParameterValue ("grain_size");
    float tDensity   = *apvts.getRawParameterValue ("density");
    float tSpread    = *apvts.getRawParameterValue ("spread");
    float tMix       = *apvts.getRawParameterValue ("mix");
    float macro1     = *apvts.getRawParameterValue ("macro1");
    float macro2     = *apvts.getRawParameterValue ("macro2");

    // --- macro influence ---
    tScatter += macro1 * (1.0f - tScatter);
    tDensity += macro1 * 0.5f * (1.0f - tDensity);
    tFloat   += macro2 * (1.0f - tFloat);

    tScatter = juce::jlimit (0.0f, 1.0f, tScatter);
    tFloat   = juce::jlimit (0.0f, 1.0f, tFloat);
    tDensity = juce::jlimit (0.0f, 1.0f, tDensity);
    tMix     = juce::jlimit (0.0f, 1.0f, tMix);

    // --- block-level exponential smoothing (~50ms) ---
    const float tc = std::exp (-6.2832f / (0.05f * (float) currentSampleRate / (float) numSamples));
    smoothScatter   += (1.0f - tc) * (tScatter   - smoothScatter);
    smoothFloat     += (1.0f - tc) * (tFloat     - smoothFloat);
    smoothGrainSize += (1.0f - tc) * (tGrainSize - smoothGrainSize);
    smoothDensity   += (1.0f - tc) * (tDensity   - smoothDensity);
    smoothSpread    += (1.0f - tc) * (tSpread    - smoothSpread);
    smoothMix       += (1.0f - tc) * (tMix       - smoothMix);

    // --- convert to usable ranges ---
    const float densityHz        = juce::jmap (smoothDensity,   0.0f, 1.0f, 2.0f,   40.0f);
    const float grainSizeSec     = juce::jmap (smoothGrainSize, 0.0f, 1.0f, 0.020f, 0.400f);
    const float grainSizeSamples = grainSizeSec * (float) currentSampleRate;
    const float spawnInterval    = 1.0f / densityHz;
    const float mixAmt           = smoothMix;

    // --- capture dry signal before modification ---
    juce::AudioBuffer<float> dryBuf (numChannels, numSamples);
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuf.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    auto* outL = buffer.getWritePointer (0);
    auto* outR = (numChannels > 1) ? buffer.getWritePointer (1) : nullptr;
    const auto* dryL = dryBuf.getReadPointer (0);
    const auto* dryR = (numChannels > 1) ? dryBuf.getReadPointer (1) : nullptr;

    // --- process sample by sample ---
    for (int i = 0; i < numSamples; ++i)
    {
        // write mono input to circular buffer
        float inMono = outL[i] * 0.5f;
        if (outR != nullptr) inMono += outR[i] * 0.5f;
        inputBuf[bufWritePos] = inMono;
        bufWritePos = (bufWritePos + 1) % bufSize;

        // sum all active grains
        float wetL = 0.0f, wetR = 0.0f;
        int   numActive = 0;

        for (auto& grain : grains)
        {
            if (!grain.active) continue;

            float phase = grain.duration > 1e-6f ? grain.age / grain.duration : 1.0f;
            if (phase >= 1.0f)
            {
                grain.active = false;
                continue;
            }

            float env    = grainEnvelope (phase);
            float sample = readBuf (inputBuf, grain.readPos);

            wetL += sample * grain.gainL * env;
            wetR += sample * grain.gainR * env;

            // advance read position
            grain.readPos += grain.speed;
            if (grain.readPos >= (float) bufSize) grain.readPos -= (float) bufSize;
            if (grain.readPos < 0.0f)             grain.readPos += (float) bufSize;

            // advance age
            grain.age += 1.0f / (float) currentSampleRate;
            ++numActive;
        }

        // normalise to prevent dense grain buildup from clipping
        float norm = 1.0f / std::sqrt ((float) numActive + 1.0f);
        wetL *= norm;
        wetR *= norm;

        // advance spawn timer and spawn new grains
        spawnTimer += 1.0f / (float) currentSampleRate;
        if (spawnTimer >= spawnInterval)
        {
            spawnTimer -= spawnInterval;

            // find inactive slot
            Grain* slot = nullptr;
            for (auto& g : grains)
            {
                if (!g.active) { slot = &g; break; }
            }

            if (slot != nullptr)
            {
                float scatterSamples = smoothScatter * 2.0f * (float) currentSampleRate;
                float scatterOffset  = rng.nextFloat() * scatterSamples;

                float startPos = (float) bufWritePos
                                 - grainSizeSamples * 0.5f
                                 - scatterOffset;

                // wrap to valid range
                float fBufSize = (float) bufSize;
                while (startPos < 0.0f)      startPos += fBufSize;
                while (startPos >= fBufSize) startPos -= fBufSize;

                float pan = (rng.nextFloat() - 0.5f) * 2.0f * smoothSpread;
                pan = juce::jlimit (-1.0f, 1.0f, pan);
                // equal-power pan law
                float panAngle = (pan + 1.0f) * 0.5f * juce::MathConstants<float>::halfPi;

                // float controls pitch deviation (±0.5 semitone max)
                float pitchDev = (rng.nextFloat() - 0.5f) * smoothFloat;
                float speed    = std::pow (2.0f, pitchDev / 12.0f);

                slot->active   = true;
                slot->readPos  = startPos;
                slot->speed    = speed;
                slot->pan      = pan;
                slot->age      = 0.0f;
                slot->duration = grainSizeSec;
                slot->gainL    = std::cos (panAngle);
                slot->gainR    = std::sin (panAngle);
            }
        }

        // wet/dry mix
        float mixedL = dryL[i] + mixAmt * (wetL - dryL[i]);
        float mixedR = (dryR != nullptr) ? dryR[i] + mixAmt * (wetR - dryR[i]) : mixedL;

        // final soft limiter — guarantees no clipping
        const float limScale = 1.0f / std::tanh (0.9f);
        outL[i] = std::tanh (mixedL * 0.9f) * limScale;
        if (outR != nullptr)
            outR[i] = std::tanh (mixedR * 0.9f) * limScale;
    }

    // --- nature texture layer ---
    float tNature = *apvts.getRawParameterValue ("nature");
    smoothNature += (1.0f - tc) * (tNature - smoothNature);
    natureLayer.process (buffer, smoothNature);

    // --- update grain display data (once per block — UI safe) ---
    for (int gi = 0; gi < 16; ++gi)
    {
        auto& g  = grains[gi];
        auto& gd = grainDisplay[gi];
        gd.active = g.active;
        if (g.active)
        {
            gd.xNorm = g.readPos / (float) bufSize;
            // speed deviation mapped to y: 1.0 = centre, higher = top
            float speedDev = (g.speed - 1.0f) / (smoothFloat * 0.04167f + 1e-6f);
            gd.yNorm = juce::jlimit (0.0f, 1.0f, speedDev * 0.5f + 0.5f);
            float phase = g.duration > 1e-6f ? g.age / g.duration : 0.0f;
            gd.env = grainEnvelope (juce::jlimit (0.0f, 1.0f, phase));
        }
        else
        {
            gd.xNorm = 0.0f;
            gd.yNorm = 0.5f;
            gd.env   = 0.0f;
        }
    }

    // --- scope buffer ---
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

juce::AudioProcessorEditor* PollenProcessor::createEditor()
{
    return new PollenEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PollenProcessor();
}
