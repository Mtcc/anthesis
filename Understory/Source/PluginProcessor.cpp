#include "PluginProcessor.h"
#include "PluginEditor.h"

// ──────────────────────────────────────────────────────────────────────────────
// Constructor
// ──────────────────────────────────────────────────────────────────────────────
UnderstoryProcessor::UnderstoryProcessor()
    : AudioProcessor (BusesProperties()
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "UnderstoryState", createParameterLayout())
{
    rng.setSeedRandomly();
}

// ──────────────────────────────────────────────────────────────────────────────
// Parameter layout
// ──────────────────────────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout UnderstoryProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto pct = juce::NormalisableRange<float> (0.0f, 1.0f);

    layout.add (std::make_unique<juce::AudioParameterInt> (
        "type", "Type", 0, 14, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("density",  "Density",  pct, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("tone",     "Tone",     pct, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("texture",  "Texture",  pct, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("width",    "Width",    pct, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("level",    "Level",    pct, 0.6f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro1",   "Macro 1",  pct, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("macro2",   "Macro 2",  pct, 0.0f));

    return layout;
}

// ──────────────────────────────────────────────────────────────────────────────
// prepareToPlay — reset all texture state
// ──────────────────────────────────────────────────────────────────────────────
void UnderstoryProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    const float sr = (float)sampleRate;

    // reset pink noise
    b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0f;
    rb0 = rb1 = rb2 = rb3 = rb4 = rb5 = rb6 = 0.0f;

    // ── Rain ──────────────────────────────────────────────────────────────
    rainState.hpfL1 = rainState.hpfL2 = 0.0f;
    rainState.hpfR1 = rainState.hpfR2 = 0.0f;
    rainState.poissonAcc = 0.0f;
    for (int i = 0; i < RainState::kDrops; ++i)
    {
        rainState.dropEnv[i]     = 0.0f;
        rainState.dropDecay[i]   = 0.99f;
        rainState.dropBpL[i]     = 0.0f;
        rainState.dropBpR[i]     = 0.0f;
        rainState.dropPanL[i]    = 0.7f;
        rainState.dropPanR[i]    = 0.7f;
        rainState.dropBpCoeff[i] = onePoleLPCoeff (1200.0f, sr);
    }

    // ── Wind ──────────────────────────────────────────────────────────────
    windState.lpfLowL = windState.lpfLowR = 0.0f;
    windState.hpfHighL1 = windState.hpfHighL2 = 0.0f;
    windState.hpfHighR1 = windState.hpfHighR2 = 0.0f;
    windState.gustPhase  = 0.0f;
    windState.gustTarget = 0.5f;
    windState.gustActual = 0.5f;
    windState.gustTimer  = 0.0f;

    // ── Stream ────────────────────────────────────────────────────────────
    for (int i = 0; i < 3; ++i)
    {
        streamState.bpStateL[i]     = 0.0f;
        streamState.bpStateR[i]     = 0.0f;
        streamState.formantPhase[i] = 0.0f;
    }
    streamState.rumbleLpfL = streamState.rumbleLpfR = 0.0f;

    // ── Wind Chimes ───────────────────────────────────────────────────────
    for (int i = 0; i < ChimeState::kChimes; ++i)
    {
        chimeState.phase[i]     = 0.0f;
        chimeState.env[i]       = 0.0f;
        chimeState.decay[i]     = std::exp (-1.0f / (3.0f * sr));
        chimeState.panL[i]      = 0.7f;
        chimeState.panR[i]      = 0.7f;
        // stagger initial trigger times so they don't all fire at once
        chimeState.trigTimer[i] = rng.nextFloat() * sr * 8.0f;
    }

    // ── Crickets ──────────────────────────────────────────────────────────
    for (int i = 0; i < CricketState::kCrickets; ++i)
    {
        cricketState.carrierPhase[i] = 0.0f;
        cricketState.amPhase[i]      = 0.0f;
        cricketState.env[i]          = 0.0f;
        cricketState.envDecay[i]     = std::exp (-1.0f / (1.0f * sr));
        cricketState.burstTimer[i]   = rng.nextFloat() * sr * 2.0f;
        // carrier: 3400 to 5200 Hz, evenly spaced
        cricketState.carrierFreq[i]  = 3400.0f + (float)i * (1800.0f / (float)(CricketState::kCrickets - 1));
        // chirp rate: 14 to 22 Hz
        cricketState.amFreq[i]       = 14.0f + (float)i * (8.0f / (float)(CricketState::kCrickets - 1));
        cricketState.panL[i]         = 0.7f;
        cricketState.panR[i]         = 0.7f;
    }

    // ── Birds ─────────────────────────────────────────────────────────────
    for (int i = 0; i < BirdState::kBirds; ++i)
    {
        birdState.phase[i]        = 0.0f;
        birdState.freqCurrent[i]  = 1000.0f;
        birdState.freqTarget[i]   = 1000.0f;
        birdState.env[i]          = 0.0f;
        birdState.envAttack[i]    = 1.0f;   // start open so silence is clean
        birdState.attackRate[i]   = 0.0f;   // set at trigger time
        birdState.envDecay[i]     = std::exp (-1.0f / (0.15f * sr));
        birdState.trigTimer[i]    = rng.nextFloat() * sr * 4.0f;
        birdState.trillCount[i]   = 0;
        birdState.trillTimer[i]   = 0.0f;
        birdState.glideSpeed[i]   = 0.0f;
        birdState.glideUp[i]      = true;
        birdState.vibratoPhase[i] = 0.0f;
        birdState.vibratoRate[i]  = 5.0f + (float)i * 0.75f;  // 5, 5.75, 6.5, 7.25 Hz
        birdState.panL[i]         = 0.7f;
        birdState.panR[i]         = 0.7f;
        birdState.modPhase[i]     = 0.0f;
        birdState.modRatio[i]     = 1.5f;
        birdState.fmIndex[i]      = 2.0f;
        birdState.noiseBpL[i]     = 0.0f;
        birdState.noiseBpH[i]     = 0.0f;
    }

    // ── Leaves ────────────────────────────────────────────────────────────
    leavesState.hpfL1 = leavesState.hpfL2 = 0.0f;
    leavesState.hpfR1 = leavesState.hpfR2 = 0.0f;
    leavesState.gustLevel   = 0.0f;
    leavesState.gustTarget  = 0.3f;
    leavesState.gustAttack  = onePoleLPCoeff (0.5f, sr);  // ~1s attack by default
    leavesState.gustTimer   = sr * 2.0f;
    leavesState.snapTimer   = rng.nextFloat() * sr * 0.5f;
    for (int i = 0; i < LeavesState::kSnaps; ++i)
    {
        leavesState.snapEnv[i]   = 0.0f;
        leavesState.snapDecay[i] = std::exp (-1.0f / (0.012f * sr));
        leavesState.snapPanL[i]  = 0.7f;
        leavesState.snapPanR[i]  = 0.7f;
    }

    // ── Thunder ─────────────────────────────────────────────────────────
    thunderState.rumbleLpfL = thunderState.rumbleLpfR = 0.0f;
    thunderState.rainHpfL1 = thunderState.rainHpfL2 = 0.0f;
    thunderState.rainHpfR1 = thunderState.rainHpfR2 = 0.0f;
    thunderState.trigTimer = sr * (2.0f + rng.nextFloat() * 4.0f);
    for (int i = 0; i < ThunderState::kSlots; ++i)
    {
        thunderState.crackEnv[i]   = 0.0f;
        thunderState.crackDecay[i] = 0.99f;
        thunderState.rumbleEnv[i]  = 0.0f;
        thunderState.rumbleDecay[i]= 0.99f;
        thunderState.rumbleDelay[i]= 0.0f;
        thunderState.rumbleLpf[i]  = 0.0f;
        thunderState.crackHpf[i]   = 0.0f;
        thunderState.panL[i]       = 0.7f;
        thunderState.panR[i]       = 0.7f;
    }

    // ── Ocean ───────────────────────────────────────────────────────────
    oceanState.wavePhase    = 0.0f;
    oceanState.wavePeriod   = 8.0f;
    oceanState.nextPeriod   = 8.0f;
    oceanState.undertowLpfL = oceanState.undertowLpfR = 0.0f;
    oceanState.waveLpfL     = oceanState.waveLpfR = 0.0f;
    oceanState.crashHpfL1   = oceanState.crashHpfL2 = 0.0f;
    oceanState.crashHpfR1   = oceanState.crashHpfR2 = 0.0f;

    // ── Fire ────────────────────────────────────────────────────────────
    fireState.roarLpfL = fireState.roarLpfR = 0.0f;
    fireState.roarLfoPhase = 0.0f;
    fireState.hissHpfL1 = fireState.hissHpfL2 = 0.0f;
    fireState.hissHpfR1 = fireState.hissHpfR2 = 0.0f;
    fireState.crackleAcc = 0.0f;
    fireState.popAcc = 0.0f;
    for (int i = 0; i < FireState::kCrackles; ++i)
    {
        fireState.crackleEnv[i]   = 0.0f;
        fireState.crackleDecay[i] = std::exp (-1.0f / (0.005f * sr));
        fireState.cracklePanL[i]  = 0.7f;
        fireState.cracklePanR[i]  = 0.7f;
        fireState.crackleHpf[i]   = 0.0f;
    }
    for (int i = 0; i < FireState::kPops; ++i)
    {
        fireState.popEnv[i]   = 0.0f;
        fireState.popDecay[i] = std::exp (-1.0f / (0.02f * sr));
        fireState.popPhase[i] = 0.0f;
        fireState.popFreq[i]  = 400.0f;
        fireState.popPanL[i]  = 0.7f;
        fireState.popPanR[i]  = 0.7f;
    }

    // ── Frogs ───────────────────────────────────────────────────────────
    for (int i = 0; i < FrogState::kPeepers; ++i)
    {
        frogState.peeperPhase[i]    = 0.0f;
        frogState.peeperBaseFreq[i] = 2500.0f + (float)i * (1500.0f / (float)(FrogState::kPeepers - 1));
        frogState.peeperSweep[i]    = 200.0f + rng.nextFloat() * 300.0f;
        frogState.peeperEnv[i]      = 0.0f;
        frogState.peeperDecay[i]    = 0.99f;
        frogState.peeperTimer[i]    = rng.nextFloat() * sr * 2.0f;
        frogState.peeperElapsed[i]  = 0.0f;
        frogState.peeperDur[i]      = 0.0f;
        float pan = rng.nextFloat();
        frogState.peeperPanL[i]     = std::sqrt (1.0f - pan);
        frogState.peeperPanR[i]     = std::sqrt (pan);
        frogState.peeperNoiseBpL[i] = 0.0f;
        frogState.peeperNoiseBpH[i] = 0.0f;
    }
    for (int i = 0; i < FrogState::kTreeFrogs; ++i)
    {
        frogState.tfPhase[i]   = 0.0f;
        frogState.tfFreq1[i]   = 400.0f + (float)i * 150.0f;
        frogState.tfFreq2[i]   = 600.0f + (float)i * 250.0f;
        frogState.tfEnv[i]     = 0.0f;
        frogState.tfTimer[i]   = rng.nextFloat() * sr * 3.0f;
        frogState.tfElapsed[i] = 0.0f;
        frogState.tfDur1[i]    = 0.1f * sr;
        frogState.tfDur2[i]    = 0.08f * sr;
        float pan = rng.nextFloat();
        frogState.tfPanL[i]    = std::sqrt (1.0f - pan);
        frogState.tfPanR[i]    = std::sqrt (pan);
        frogState.tfFmPhase[i] = 0.0f;
    }

    // ── Cicadas ─────────────────────────────────────────────────────────
    for (int i = 0; i < CicadaState::kVoices; ++i)
    {
        cicadaState.carrierPhase[i] = 0.0f;
        cicadaState.carrierFreq[i]  = 4000.0f + (float)i * (4000.0f / (float)(CicadaState::kVoices - 1));
        cicadaState.amPhase[i]      = 0.0f;
        cicadaState.amFreq[i]       = 50.0f + (float)i * (150.0f / (float)(CicadaState::kVoices - 1));
        cicadaState.swellStage[i]   = 3;  // start silent
        cicadaState.swellEnv[i]     = 0.0f;
        // stagger initial silence durations
        cicadaState.swellTimer[i]   = rng.nextFloat() * sr * 5.0f;
        cicadaState.carrierPhase2[i] = 0.0f;
        cicadaState.carrierPhase3[i] = 0.0f;
        cicadaState.detune1[i]       = 3.0f + rng.nextFloat() * 4.0f;
        cicadaState.detune2[i]       = -(3.0f + rng.nextFloat() * 4.0f);
        cicadaState.noiseBpL[i]      = 0.0f;
        cicadaState.noiseBpH[i]      = 0.0f;
    }

    // ── Owls ────────────────────────────────────────────────────────────
    owlState.baseFreq[0] = 300.0f + rng.nextFloat() * 50.0f;
    owlState.baseFreq[1] = 380.0f + rng.nextFloat() * 70.0f;
    owlState.nightLpfL = owlState.nightLpfR = 0.0f;
    for (int i = 0; i < OwlState::kOwls; ++i)
    {
        owlState.phase[i]     = 0.0f;
        owlState.env[i]       = 0.0f;
        owlState.noteIndex[i] = -1;
        owlState.noteTimer[i] = 0.0f;
        owlState.callTimer[i] = sr * (2.0f + rng.nextFloat() * 6.0f + (float)i * 4.0f);
        owlState.noteFreq[i]  = owlState.baseFreq[i];
        float pan = (float)i * 0.6f + 0.2f;
        owlState.panL[i]      = std::sqrt (1.0f - pan);
        owlState.panR[i]      = std::sqrt (pan);
        owlState.noiseBpL[i]  = 0.0f;
        owlState.noiseBpH[i]  = 0.0f;
    }

    // ── Cave Drips ──────────────────────────────────────────────────────
    caveState.ambientLpfL = caveState.ambientLpfR = 0.0f;
    caveState.dripAcc = 0.0f;
    for (int i = 0; i < CaveState::kSlots; ++i)
    {
        caveState.dripPhase[i] = 0.0f;
        caveState.dripFreq[i]  = 1200.0f;
        caveState.dripEnv[i]   = 0.0f;
        caveState.dripDecay[i] = std::exp (-1.0f / (0.15f * sr));
        caveState.dripPanL[i]  = 0.7f;
        caveState.dripPanR[i]  = 0.7f;
        caveState.dripDelay[i]     = 0.0f;
        caveState.dripFreqStart[i] = 1200.0f;
        caveState.dripSweepRate[i] = 0.03f * sr;
        caveState.dripElapsed[i]   = 0.0f;
    }

    // ── Bees ────────────────────────────────────────────────────────────
    for (int i = 0; i < BeeState::kBees; ++i)
    {
        beeState.phase[i]     = rng.nextFloat() * juce::MathConstants<float>::twoPi;
        beeState.baseFreq[i]  = 180.0f + (float)i * (170.0f / (float)(BeeState::kBees - 1));
        beeState.lfoPhase[i]  = rng.nextFloat() * juce::MathConstants<float>::twoPi;
        beeState.lfoRate[i]   = 0.5f + rng.nextFloat() * 1.5f;
        beeState.lfoDepth[i]  = 5.0f + rng.nextFloat() * 10.0f;
        beeState.env[i]       = 0.0f;
        beeState.envTarget[i] = 0.0f;
        beeState.envSpeed[i]  = onePoleLPCoeff (1.0f, sr);
        beeState.fadeTimer[i] = rng.nextFloat() * sr * 3.0f;
    }

    // prime smoothed params
    smoothDensity = *apvts.getRawParameterValue ("density");
    smoothTone    = *apvts.getRawParameterValue ("tone");
    smoothTexture = *apvts.getRawParameterValue ("texture");
    smoothWidth   = *apvts.getRawParameterValue ("width");
    smoothLevel   = *apvts.getRawParameterValue ("level");
    smoothMacro1  = *apvts.getRawParameterValue ("macro1");
    smoothMacro2  = *apvts.getRawParameterValue ("macro2");

    isPrepared = true;
}

void UnderstoryProcessor::releaseResources()
{
    isPrepared = false;
}

// ──────────────────────────────────────────────────────────────────────────────
// processBlock — main entry point
// ──────────────────────────────────────────────────────────────────────────────
void UnderstoryProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (!isPrepared) return;

    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    const float sr    = (float)currentSampleRate;
    const float twoPi = juce::MathConstants<float>::twoPi;

    // clear the output buffer (IS_SYNTH — we own it)
    buffer.clear();

    // ── 1. read and smooth parameters ─────────────────────────────────────
    const float tc = std::exp (-twoPi / (0.05f * sr / (float)numSamples));

    float tDensity  = *apvts.getRawParameterValue ("density");
    float tTone     = *apvts.getRawParameterValue ("tone");
    float tTexture  = *apvts.getRawParameterValue ("texture");
    float tWidth    = *apvts.getRawParameterValue ("width");
    float tLevel    = *apvts.getRawParameterValue ("level");
    float tMacro1   = *apvts.getRawParameterValue ("macro1");
    float tMacro2   = *apvts.getRawParameterValue ("macro2");

    // macro1 → density; macro2 → tone
    tDensity = juce::jlimit (0.0f, 1.0f, tDensity + tMacro1 * (1.0f - tDensity));
    tTone    = juce::jlimit (0.0f, 1.0f, tTone    + tMacro2 * (1.0f - tTone));

    smoothDensity += (1.0f - tc) * (tDensity - smoothDensity);
    smoothTone    += (1.0f - tc) * (tTone    - smoothTone);
    smoothTexture += (1.0f - tc) * (tTexture - smoothTexture);
    smoothWidth   += (1.0f - tc) * (tWidth   - smoothWidth);
    smoothLevel   += (1.0f - tc) * (tLevel   - smoothLevel);
    smoothMacro1  += (1.0f - tc) * (tMacro1  - smoothMacro1);
    smoothMacro2  += (1.0f - tc) * (tMacro2  - smoothMacro2);

    // ── 2. allocate temp stereo buffers ───────────────────────────────────
    std::vector<float> tempL (static_cast<size_t> (numSamples), 0.0f);
    std::vector<float> tempR (static_cast<size_t> (numSamples), 0.0f);

    float* outL = tempL.data();
    float* outR = tempR.data();

    // ── 3. route to texture synthesizer ───────────────────────────────────
    const int type = juce::jlimit (0, 14,
        (int)(*apvts.getRawParameterValue ("type")));

    switch (type)
    {
        case 0:  synthRain     (outL, outR, numSamples, smoothDensity, smoothTone, smoothTexture); break;
        case 1:  synthWind     (outL, outR, numSamples, smoothDensity, smoothTone, smoothTexture); break;
        case 2:  synthStream   (outL, outR, numSamples, smoothDensity, smoothTone, smoothTexture); break;
        case 3:  synthChimes   (outL, outR, numSamples, smoothDensity, smoothTone, smoothTexture); break;
        case 4:  synthCrickets (outL, outR, numSamples, smoothDensity, smoothTone, smoothTexture); break;
        case 5:  synthBirds    (outL, outR, numSamples, smoothDensity, smoothTone, smoothTexture); break;
        case 6:  synthLeaves   (outL, outR, numSamples, smoothDensity, smoothTone, smoothTexture); break;
        case 7:  synthThunder  (outL, outR, numSamples, smoothDensity, smoothTone, smoothTexture); break;
        case 8:  synthOcean    (outL, outR, numSamples, smoothDensity, smoothTone, smoothTexture); break;
        case 9:  synthFire     (outL, outR, numSamples, smoothDensity, smoothTone, smoothTexture); break;
        case 10: synthFrogs    (outL, outR, numSamples, smoothDensity, smoothTone, smoothTexture); break;
        case 11: synthCicadas  (outL, outR, numSamples, smoothDensity, smoothTone, smoothTexture); break;
        case 12: synthOwls     (outL, outR, numSamples, smoothDensity, smoothTone, smoothTexture); break;
        case 13: synthCave     (outL, outR, numSamples, smoothDensity, smoothTone, smoothTexture); break;
        case 14: synthBees     (outL, outR, numSamples, smoothDensity, smoothTone, smoothTexture); break;
        default: break;
    }

    // ── 4. stereo width processing ────────────────────────────────────────
    // width=0: mono, width=1: full stereo
    for (int i = 0; i < numSamples; ++i)
    {
        float l = outL[i], r = outR[i];
        float mid  = (l + r) * 0.5f;
        float side = (l - r) * 0.5f * smoothWidth;
        outL[i] = mid + side;
        outR[i] = mid - side;
    }

    // ── 5. level + soft limiter ────────────────────────────────────────────
    const float limScale = 1.0f / std::tanh (0.9f);
    for (int i = 0; i < numSamples; ++i)
    {
        outL[i] = std::tanh (outL[i] * smoothLevel * 0.9f) * limScale;
        outR[i] = std::tanh (outR[i] * smoothLevel * 0.9f) * limScale;
    }

    // ── 6. write to JUCE buffer ────────────────────────────────────────────
    if (numChannels >= 1)
        juce::FloatVectorOperations::copy (buffer.getWritePointer (0), outL, numSamples);
    if (numChannels >= 2)
        juce::FloatVectorOperations::copy (buffer.getWritePointer (1), outR, numSamples);

    // ── 7. scope ring buffer ──────────────────────────────────────────────
    int wPos = scopeWritePos.load (std::memory_order_relaxed);
    for (int i = 0; i < numSamples; ++i)
    {
        scopeBuffer[wPos] = outL[i];
        wPos = (wPos + 1) & (kScopeSize - 1);
    }
    scopeWritePos.store (wPos, std::memory_order_relaxed);
}

// ──────────────────────────────────────────────────────────────────────────────
// Type 0: Rain
// ──────────────────────────────────────────────────────────────────────────────
void UnderstoryProcessor::synthRain (float* outL, float* outR, int n,
                                      float density, float tone, float /*texture*/)
{
    const float sr = (float)currentSampleRate;

    // HPF cutoff: 600-1200 Hz mapped from tone
    const float hpfCutoff = 600.0f + tone * 600.0f;
    const float hpfCoeff  = onePoleHPCoeff (hpfCutoff, sr);

    // drop rate: 15-45 drops/sec mapped from density
    const float dropRate  = 15.0f + density * 30.0f;
    // average samples between drops
    const float avgGap    = sr / dropRate;

    for (int i = 0; i < n; ++i)
    {
        // ── hiss: pink noise through 2-pole HPF ───────────────────────────
        float noiseL = pinkNoise();
        float noiseR = pinkNoiseR();

        // 2-stage one-pole HP (cascade for steeper slope)
        float hp1L = noiseL - rainState.hpfL1;
        rainState.hpfL1 += hpfCoeff * hp1L;
        float hp2L = hp1L - rainState.hpfL2;
        rainState.hpfL2 += hpfCoeff * hp2L;

        float hp1R = noiseR - rainState.hpfR1;
        rainState.hpfR1 += hpfCoeff * hp1R;
        float hp2R = hp1R - rainState.hpfR2;
        rainState.hpfR2 += hpfCoeff * hp2R;

        float hissL = hp2L * (0.3f + density * 0.4f);
        float hissR = hp2R * (0.3f + density * 0.4f);

        // ── Poisson drop trigger ───────────────────────────────────────────
        rainState.poissonAcc += 1.0f / avgGap;
        while (rainState.poissonAcc >= 1.0f)
        {
            rainState.poissonAcc -= 1.0f;
            // find an idle drop slot
            for (int d = 0; d < RainState::kDrops; ++d)
            {
                if (rainState.dropEnv[d] < 0.01f)
                {
                    // decay: 15-35ms
                    float decayMs = 15.0f + rng.nextFloat() * 20.0f;
                    rainState.dropDecay[d] = std::exp (-1.0f / (decayMs * 0.001f * sr));
                    rainState.dropEnv[d]   = 0.9f + rng.nextFloat() * 0.1f;
                    // bandpass centre: 1500-3000 Hz
                    float bpHz = 1500.0f + rng.nextFloat() * 1500.0f;
                    rainState.dropBpCoeff[d] = onePoleLPCoeff (bpHz, sr);
                    // random stereo pan
                    float pan = rng.nextFloat();
                    rainState.dropPanL[d] = std::sqrt (1.0f - pan);
                    rainState.dropPanR[d] = std::sqrt (pan);
                    break;
                }
            }
        }

        // ── accumulate drop bursts ─────────────────────────────────────────
        float dropSumL = 0.0f, dropSumR = 0.0f;
        for (int d = 0; d < RainState::kDrops; ++d)
        {
            if (rainState.dropEnv[d] < 0.001f) continue;

            float dropNoise = rng.nextFloat() * 2.0f - 1.0f;
            // one-pole BP approximation (LP applied to noise burst)
            rainState.dropBpL[d] += rainState.dropBpCoeff[d] * (dropNoise - rainState.dropBpL[d]);
            rainState.dropBpR[d] += rainState.dropBpCoeff[d] * (dropNoise - rainState.dropBpR[d]);

            dropSumL += rainState.dropBpL[d] * rainState.dropEnv[d] * rainState.dropPanL[d];
            dropSumR += rainState.dropBpR[d] * rainState.dropEnv[d] * rainState.dropPanR[d];

            rainState.dropEnv[d] *= rainState.dropDecay[d];
        }

        outL[i] = hissL + dropSumL * 0.4f;
        outR[i] = hissR + dropSumR * 0.4f;
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Type 1: Wind
// ──────────────────────────────────────────────────────────────────────────────
void UnderstoryProcessor::synthWind (float* outL, float* outR, int n,
                                      float density, float tone, float /*texture*/)
{
    const float sr = (float)currentSampleRate;

    // LPF cutoff for low rumble: 150-600 Hz
    const float lpfCutoff = 150.0f + tone * 450.0f;
    const float lpfCoeff  = onePoleLPCoeff (lpfCutoff, sr);

    // HPF cutoff for whistle: 2kHz fixed
    const float hpfCoeff  = onePoleHPCoeff (2000.0f, sr);

    // gust LFO rate: random in 0.1-0.5Hz range, changes every few seconds
    const float gustStep = juce::MathConstants<float>::twoPi * 0.2f / sr;

    for (int i = 0; i < n; ++i)
    {
        // ── gust LFO ──────────────────────────────────────────────────────
        windState.gustTimer -= 1.0f;
        if (windState.gustTimer <= 0.0f)
        {
            windState.gustTarget = 0.2f + rng.nextFloat() * 0.8f;
            windState.gustTimer  = sr * (3.0f + rng.nextFloat() * 5.0f);
        }
        windState.gustActual += 0.0001f * (windState.gustTarget - windState.gustActual);
        windState.gustPhase  += gustStep;
        if (windState.gustPhase > juce::MathConstants<float>::twoPi)
            windState.gustPhase -= juce::MathConstants<float>::twoPi;

        float gust = windState.gustActual * (0.6f + 0.4f * std::sin (windState.gustPhase));
        gust = gust * (0.5f + density * 0.5f);

        // ── low rumble: pink noise → LPF ──────────────────────────────────
        float pinkL = pinkNoise();
        float pinkR = pinkNoiseR();
        windState.lpfLowL += lpfCoeff * (pinkL - windState.lpfLowL);
        windState.lpfLowR += lpfCoeff * (pinkR - windState.lpfLowR);
        float rumbleL = windState.lpfLowL;
        float rumbleR = windState.lpfLowR;

        // ── high whistle: pink noise → 2-pole HPF ─────────────────────────
        float noiseWL = pinkNoise();
        float noiseWR = pinkNoiseR();
        float hw1L = noiseWL - windState.hpfHighL1;
        windState.hpfHighL1 += hpfCoeff * hw1L;
        float hw2L = hw1L - windState.hpfHighL2;
        windState.hpfHighL2 += hpfCoeff * hw2L;

        float hw1R = noiseWR - windState.hpfHighR1;
        windState.hpfHighR1 += hpfCoeff * hw1R;
        float hw2R = hw1R - windState.hpfHighR2;
        windState.hpfHighR2 += hpfCoeff * hw2R;

        // tone crossfades LP vs HP component
        float whistleL = hw2L * tone;
        float whistleR = hw2R * tone;

        outL[i] = (rumbleL * (1.0f - tone * 0.5f) + whistleL) * gust;
        outR[i] = (rumbleR * (1.0f - tone * 0.5f) + whistleR) * gust;
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Type 2: Stream / Running Water
// ──────────────────────────────────────────────────────────────────────────────
void UnderstoryProcessor::synthStream (float* outL, float* outR, int n,
                                        float density, float tone, float /*texture*/)
{
    const float sr    = (float)currentSampleRate;
    const float twoPi = juce::MathConstants<float>::twoPi;

    // base formant centres, shifted by tone (-30% to +30% range)
    const float kBaseFreqs[3] = { 280.0f, 550.0f, 1100.0f };
    // LFO rates for formant sweep
    const float kLfoRates[3] = { 0.13f, 0.21f, 0.37f };

    // rumble LPF cutoff
    const float rumbleCoeff = onePoleLPCoeff (180.0f, sr);

    for (int i = 0; i < n; ++i)
    {
        // advance formant LFOs
        for (int f = 0; f < 3; ++f)
        {
            streamState.formantPhase[f] += twoPi * kLfoRates[f] / sr;
            if (streamState.formantPhase[f] > twoPi)
                streamState.formantPhase[f] -= twoPi;
        }

        float pink    = pinkNoise();
        float pinkR_  = pinkNoiseR();

        // ── low rumble ────────────────────────────────────────────────────
        streamState.rumbleLpfL += rumbleCoeff * (pink  - streamState.rumbleLpfL);
        streamState.rumbleLpfR += rumbleCoeff * (pinkR_ - streamState.rumbleLpfR);
        float rumbleL = streamState.rumbleLpfL;
        float rumbleR = streamState.rumbleLpfR;

        // ── 3 formant bandpass filters ─────────────────────────────────────
        float formantL = 0.0f, formantR = 0.0f;
        for (int f = 0; f < 3; ++f)
        {
            // sweep ±30% around centre
            float toneMod  = 0.7f + tone * 0.6f;  // 0.7 (dark) to 1.3 (bright)
            float sweep    = 1.0f + 0.3f * std::sin (streamState.formantPhase[f]);
            float bpHz     = kBaseFreqs[f] * sweep * toneMod;
            bpHz = juce::jlimit (80.0f, sr * 0.45f, bpHz);
            float coeff    = onePoleLPCoeff (bpHz, sr);

            streamState.bpStateL[f] += coeff * (pink  - streamState.bpStateL[f]);
            streamState.bpStateR[f] += coeff * (pinkR_ - streamState.bpStateR[f]);
            formantL += streamState.bpStateL[f];
            formantR += streamState.bpStateR[f];
        }
        formantL *= (1.0f / 3.0f);
        formantR *= (1.0f / 3.0f);

        // density → formant vs rumble balance
        outL[i] = formantL * (0.4f + density * 0.6f) + rumbleL * (1.0f - density * 0.7f);
        outR[i] = formantR * (0.4f + density * 0.6f) + rumbleR * (1.0f - density * 0.7f);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Type 3: Wind Chimes
// ──────────────────────────────────────────────────────────────────────────────
void UnderstoryProcessor::synthChimes (float* outL, float* outR, int n,
                                        float density, float tone, float /*texture*/)
{
    const float sr    = (float)currentSampleRate;
    const float twoPi = juce::MathConstants<float>::twoPi;

    // density → trigger rate: average 1.5 to 14 sec interval, inverted
    // higher density = shorter interval = more chimes
    // tone → damping: lower tone = faster decay, higher = longer ring

    for (int i = 0; i < n; ++i)
    {
        float sumL = 0.0f, sumR = 0.0f;

        for (int c = 0; c < ChimeState::kChimes; ++c)
        {
            // trigger countdown
            chimeState.trigTimer[c] -= 1.0f;
            if (chimeState.trigTimer[c] <= 0.0f)
            {
                // interval: 1.5-14 sec, density compresses toward shorter
                float minInterval = 1.5f;
                float maxInterval = 14.0f;
                float interval = juce::jmap (density, 0.0f, 1.0f, maxInterval, minInterval);
                // add randomness: Poisson-like uniform in [0.3*interval, 1.7*interval]
                float jitter = interval * (0.3f + rng.nextFloat() * 1.4f);
                chimeState.trigTimer[c] = jitter * sr;

                // set hit amplitude
                chimeState.env[c] = 0.7f + rng.nextFloat() * 0.3f;

                // decay: 2-5 sec, tone controls (0=2s, 1=5s)
                float decaySec = 2.0f + tone * 3.0f;
                chimeState.decay[c] = std::exp (-1.0f / (decaySec * sr));

                // random pan
                float pan = rng.nextFloat();
                chimeState.panL[c] = std::sqrt (1.0f - pan);
                chimeState.panR[c] = std::sqrt (pan);
            }

            if (chimeState.env[c] < 0.0001f) continue;

            // advance sine oscillator
            float freq   = ChimeState::kFreqs[c];
            chimeState.phase[c] += twoPi * freq / sr;
            if (chimeState.phase[c] > twoPi)
                chimeState.phase[c] -= twoPi;

            float sample = std::sin (chimeState.phase[c]) * chimeState.env[c];
            sumL += sample * chimeState.panL[c];
            sumR += sample * chimeState.panR[c];

            chimeState.env[c] *= chimeState.decay[c];
        }

        // normalise by max voices
        outL[i] = sumL * (0.5f / (float)ChimeState::kChimes);
        outR[i] = sumR * (0.5f / (float)ChimeState::kChimes);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Type 4: Crickets / Insects
// ──────────────────────────────────────────────────────────────────────────────
void UnderstoryProcessor::synthCrickets (float* outL, float* outR, int n,
                                          float density, float tone, float /*texture*/)
{
    const float sr    = (float)currentSampleRate;
    const float twoPi = juce::MathConstants<float>::twoPi;

    // density controls: number of active crickets and burst rate
    // tone controls: carrier frequency range (0=low end, 1=high end)
    int activeCrickets = 1 + (int)(density * (float)(CricketState::kCrickets - 1) + 0.5f);
    activeCrickets = juce::jlimit (1, CricketState::kCrickets, activeCrickets);

    for (int i = 0; i < n; ++i)
    {
        float sumL = 0.0f, sumR = 0.0f;

        for (int c = 0; c < activeCrickets; ++c)
        {
            // burst trigger
            cricketState.burstTimer[c] -= 1.0f;
            if (cricketState.burstTimer[c] <= 0.0f)
            {
                // burst interval: 0.5-4s, density compresses range
                float minB = 0.5f, maxB = 4.0f;
                float interval = juce::jmap (density, 0.0f, 1.0f, maxB, minB);
                cricketState.burstTimer[c] = interval * (0.5f + rng.nextFloat()) * sr;
                cricketState.env[c] = 0.8f + rng.nextFloat() * 0.2f;
                // burst duration: 0.3-1.5s
                float burstDur = 0.3f + rng.nextFloat() * 1.2f;
                cricketState.envDecay[c] = std::exp (-1.0f / (burstDur * sr));
                // random pan
                float pan = rng.nextFloat();
                cricketState.panL[c] = std::sqrt (1.0f - pan);
                cricketState.panR[c] = std::sqrt (pan);
            }

            if (cricketState.env[c] < 0.001f) continue;

            // carrier frequency: tone shifts from low end to high end of range
            float freq = 3400.0f + tone * 1800.0f
                         + (float)c * (1800.0f / (float)(CricketState::kCrickets - 1)) * 0.3f;
            freq = juce::jlimit (3400.0f, 5200.0f, freq);

            // AM chirp
            cricketState.amPhase[c] += twoPi * cricketState.amFreq[c] / sr;
            if (cricketState.amPhase[c] > twoPi)
                cricketState.amPhase[c] -= twoPi;

            float am = 0.5f + 0.5f * std::sin (cricketState.amPhase[c]);

            // carrier
            cricketState.carrierPhase[c] += twoPi * freq / sr;
            if (cricketState.carrierPhase[c] > twoPi)
                cricketState.carrierPhase[c] -= twoPi;

            float sample = std::sin (cricketState.carrierPhase[c]) * am * cricketState.env[c];
            sumL += sample * cricketState.panL[c];
            sumR += sample * cricketState.panR[c];

            cricketState.env[c] *= cricketState.envDecay[c];
        }

        outL[i] = sumL * (0.35f / (float)CricketState::kCrickets);
        outR[i] = sumR * (0.35f / (float)CricketState::kCrickets);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Type 5: Birds / Birdsong
// Improvements: attack envelope, harmonics (1st+2nd+3rd), vibrato that grows in
// ──────────────────────────────────────────────────────────────────────────────
void UnderstoryProcessor::synthBirds (float* outL, float* outR, int n,
                                       float density, float tone, float /*texture*/)
{
    const float sr    = (float)currentSampleRate;
    const float twoPi = juce::MathConstants<float>::twoPi;
    for (int i = 0; i < n; ++i)
    {
        float sumL = 0.0f, sumR = 0.0f;

        for (int b = 0; b < BirdState::kBirds; ++b)
        {
            // ── trill sub-timer ────────────────────────────────────────────
            if (birdState.trillCount[b] > 0)
            {
                birdState.trillTimer[b] -= 1.0f;
                if (birdState.trillTimer[b] <= 0.0f)
                {
                    birdState.env[b]       = 0.65f + rng.nextFloat() * 0.35f;
                    birdState.envAttack[b] = 0.0f;
                    float attackMs         = 4.0f + rng.nextFloat() * 6.0f;
                    birdState.attackRate[b] = 1.0f / (attackMs * 0.001f * sr);

                    // trill notes pitch-step slightly for realism
                    float shift = 1.0f + (rng.nextBool() ? 1.0f : -1.0f)
                                       * (0.03f + rng.nextFloat() * 0.08f);
                    birdState.freqCurrent[b] = juce::jlimit (200.0f, 8000.0f,
                                                birdState.freqCurrent[b] * shift);
                    birdState.freqTarget[b] = birdState.freqCurrent[b];

                    float durMs = 35.0f + rng.nextFloat() * 70.0f;
                    birdState.envDecay[b]   = std::exp (-1.0f / (durMs * 0.001f * sr));
                    birdState.trillTimer[b] = durMs * 0.001f * 0.78f * sr;
                    birdState.trillCount[b]--;

                    // reset FM for trill note
                    birdState.modPhase[b] = 0.0f;
                    birdState.fmIndex[b]  = 1.5f + rng.nextFloat() * 2.5f;
                }
            }

            // ── main trigger ──────────────────────────────────────────────
            birdState.trigTimer[b] -= 1.0f;
            if (birdState.trigTimer[b] <= 0.0f && birdState.env[b] < 0.01f)
            {
                float maxInt = 6.0f, minInt = 0.4f;
                float interval = juce::jmap (density, 0.0f, 1.0f, maxInt, minInt);
                birdState.trigTimer[b] = interval * (0.4f + rng.nextFloat() * 1.2f) * sr;

                // pitch register: tone shifts from low forest birds to high
                float freqLow  = 600.0f + tone * 1800.0f;
                float freqSpan = 1000.0f + tone * 800.0f;
                float freqBase = freqLow + rng.nextFloat() * freqSpan;
                birdState.freqCurrent[b] = freqBase;

                // glide: rising more common (60%), 20-150 Hz over 60-280ms
                float glideHz = 20.0f + rng.nextFloat() * 130.0f;
                float durMs   = 60.0f + rng.nextFloat() * 220.0f;
                birdState.glideUp[b]    = (rng.nextFloat() < 0.60f);
                birdState.glideSpeed[b] = glideHz / (durMs * 0.001f * sr);
                birdState.freqTarget[b] = birdState.glideUp[b]
                    ? freqBase + glideHz : freqBase - glideHz;

                birdState.env[b]      = 0.65f + rng.nextFloat() * 0.35f;
                birdState.envDecay[b] = std::exp (-1.0f / (durMs * 0.001f * sr));

                // attack: 5-20ms linear ramp
                birdState.envAttack[b] = 0.0f;
                float attackMs = 5.0f + rng.nextFloat() * 15.0f;
                birdState.attackRate[b] = 1.0f / (attackMs * 0.001f * sr);

                // vibrato starts at zero and grows with attack
                birdState.vibratoPhase[b] = 0.0f;

                // FM params: non-integer ratio for inharmonic spectrum
                birdState.modRatio[b] = 1.2f + rng.nextFloat() * 1.3f;
                birdState.fmIndex[b]  = 1.5f + rng.nextFloat() * 2.5f;
                birdState.modPhase[b] = 0.0f;

                float pan = rng.nextFloat();
                birdState.panL[b] = std::sqrt (1.0f - pan);
                birdState.panR[b] = std::sqrt (pan);

                if (rng.nextFloat() < 0.20f)
                {
                    birdState.trillCount[b] = 2 + rng.nextInt (4);
                    birdState.trillTimer[b] = durMs * 0.001f * 0.82f * sr;
                }
                else
                {
                    birdState.trillCount[b] = 0;
                }
            }

            if (birdState.env[b] < 0.0001f) continue;

            // ── attack ramp (linear, clamped at 1) ────────────────────────
            birdState.envAttack[b] = juce::jmin (1.0f,
                                        birdState.envAttack[b] + birdState.attackRate[b]);

            // ── vibrato: depth grows with attack, max ~0.3% ───────────────
            birdState.vibratoPhase[b] += twoPi * birdState.vibratoRate[b] / sr;
            if (birdState.vibratoPhase[b] > twoPi)
                birdState.vibratoPhase[b] -= twoPi;
            float vibratoMod = 1.0f + 0.003f * birdState.envAttack[b]
                                    * std::sin (birdState.vibratoPhase[b]);

            // ── frequency glide ────────────────────────────────────────────
            float freqDelta = birdState.freqTarget[b] - birdState.freqCurrent[b];
            float step = birdState.glideSpeed[b] * (freqDelta >= 0.0f ? 1.0f : -1.0f);
            if (std::abs (freqDelta) > std::abs (step))
                birdState.freqCurrent[b] += step;
            else
                birdState.freqCurrent[b] = birdState.freqTarget[b];

            float freq = juce::jlimit (200.0f, 8000.0f,
                                       birdState.freqCurrent[b] * vibratoMod);

            // ── FM synthesis ───────────────────────────────────────────────
            birdState.phase[b] += twoPi * freq / sr;
            if (birdState.phase[b] > twoPi) birdState.phase[b] -= twoPi;

            float modFreq = freq * birdState.modRatio[b];
            birdState.modPhase[b] += twoPi * modFreq / sr;
            if (birdState.modPhase[b] > twoPi) birdState.modPhase[b] -= twoPi;

            // fm index decays with envelope: bright attack, purer tail
            float currentFmIdx = birdState.fmIndex[b] * birdState.env[b];
            float carrier = std::sin (birdState.phase[b]
                          + currentFmIdx * std::sin (birdState.modPhase[b]));

            // ── bandpass noise at chirp frequency (~20%) ───────────────────
            float noise = rng.nextFloat() * 2.0f - 1.0f;
            float hpCoeff = onePoleLPCoeff (freq * 0.5f, sr);
            float lpCoeff = onePoleLPCoeff (freq * 2.0f, sr);
            birdState.noiseBpH[b] += hpCoeff * (noise - birdState.noiseBpH[b]);
            float hpOut = noise - birdState.noiseBpH[b];
            birdState.noiseBpL[b] += lpCoeff * (hpOut - birdState.noiseBpL[b]);
            float bpNoise = birdState.noiseBpL[b];

            float amplitude = birdState.env[b] * birdState.envAttack[b];
            float sample    = (0.8f * carrier + 0.2f * bpNoise) * amplitude;

            sumL += sample * birdState.panL[b];
            sumR += sample * birdState.panR[b];

            birdState.env[b] *= birdState.envDecay[b];
        }

        outL[i] = sumL * (0.4f / (float)BirdState::kBirds);
        outR[i] = sumR * (0.4f / (float)BirdState::kBirds);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Type 6: Leaves / Rustling
// Snaps use already-HPF'd noise (not raw white noise) — removes "rain tick" quality
// ──────────────────────────────────────────────────────────────────────────────
void UnderstoryProcessor::synthLeaves (float* outL, float* outR, int n,
                                        float density, float tone, float /*texture*/)
{
    const float sr = (float)currentSampleRate;

    // HPF cutoff: 1.5-4kHz mapped from tone
    const float hpfCutoff = 1500.0f + tone * 2500.0f;
    const float hpfCoeff  = onePoleHPCoeff (hpfCutoff, sr);

    // snap rate: 0.3-1.5/sec — much lower than before, prevents ticky feel
    const float snapRate = 0.3f + density * 1.2f;

    for (int i = 0; i < n; ++i)
    {
        // ── gust envelope ─────────────────────────────────────────────────
        leavesState.gustTimer -= 1.0f;
        if (leavesState.gustTimer <= 0.0f)
        {
            leavesState.gustTarget = 0.1f + rng.nextFloat() * 0.9f;
            float attackSec = 0.25f + rng.nextFloat() * 2.2f;
            leavesState.gustAttack = onePoleLPCoeff (1.0f / attackSec, sr);
            leavesState.gustTimer = sr * (1.0f + rng.nextFloat() * 5.0f);
        }
        leavesState.gustLevel += leavesState.gustAttack
                                  * (leavesState.gustTarget - leavesState.gustLevel);

        float gustMod = leavesState.gustLevel * (0.3f + density * 0.7f);

        // ── rustle: pink noise through 2-pole HPF ─────────────────────────
        float noiseL = pinkNoise();
        float noiseR = pinkNoiseR();

        float hp1L = noiseL - leavesState.hpfL1;
        leavesState.hpfL1 += hpfCoeff * hp1L;
        float hp2L = hp1L - leavesState.hpfL2;
        leavesState.hpfL2 += hpfCoeff * hp2L;

        float hp1R = noiseR - leavesState.hpfR1;
        leavesState.hpfR1 += hpfCoeff * hp1R;
        float hp2R = hp1R - leavesState.hpfR2;
        leavesState.hpfR2 += hpfCoeff * hp2R;

        float rustleL = hp2L * gustMod;
        float rustleR = hp2R * gustMod;

        // ── leaf snaps ────────────────────────────────────────────────────
        // use filtered noise (hp2L/hp2R) not raw white noise — this is the key fix.
        // snaps now sound like a sudden swell of the same texture as the rustle,
        // not a broadband "tick" that resembles a raindrop.
        leavesState.snapTimer -= 1.0f;
        if (leavesState.snapTimer <= 0.0f)
        {
            // high variance interval — irregular feel
            leavesState.snapTimer = sr / snapRate * (0.5f + rng.nextFloat() * 1.5f);
            for (int s = 0; s < LeavesState::kSnaps; ++s)
            {
                if (leavesState.snapEnv[s] < 0.01f)
                {
                    // 30-90ms decay — longer, more papery
                    float decayMs = 30.0f + rng.nextFloat() * 60.0f;
                    leavesState.snapDecay[s] = std::exp (-1.0f / (decayMs * 0.001f * sr));
                    leavesState.snapEnv[s]   = 1.5f + rng.nextFloat() * 2.0f;
                    float pan = rng.nextFloat();
                    leavesState.snapPanL[s]  = std::sqrt (1.0f - pan);
                    leavesState.snapPanR[s]  = std::sqrt (pan);
                    break;
                }
            }
        }

        float snapSumL = 0.0f, snapSumR = 0.0f;
        for (int s = 0; s < LeavesState::kSnaps; ++s)
        {
            if (leavesState.snapEnv[s] < 0.001f) continue;
            // hp2L/hp2R: same filtered noise as the rustle — tonally identical
            snapSumL += hp2L * leavesState.snapEnv[s] * leavesState.snapPanL[s];
            snapSumR += hp2R * leavesState.snapEnv[s] * leavesState.snapPanR[s];
            leavesState.snapEnv[s] *= leavesState.snapDecay[s];
        }

        outL[i] = rustleL + snapSumL * 0.25f;
        outR[i] = rustleR + snapSumR * 0.25f;
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Type 7: Thunder / Storm
// ──────────────────────────────────────────────────────────────────────────────
void UnderstoryProcessor::synthThunder (float* outL, float* outR, int n,
                                         float density, float tone, float /*texture*/)
{
    const float sr = (float)currentSampleRate;

    // rumble LPF: 40-80Hz, tone raises cutoff (closer thunder)
    const float rumbleLpfHz = 40.0f + tone * 40.0f;
    const float rumbleLpfC  = onePoleLPCoeff (rumbleLpfHz, sr);

    // rain bed HPF: 800Hz
    const float rainHpfC = onePoleHPCoeff (800.0f, sr);

    // crack HPF: tone controls brightness (distant = more LPF = lower HPF cutoff)
    const float crackHpfHz = 1200.0f + tone * 2000.0f;
    const float crackHpfC  = onePoleHPCoeff (crackHpfHz, sr);

    // rumble event LPF: 200Hz
    const float evtRumbleLpfC = onePoleLPCoeff (200.0f, sr);

    // thunder interval: 4-20s, density inverts
    const float avgInterval = 20.0f - density * 16.0f;

    for (int i = 0; i < n; ++i)
    {
        // ── base rumble: pink noise → LPF ────────────────────────────────
        float pnL = pinkNoise();
        float pnR = pinkNoiseR();
        thunderState.rumbleLpfL += rumbleLpfC * (pnL - thunderState.rumbleLpfL);
        thunderState.rumbleLpfR += rumbleLpfC * (pnR - thunderState.rumbleLpfR);
        float baseL = thunderState.rumbleLpfL * 0.3f;
        float baseR = thunderState.rumbleLpfR * 0.3f;

        // ── rain bed: pink noise → 2-pole HPF at low level ──────────────
        float rn1L = pnL - thunderState.rainHpfL1;
        thunderState.rainHpfL1 += rainHpfC * rn1L;
        float rn2L = rn1L - thunderState.rainHpfL2;
        thunderState.rainHpfL2 += rainHpfC * rn2L;

        float rn1R = pnR - thunderState.rainHpfR1;
        thunderState.rainHpfR1 += rainHpfC * rn1R;
        float rn2R = rn1R - thunderState.rainHpfR2;
        thunderState.rainHpfR2 += rainHpfC * rn2R;

        float rainL = rn2L * 0.15f;
        float rainR = rn2R * 0.15f;

        // ── thunder event trigger ────────────────────────────────────────
        thunderState.trigTimer -= 1.0f;
        if (thunderState.trigTimer <= 0.0f)
        {
            thunderState.trigTimer = avgInterval * (0.5f + rng.nextFloat()) * sr;
            // find idle slot
            for (int s = 0; s < ThunderState::kSlots; ++s)
            {
                if (thunderState.crackEnv[s] < 0.001f && thunderState.rumbleEnv[s] < 0.001f)
                {
                    // crack: 5-20ms decay
                    float crackMs = 5.0f + rng.nextFloat() * 15.0f;
                    thunderState.crackDecay[s] = std::exp (-1.0f / (crackMs * 0.001f * sr));
                    thunderState.crackEnv[s]   = 0.8f + rng.nextFloat() * 0.2f;
                    thunderState.crackHpf[s]   = 0.0f;

                    // rumble: 1-4s decay, starts 80-120ms after crack
                    float rumbleSec = 1.0f + rng.nextFloat() * 3.0f;
                    thunderState.rumbleDecay[s] = std::exp (-1.0f / (rumbleSec * sr));
                    thunderState.rumbleEnv[s]   = 0.6f + rng.nextFloat() * 0.3f;
                    thunderState.rumbleDelay[s]  = (0.08f + rng.nextFloat() * 0.04f) * sr;
                    thunderState.rumbleLpf[s]   = 0.0f;

                    // random stereo
                    float pan = rng.nextFloat();
                    thunderState.panL[s] = std::sqrt (1.0f - pan);
                    thunderState.panR[s] = std::sqrt (pan);
                    break;
                }
            }
        }

        // ── thunder events ───────────────────────────────────────────────
        float evtL = 0.0f, evtR = 0.0f;
        for (int s = 0; s < ThunderState::kSlots; ++s)
        {
            float slotL = 0.0f;

            // crack: white noise burst → HPF
            if (thunderState.crackEnv[s] > 0.001f)
            {
                float wn = rng.nextFloat() * 2.0f - 1.0f;
                float hp = wn - thunderState.crackHpf[s];
                thunderState.crackHpf[s] += crackHpfC * hp;
                slotL += hp * thunderState.crackEnv[s] * 1.2f;
                thunderState.crackEnv[s] *= thunderState.crackDecay[s];
            }

            // rumble: pink noise → LPF, delayed start
            if (thunderState.rumbleDelay[s] > 0.0f)
            {
                thunderState.rumbleDelay[s] -= 1.0f;
            }
            else if (thunderState.rumbleEnv[s] > 0.001f)
            {
                float rNoise = rng.nextFloat() * 2.0f - 1.0f;
                thunderState.rumbleLpf[s] += evtRumbleLpfC * (rNoise - thunderState.rumbleLpf[s]);
                slotL += thunderState.rumbleLpf[s] * thunderState.rumbleEnv[s];
                thunderState.rumbleEnv[s] *= thunderState.rumbleDecay[s];
            }

            evtL += slotL * thunderState.panL[s];
            evtR += slotL * thunderState.panR[s];
        }

        // combine: tone already shapes each component's brightness
        outL[i] = baseL + rainL + evtL;
        outR[i] = baseR + rainR + evtR;
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Type 8: Ocean / Surf
// ──────────────────────────────────────────────────────────────────────────────
void UnderstoryProcessor::synthOcean (float* outL, float* outR, int n,
                                       float density, float tone, float /*texture*/)
{
    const float sr = (float)currentSampleRate;

    // undertow: constant pink noise → LPF @ 60Hz
    const float undertowC = onePoleLPCoeff (60.0f, sr);

    // wave period: 5-12s, density compresses toward shorter
    const float basePeriod = 12.0f - density * 7.0f;

    for (int i = 0; i < n; ++i)
    {
        // ── advance wave phase ───────────────────────────────────────────
        float phaseInc = 1.0f / (oceanState.wavePeriod * sr);
        oceanState.wavePhase += phaseInc;

        // wrap: start new wave
        if (oceanState.wavePhase >= 1.0f)
        {
            oceanState.wavePhase -= 1.0f;
            // randomize next period: base ± 2s
            oceanState.wavePeriod = oceanState.nextPeriod;
            oceanState.nextPeriod = juce::jlimit (3.0f, 15.0f,
                basePeriod + (rng.nextFloat() * 4.0f - 2.0f));
        }

        float ph = oceanState.wavePhase;

        // ── wave envelope and filter modulation ──────────────────────────
        float waveAmp = 0.0f;
        float lpfHz   = 200.0f;
        float crashMix = 0.0f;

        if (ph < 0.4f)
        {
            // SURGE: amplitude rises with sine, LPF opens 200→2000Hz
            float t = ph / 0.4f;
            waveAmp = std::sin (t * juce::MathConstants<float>::halfPi);
            lpfHz   = 200.0f + t * 1800.0f;
        }
        else if (ph < 0.55f)
        {
            // BREAK: noise burst, HPF content increases
            float t = (ph - 0.4f) / 0.15f;
            waveAmp = 1.0f;
            lpfHz   = 2000.0f;
            crashMix = 0.5f + tone * 0.5f;  // tone controls crash violence
            // sharp brightness spike then fade
            crashMix *= (1.0f - t * 0.5f);
        }
        else if (ph < 0.85f)
        {
            // WASH: amplitude falls, LPF closes, fizzy retreat
            float t = (ph - 0.55f) / 0.30f;
            waveAmp = 1.0f - t * 0.85f;
            lpfHz   = 2000.0f - t * 1600.0f;
            crashMix = (1.0f - t) * 0.3f * tone;
        }
        else
        {
            // LULL: very quiet undertow rumble
            float t = (ph - 0.85f) / 0.15f;
            waveAmp = 0.15f - t * 0.10f;
            lpfHz   = 400.0f;
        }

        float waveLpfC = onePoleLPCoeff (juce::jlimit (80.0f, sr * 0.45f, lpfHz), sr);
        float crashHpfC = onePoleHPCoeff (1500.0f + tone * 1500.0f, sr);

        // ── noise sources ────────────────────────────────────────────────
        float pnL = pinkNoise();
        float pnR = pinkNoiseR();

        // undertow
        oceanState.undertowLpfL += undertowC * (pnL - oceanState.undertowLpfL);
        oceanState.undertowLpfR += undertowC * (pnR - oceanState.undertowLpfR);

        // wave body: pink noise → dynamic LPF
        oceanState.waveLpfL += waveLpfC * (pnL - oceanState.waveLpfL);
        oceanState.waveLpfR += waveLpfC * (pnR - oceanState.waveLpfR);

        // crash foam: pink noise → 2-pole HPF
        float ch1L = pnL - oceanState.crashHpfL1;
        oceanState.crashHpfL1 += crashHpfC * ch1L;
        float ch2L = ch1L - oceanState.crashHpfL2;
        oceanState.crashHpfL2 += crashHpfC * ch2L;

        float ch1R = pnR - oceanState.crashHpfR1;
        oceanState.crashHpfR1 += crashHpfC * ch1R;
        float ch2R = ch1R - oceanState.crashHpfR2;
        oceanState.crashHpfR2 += crashHpfC * ch2R;

        float bodyL = oceanState.waveLpfL * waveAmp;
        float bodyR = oceanState.waveLpfR * waveAmp;
        float foamL = ch2L * crashMix * waveAmp;
        float foamR = ch2R * crashMix * waveAmp;

        outL[i] = bodyL + foamL + oceanState.undertowLpfL * 0.2f;
        outR[i] = bodyR + foamR + oceanState.undertowLpfR * 0.2f;
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Type 9: Fire / Campfire
// ──────────────────────────────────────────────────────────────────────────────
void UnderstoryProcessor::synthFire (float* outL, float* outR, int n,
                                      float density, float tone, float /*texture*/)
{
    const float sr    = (float)currentSampleRate;
    const float twoPi = juce::MathConstants<float>::twoPi;

    // roar LPF cutoff: 200-600Hz controlled by tone
    const float roarLpfHz = 200.0f + tone * 400.0f;
    const float roarLpfC  = onePoleLPCoeff (roarLpfHz, sr);

    // hiss HPF: 2-5kHz controlled by tone
    const float hissHpfHz = 2000.0f + tone * 3000.0f;
    const float hissHpfC  = onePoleHPCoeff (hissHpfHz, sr);

    // crackle HPF
    const float crackleHpfC = onePoleHPCoeff (3000.0f + tone * 2000.0f, sr);

    // crackle rate: 3-15 per sec
    const float crackleRate = 3.0f + density * 12.0f;
    // pop rate: 0.2-1.0 per sec
    const float popRate = 0.2f + density * 0.8f;

    for (int i = 0; i < n; ++i)
    {
        // ── roar LFO: slow fluctuation 0.1-0.3Hz ────────────────────────
        fireState.roarLfoPhase += twoPi * 0.18f / sr;
        if (fireState.roarLfoPhase > twoPi)
            fireState.roarLfoPhase -= twoPi;
        float gustMod = 0.6f + 0.4f * std::sin (fireState.roarLfoPhase);

        // ── base roar: pink noise → LPF ─────────────────────────────────
        float pnL = pinkNoise();
        float pnR = pinkNoiseR();
        fireState.roarLpfL += roarLpfC * (pnL - fireState.roarLpfL);
        fireState.roarLpfR += roarLpfC * (pnR - fireState.roarLpfR);
        float roarL = fireState.roarLpfL * gustMod * (0.3f + density * 0.5f);
        float roarR = fireState.roarLpfR * gustMod * (0.3f + density * 0.5f);

        // ── hiss: pink noise → 2-pole HPF, fluctuates with roar ─────────
        float hh1L = pnL - fireState.hissHpfL1;
        fireState.hissHpfL1 += hissHpfC * hh1L;
        float hh2L = hh1L - fireState.hissHpfL2;
        fireState.hissHpfL2 += hissHpfC * hh2L;

        float hh1R = pnR - fireState.hissHpfR1;
        fireState.hissHpfR1 += hissHpfC * hh1R;
        float hh2R = hh1R - fireState.hissHpfR2;
        fireState.hissHpfR2 += hissHpfC * hh2R;

        float hissL = hh2L * 0.12f * gustMod;
        float hissR = hh2R * 0.12f * gustMod;

        // ── crackle trigger (Poisson) ────────────────────────────────────
        fireState.crackleAcc += crackleRate / sr;
        while (fireState.crackleAcc >= 1.0f)
        {
            fireState.crackleAcc -= 1.0f;
            for (int c = 0; c < FireState::kCrackles; ++c)
            {
                if (fireState.crackleEnv[c] < 0.001f)
                {
                    float decayMs = 2.0f + rng.nextFloat() * 6.0f;
                    fireState.crackleDecay[c] = std::exp (-1.0f / (decayMs * 0.001f * sr));
                    fireState.crackleEnv[c]   = 0.3f + rng.nextFloat() * 0.4f;
                    fireState.crackleHpf[c]   = 0.0f;
                    float pan = rng.nextFloat();
                    fireState.cracklePanL[c] = std::sqrt (1.0f - pan);
                    fireState.cracklePanR[c] = std::sqrt (pan);
                    break;
                }
            }
        }

        // ── pop trigger (Poisson) ────────────────────────────────────────
        fireState.popAcc += popRate / sr;
        while (fireState.popAcc >= 1.0f)
        {
            fireState.popAcc -= 1.0f;
            for (int p = 0; p < FireState::kPops; ++p)
            {
                if (fireState.popEnv[p] < 0.001f)
                {
                    float decayMs = 10.0f + rng.nextFloat() * 20.0f;
                    fireState.popDecay[p] = std::exp (-1.0f / (decayMs * 0.001f * sr));
                    fireState.popEnv[p]   = 0.5f + rng.nextFloat() * 0.5f;
                    fireState.popPhase[p] = 0.0f;
                    fireState.popFreq[p]  = 200.0f + rng.nextFloat() * 600.0f;
                    float pan = rng.nextFloat();
                    fireState.popPanL[p] = std::sqrt (1.0f - pan);
                    fireState.popPanR[p] = std::sqrt (pan);
                    break;
                }
            }
        }

        // ── accumulate crackles ──────────────────────────────────────────
        float crackSumL = 0.0f, crackSumR = 0.0f;
        for (int c = 0; c < FireState::kCrackles; ++c)
        {
            if (fireState.crackleEnv[c] < 0.001f) continue;
            float wn = rng.nextFloat() * 2.0f - 1.0f;
            float hp = wn - fireState.crackleHpf[c];
            fireState.crackleHpf[c] += crackleHpfC * hp;
            crackSumL += hp * fireState.crackleEnv[c] * fireState.cracklePanL[c];
            crackSumR += hp * fireState.crackleEnv[c] * fireState.cracklePanR[c];
            fireState.crackleEnv[c] *= fireState.crackleDecay[c];
        }

        // ── accumulate pops ──────────────────────────────────────────────
        float popSumL = 0.0f, popSumR = 0.0f;
        for (int p = 0; p < FireState::kPops; ++p)
        {
            if (fireState.popEnv[p] < 0.001f) continue;
            fireState.popPhase[p] += twoPi * fireState.popFreq[p] / sr;
            if (fireState.popPhase[p] > twoPi) fireState.popPhase[p] -= twoPi;
            float s = std::sin (fireState.popPhase[p]) * fireState.popEnv[p];
            popSumL += s * fireState.popPanL[p];
            popSumR += s * fireState.popPanR[p];
            fireState.popEnv[p] *= fireState.popDecay[p];
        }

        outL[i] = roarL + hissL + crackSumL * 0.25f + popSumL * 0.35f;
        outR[i] = roarR + hissR + crackSumR * 0.25f + popSumR * 0.35f;
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Type 10: Frogs
// ──────────────────────────────────────────────────────────────────────────────
void UnderstoryProcessor::synthFrogs (float* outL, float* outR, int n,
                                       float density, float tone, float /*texture*/)
{
    const float sr    = (float)currentSampleRate;
    const float twoPi = juce::MathConstants<float>::twoPi;

    // density → call rate multiplier
    // tone → pitch register (0 = tree frogs dominant, 1 = peepers dominant)
    float peeperRate = 2.0f + density * 6.0f;   // 2-8 calls/sec across all peepers
    float treeFrogRate = 0.3f + density * 0.7f;  // 0.3-1.0 calls/sec

    // tone blends amplitude: low tone favors tree frogs, high tone favors peepers
    float peeperGain   = 0.3f + tone * 0.7f;
    float treeFrogGain = 1.0f - tone * 0.6f;

    for (int i = 0; i < n; ++i)
    {
        float sumL = 0.0f, sumR = 0.0f;

        // ── spring peepers ───────────────────────────────────────────────
        for (int p = 0; p < FrogState::kPeepers; ++p)
        {
            frogState.peeperTimer[p] -= 1.0f;
            if (frogState.peeperTimer[p] <= 0.0f && frogState.peeperEnv[p] < 0.01f)
            {
                // per-peeper rate is total rate / number of peepers, with jitter
                float avgGap = sr / (peeperRate / (float)FrogState::kPeepers);
                frogState.peeperTimer[p] = avgGap * (0.3f + rng.nextFloat() * 1.4f);

                // call duration: 80-120ms
                float durMs = 80.0f + rng.nextFloat() * 40.0f;
                frogState.peeperDur[p]     = durMs * 0.001f * sr;
                frogState.peeperElapsed[p] = 0.0f;
                frogState.peeperEnv[p]     = 0.6f + rng.nextFloat() * 0.4f;
                frogState.peeperDecay[p]   = std::exp (-1.0f / (durMs * 0.001f * sr));

                // frequency: base shifted by tone
                float toneShift = tone * 800.0f;
                frogState.peeperBaseFreq[p] = 2500.0f + toneShift
                    + (float)p * (1500.0f / (float)(FrogState::kPeepers - 1));
                frogState.peeperSweep[p] = 200.0f + rng.nextFloat() * 300.0f;
            }

            if (frogState.peeperEnv[p] < 0.001f) continue;

            // kill note when elapsed exceeds duration
            if (frogState.peeperElapsed[p] >= frogState.peeperDur[p])
            {
                frogState.peeperEnv[p] = 0.0f;
                continue;
            }

            // sharp envelope: fast attack (5%), sustain, quick release (30%)
            float envT = frogState.peeperElapsed[p] / juce::jmax (1.0f, frogState.peeperDur[p]);
            float envShape = 1.0f;
            if (envT < 0.05f) envShape = envT / 0.05f;
            else if (envT > 0.7f) envShape = juce::jmax (0.0f, (1.0f - envT) / 0.3f);

            // rising frequency sweep
            float freq = frogState.peeperBaseFreq[p] + frogState.peeperSweep[p] * envT;
            freq = juce::jlimit (1000.0f, sr * 0.45f, freq);

            frogState.peeperPhase[p] += twoPi * freq / sr;
            if (frogState.peeperPhase[p] > twoPi)
                frogState.peeperPhase[p] -= twoPi;

            // bandpass noise for breath texture (~15%)
            float pn = rng.nextFloat() * 2.0f - 1.0f;
            float bpHC = onePoleLPCoeff (freq * 0.5f, sr);
            float bpLC = onePoleLPCoeff (freq * 2.0f, sr);
            frogState.peeperNoiseBpH[p] += bpHC * (pn - frogState.peeperNoiseBpH[p]);
            float hpN = pn - frogState.peeperNoiseBpH[p];
            frogState.peeperNoiseBpL[p] += bpLC * (hpN - frogState.peeperNoiseBpL[p]);
            float chirpNoise = frogState.peeperNoiseBpL[p];

            float s = (0.85f * std::sin (frogState.peeperPhase[p]) + 0.15f * chirpNoise)
                    * frogState.peeperEnv[p] * envShape * peeperGain;
            sumL += s * frogState.peeperPanL[p];
            sumR += s * frogState.peeperPanR[p];

            frogState.peeperElapsed[p] += 1.0f;
        }

        // ── tree frogs ───────────────────────────────────────────────────
        for (int t = 0; t < FrogState::kTreeFrogs; ++t)
        {
            frogState.tfTimer[t] -= 1.0f;
            if (frogState.tfTimer[t] <= 0.0f && frogState.tfEnv[t] < 0.01f)
            {
                float avgGap = sr / (treeFrogRate / (float)FrogState::kTreeFrogs);
                frogState.tfTimer[t] = avgGap * (0.5f + rng.nextFloat() * 1.0f);

                frogState.tfEnv[t]     = 0.7f + rng.nextFloat() * 0.3f;
                frogState.tfElapsed[t] = 0.0f;

                // two-tone ribbit frequencies, shifted by tone (inverted: low tone = lower)
                float toneOffset = (1.0f - tone) * 200.0f;
                frogState.tfFreq1[t] = 400.0f + rng.nextFloat() * 300.0f - toneOffset;
                frogState.tfFreq2[t] = 600.0f + rng.nextFloat() * 500.0f - toneOffset;
                frogState.tfFreq1[t] = juce::jlimit (200.0f, 1200.0f, frogState.tfFreq1[t]);
                frogState.tfFreq2[t] = juce::jlimit (300.0f, 1500.0f, frogState.tfFreq2[t]);

                frogState.tfDur1[t] = 0.1f * sr;
                frogState.tfDur2[t] = 0.08f * sr;
                frogState.tfFmPhase[t] = 0.0f;
            }

            if (frogState.tfEnv[t] < 0.001f) continue;

            float totalDur = frogState.tfDur1[t] + frogState.tfDur2[t];
            float elapsed  = frogState.tfElapsed[t];

            float freq;
            if (elapsed < frogState.tfDur1[t])
                freq = frogState.tfFreq1[t];
            else if (elapsed < totalDur)
                freq = frogState.tfFreq2[t];
            else
            {
                frogState.tfEnv[t] = 0.0f;
                continue;
            }

            // FM wobble for organic quality
            frogState.tfFmPhase[t] += twoPi * 12.0f / sr;
            if (frogState.tfFmPhase[t] > twoPi) frogState.tfFmPhase[t] -= twoPi;
            freq += 50.0f * std::sin (frogState.tfFmPhase[t]);

            frogState.tfPhase[t] += twoPi * freq / sr;
            if (frogState.tfPhase[t] > twoPi) frogState.tfPhase[t] -= twoPi;

            // envelope shape: quick attack, sustain, quick release at end of each note
            float noteT;
            if (elapsed < frogState.tfDur1[t])
                noteT = elapsed / frogState.tfDur1[t];
            else
                noteT = (elapsed - frogState.tfDur1[t]) / frogState.tfDur2[t];
            // attack first 10%, release last 20%
            float envShape = 1.0f;
            if (noteT < 0.1f) envShape = noteT / 0.1f;
            else if (noteT > 0.8f) envShape = (1.0f - noteT) / 0.2f;

            float s = std::sin (frogState.tfPhase[t]) * frogState.tfEnv[t] * envShape * treeFrogGain;
            sumL += s * frogState.tfPanL[t];
            sumR += s * frogState.tfPanR[t];

            frogState.tfElapsed[t] += 1.0f;
        }

        outL[i] = sumL * 0.3f;
        outR[i] = sumR * 0.3f;
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Type 11: Cicadas
// ──────────────────────────────────────────────────────────────────────────────
void UnderstoryProcessor::synthCicadas (float* outL, float* outR, int n,
                                         float density, float tone, float /*texture*/)
{
    const float sr    = (float)currentSampleRate;
    const float twoPi = juce::MathConstants<float>::twoPi;

    // density → active voices (1-6) and overlap
    int activeVoices = 1 + (int)(density * (float)(CicadaState::kVoices - 1) + 0.5f);
    activeVoices = juce::jlimit (1, CicadaState::kVoices, activeVoices);

    // tone → carrier frequency range shift
    float freqBase = 3000.0f + tone * 3000.0f;  // 3-6kHz low end
    float freqSpan = 2000.0f + tone * 2000.0f;  // 2-4kHz range

    for (int i = 0; i < n; ++i)
    {
        float sumL = 0.0f, sumR = 0.0f;

        for (int v = 0; v < activeVoices; ++v)
        {
            // ── swell envelope state machine ─────────────────────────────
            cicadaState.swellTimer[v] -= 1.0f;
            if (cicadaState.swellTimer[v] <= 0.0f)
            {
                cicadaState.swellStage[v] = (cicadaState.swellStage[v] + 1) % 4;
                switch (cicadaState.swellStage[v])
                {
                    case 0: // rising: 2-5s
                        cicadaState.swellTimer[v] = (2.0f + rng.nextFloat() * 3.0f) * sr;
                        break;
                    case 1: // holding: 5-15s
                        cicadaState.swellTimer[v] = (5.0f + rng.nextFloat() * 10.0f) * sr;
                        break;
                    case 2: // falling: 2-3s
                        cicadaState.swellTimer[v] = (2.0f + rng.nextFloat() * 1.0f) * sr;
                        break;
                    case 3: // silence: 1-5s
                        cicadaState.swellTimer[v] = (1.0f + rng.nextFloat() * 4.0f) * sr;
                        cicadaState.swellEnv[v] = 0.0f;
                        break;
                }
            }

            // envelope shape per stage
            float targetEnv = 0.0f;
            float envRate = 0.0f;
            switch (cicadaState.swellStage[v])
            {
                case 0: // rising
                    targetEnv = 1.0f;
                    envRate = 1.0f / (3.0f * sr);
                    break;
                case 1: // holding
                    targetEnv = 1.0f;
                    envRate = 1.0f / (0.5f * sr);
                    break;
                case 2: // falling
                    targetEnv = 0.0f;
                    envRate = 1.0f / (2.5f * sr);
                    break;
                case 3: // silence
                    targetEnv = 0.0f;
                    envRate = 1.0f / (0.1f * sr);
                    break;
            }
            cicadaState.swellEnv[v] += envRate * (targetEnv - cicadaState.swellEnv[v]);
            cicadaState.swellEnv[v] = juce::jlimit (0.0f, 1.0f, cicadaState.swellEnv[v]);

            if (cicadaState.swellEnv[v] < 0.001f) continue;

            // carrier frequency: evenly spaced across range
            float carrierHz = freqBase + (float)v * (freqSpan / (float)(CicadaState::kVoices - 1));
            cicadaState.carrierFreq[v] = carrierHz;

            // AM modulator: 50-200Hz creates buzzy quality
            float amHz = cicadaState.amFreq[v];
            cicadaState.amPhase[v] += twoPi * amHz / sr;
            if (cicadaState.amPhase[v] > twoPi) cicadaState.amPhase[v] -= twoPi;
            float am = 0.5f + 0.5f * std::sin (cicadaState.amPhase[v]);

            // 3 detuned carriers for dense buzzing
            float freq2 = carrierHz + cicadaState.detune1[v];
            float freq3 = carrierHz + cicadaState.detune2[v];

            cicadaState.carrierPhase[v] += twoPi * carrierHz / sr;
            if (cicadaState.carrierPhase[v] > twoPi) cicadaState.carrierPhase[v] -= twoPi;
            cicadaState.carrierPhase2[v] += twoPi * freq2 / sr;
            if (cicadaState.carrierPhase2[v] > twoPi) cicadaState.carrierPhase2[v] -= twoPi;
            cicadaState.carrierPhase3[v] += twoPi * freq3 / sr;
            if (cicadaState.carrierPhase3[v] > twoPi) cicadaState.carrierPhase3[v] -= twoPi;

            float carrier = (std::sin (cicadaState.carrierPhase[v])
                           + std::sin (cicadaState.carrierPhase2[v])
                           + std::sin (cicadaState.carrierPhase3[v])) * (1.0f / 3.0f);

            // bandpass noise for texture (~10%)
            float noise = rng.nextFloat() * 2.0f - 1.0f;
            float bpHCoeff = onePoleLPCoeff (carrierHz * 0.5f, sr);
            float bpLCoeff = onePoleLPCoeff (carrierHz * 2.0f, sr);
            cicadaState.noiseBpH[v] += bpHCoeff * (noise - cicadaState.noiseBpH[v]);
            float hpOut = noise - cicadaState.noiseBpH[v];
            cicadaState.noiseBpL[v] += bpLCoeff * (hpOut - cicadaState.noiseBpL[v]);
            float bpNoise = cicadaState.noiseBpL[v];

            float sample = (0.9f * carrier + 0.1f * bpNoise) * am * cicadaState.swellEnv[v];

            // stereo: spread voices across field
            float pan = (float)v / (float)(CicadaState::kVoices - 1);
            sumL += sample * std::sqrt (1.0f - pan);
            sumR += sample * std::sqrt (pan);
        }

        outL[i] = sumL * (0.3f / (float)CicadaState::kVoices);
        outR[i] = sumR * (0.3f / (float)CicadaState::kVoices);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Type 12: Owls / Night
// ──────────────────────────────────────────────────────────────────────────────
void UnderstoryProcessor::synthOwls (float* outL, float* outR, int n,
                                      float density, float tone, float /*texture*/)
{
    const float sr    = (float)currentSampleRate;
    const float twoPi = juce::MathConstants<float>::twoPi;

    // night air LPF
    const float nightLpfC = onePoleLPCoeff (200.0f, sr);

    // call patterns
    static constexpr float kNoteDur[5]   = { 0.25f, 0.20f, 0.30f, 0.20f, 0.35f };
    static constexpr float kNoteGap[5]   = { 0.15f, 0.10f, 0.08f, 0.10f, 0.0f };
    static constexpr float kFreqMult[5]  = { 1.0f, 1.0f, 1.15f, 1.05f, 0.95f };
    static constexpr float kAmpMult[5]   = { 0.6f, 0.6f, 1.0f, 0.7f, 0.5f };

    // call interval: 8-15s, density compresses
    const float callInterval = 15.0f - density * 7.0f;

    for (int i = 0; i < n; ++i)
    {
        // ── night air ────────────────────────────────────────────────────
        float pnL = pinkNoise();
        float pnR = pinkNoiseR();
        owlState.nightLpfL += nightLpfC * (pnL - owlState.nightLpfL);
        owlState.nightLpfR += nightLpfC * (pnR - owlState.nightLpfR);
        float airL = owlState.nightLpfL * 0.06f;
        float airR = owlState.nightLpfR * 0.06f;

        float sumL = 0.0f, sumR = 0.0f;

        for (int o = 0; o < OwlState::kOwls; ++o)
        {
            // tone shifts pitch: low tone = deeper, high tone = higher
            float pitchMult = 0.8f + tone * 0.4f;

            if (owlState.noteIndex[o] < 0)
            {
                // waiting for next call
                owlState.callTimer[o] -= 1.0f;
                if (owlState.callTimer[o] <= 0.0f)
                {
                    owlState.noteIndex[o] = 0;
                    owlState.noteTimer[o] = kNoteDur[0] * sr;
                    owlState.noteFreq[o]  = owlState.baseFreq[o] * kFreqMult[0] * pitchMult;
                    owlState.env[o]       = kAmpMult[0];
                    owlState.phase[o]     = 0.0f;
                }
            }
            else
            {
                // in a call sequence
                owlState.noteTimer[o] -= 1.0f;
                if (owlState.noteTimer[o] <= 0.0f)
                {
                    int ni = owlState.noteIndex[o];
                    // gap after current note
                    if (kNoteGap[ni] > 0.0f && owlState.env[o] > 0.001f)
                    {
                        // enter gap
                        owlState.env[o] = 0.0f;
                        owlState.noteTimer[o] = kNoteGap[ni] * sr;
                    }
                    else
                    {
                        // advance to next note
                        ni++;
                        if (ni >= 5)
                        {
                            // sequence done
                            owlState.noteIndex[o] = -1;
                            owlState.env[o] = 0.0f;
                            owlState.callTimer[o] = callInterval * (0.6f + rng.nextFloat() * 0.8f) * sr;
                            continue;
                        }
                        owlState.noteIndex[o] = ni;
                        owlState.noteTimer[o] = kNoteDur[ni] * sr;
                        owlState.noteFreq[o]  = owlState.baseFreq[o] * kFreqMult[ni] * pitchMult;
                        owlState.env[o]       = kAmpMult[ni];
                        owlState.phase[o]     = 0.0f;
                    }
                }
            }

            if (owlState.env[o] < 0.001f) continue;

            // 30ms attack for smoother onset
            float attackSamples = 0.03f * sr;
            float noteElapsed = kNoteDur[juce::jlimit (0, 4, owlState.noteIndex[o])] * sr
                                - owlState.noteTimer[o];
            float attackEnv = juce::jmin (1.0f, noteElapsed / attackSamples);

            // slight frequency droop during note (-2%)
            float droopT = 1.0f;
            if (owlState.noteTimer[o] > 0.0f)
            {
                float totalNoteSamples = kNoteDur[juce::jlimit (0, 4, owlState.noteIndex[o])] * sr;
                if (totalNoteSamples > 0.0f)
                    droopT = 1.0f - 0.02f * (1.0f - owlState.noteTimer[o] / totalNoteSamples);
            }

            float freq = owlState.noteFreq[o] * droopT;
            owlState.phase[o] += twoPi * freq / sr;
            if (owlState.phase[o] > twoPi) owlState.phase[o] -= twoPi;

            // formant-filtered noise for breathy quality
            float owlNoise = rng.nextFloat() * 2.0f - 1.0f;
            float bpHCoeff = onePoleLPCoeff (freq * 0.7f, sr);
            float bpLCoeff = onePoleLPCoeff (freq * 1.5f, sr);
            owlState.noiseBpH[o] += bpHCoeff * (owlNoise - owlState.noiseBpH[o]);
            float hpOut = owlNoise - owlState.noiseBpH[o];
            owlState.noiseBpL[o] += bpLCoeff * (hpOut - owlState.noiseBpL[o]);
            float formant = owlState.noiseBpL[o];

            // slight amplitude flutter during held note
            float flutter = 1.0f + 0.04f * std::sin (noteElapsed * twoPi * 2.5f / sr);

            float ph = owlState.phase[o];
            float sample = (0.6f * std::sin (ph) + 0.4f * formant) * flutter;
            sample *= owlState.env[o] * attackEnv;

            sumL += sample * owlState.panL[o];
            sumR += sample * owlState.panR[o];
        }

        outL[i] = airL + sumL * 0.5f;
        outR[i] = airR + sumR * 0.5f;
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Type 13: Cave Drips
// ──────────────────────────────────────────────────────────────────────────────
void UnderstoryProcessor::synthCave (float* outL, float* outR, int n,
                                      float density, float tone, float /*texture*/)
{
    const float sr    = (float)currentSampleRate;
    const float twoPi = juce::MathConstants<float>::twoPi;

    // cave ambient: very quiet pink noise → LPF @ 300Hz
    const float ambientLpfC = onePoleLPCoeff (300.0f, sr);

    // drip rate: 0.5-3 drips/sec
    const float dripRate = 0.5f + density * 2.5f;

    // drip pitch range: tone controls (darker = 800-1200, brighter = 1200-2000)
    const float dripFreqLow  = 800.0f + tone * 400.0f;
    const float dripFreqHigh = 1200.0f + tone * 800.0f;

    // echo amplitude scale from tone
    const float echoScale = 0.6f + tone * 0.4f;

    for (int i = 0; i < n; ++i)
    {
        // ── cave ambient ─────────────────────────────────────────────────
        float pnL = pinkNoise();
        float pnR = pinkNoiseR();
        caveState.ambientLpfL += ambientLpfC * (pnL - caveState.ambientLpfL);
        caveState.ambientLpfR += ambientLpfC * (pnR - caveState.ambientLpfR);
        float ambL = caveState.ambientLpfL * 0.05f;
        float ambR = caveState.ambientLpfR * 0.05f;

        // ── drip trigger (Poisson) ───────────────────────────────────────
        caveState.dripAcc += dripRate / sr;
        while (caveState.dripAcc >= 1.0f)
        {
            caveState.dripAcc -= 1.0f;

            // find an idle slot for primary drip
            int primarySlot = -1;
            for (int d = 0; d < CaveState::kSlots; ++d)
            {
                if (caveState.dripEnv[d] < 0.001f && caveState.dripDelay[d] <= 0.0f)
                {
                    primarySlot = d;
                    break;
                }
            }

            if (primarySlot >= 0)
            {
                float freq = dripFreqLow + rng.nextFloat() * (dripFreqHigh - dripFreqLow);
                float decayMs = 100.0f + rng.nextFloat() * 200.0f;
                caveState.dripFreq[primarySlot]  = freq;
                caveState.dripDecay[primarySlot] = std::exp (-1.0f / (decayMs * 0.001f * sr));
                caveState.dripEnv[primarySlot]   = 0.7f + rng.nextFloat() * 0.3f;
                caveState.dripPhase[primarySlot] = 0.0f;
                caveState.dripDelay[primarySlot] = 0.0f;
                caveState.dripFreqStart[primarySlot] = freq * 1.5f;
                float sweepMs = 20.0f + rng.nextFloat() * 30.0f;
                caveState.dripSweepRate[primarySlot] = sweepMs * 0.001f * sr;
                caveState.dripElapsed[primarySlot] = 0.0f;
                float pan = rng.nextFloat();
                caveState.dripPanL[primarySlot] = std::sqrt (1.0f - pan);
                caveState.dripPanR[primarySlot] = std::sqrt (pan);

                // schedule 1-2 echoes in other idle slots
                int numEchoes = 1 + (rng.nextFloat() < 0.5f ? 1 : 0);
                int echoCount = 0;
                for (int d = 0; d < CaveState::kSlots && echoCount < numEchoes; ++d)
                {
                    if (d == primarySlot) continue;
                    if (caveState.dripEnv[d] < 0.001f && caveState.dripDelay[d] <= 0.0f)
                    {
                        float freqDrift = (echoCount == 0)
                            ? (1.0f + (rng.nextFloat() * 0.04f - 0.02f))   // ±2%
                            : (1.0f + (rng.nextFloat() * 0.10f - 0.05f));  // ±5%
                        float echoAmp = (echoCount == 0) ? 0.35f : 0.12f;
                        float delayS  = (echoCount == 0)
                            ? (0.15f + rng.nextFloat() * 0.20f)
                            : (0.35f + rng.nextFloat() * 0.30f);

                        caveState.dripFreq[d]  = freq * freqDrift;
                        caveState.dripDecay[d] = caveState.dripDecay[primarySlot];
                        caveState.dripEnv[d]   = caveState.dripEnv[primarySlot] * echoAmp * echoScale;
                        caveState.dripPhase[d] = 0.0f;
                        caveState.dripDelay[d] = delayS * sr;
                        caveState.dripPanL[d]  = caveState.dripPanL[primarySlot];
                        caveState.dripPanR[d]  = caveState.dripPanR[primarySlot];
                        // echoes: no sweep, skip noise transient
                        caveState.dripFreqStart[d] = caveState.dripFreq[d];
                        caveState.dripSweepRate[d] = 1.0f;
                        caveState.dripElapsed[d] = 0.004f * sr;
                        echoCount++;
                    }
                }
            }
        }

        // ── accumulate drips ─────────────────────────────────────────────
        float dripSumL = 0.0f, dripSumR = 0.0f;
        for (int d = 0; d < CaveState::kSlots; ++d)
        {
            // handle start delay
            if (caveState.dripDelay[d] > 0.0f)
            {
                caveState.dripDelay[d] -= 1.0f;
                continue;
            }
            if (caveState.dripEnv[d] < 0.001f) continue;

            // frequency sweep: start high, decay to base
            float elapsed = caveState.dripElapsed[d];
            float sweepDur = juce::jmax (1.0f, caveState.dripSweepRate[d]);
            float sweepFactor = std::exp (-5.0f * elapsed / sweepDur);
            float currentFreq = caveState.dripFreq[d]
                + (caveState.dripFreqStart[d] - caveState.dripFreq[d]) * sweepFactor;

            caveState.dripPhase[d] += twoPi * currentFreq / sr;
            if (caveState.dripPhase[d] > twoPi) caveState.dripPhase[d] -= twoPi;

            // noise transient for impact splash (first 3ms)
            float transientDur = 0.003f * sr;
            float transientEnv = (elapsed < transientDur)
                ? (1.0f - elapsed / transientDur) : 0.0f;
            float impactNoise = (rng.nextFloat() * 2.0f - 1.0f) * transientEnv;

            float s = (std::sin (caveState.dripPhase[d]) + impactNoise * 0.3f)
                    * caveState.dripEnv[d];
            dripSumL += s * caveState.dripPanL[d];
            dripSumR += s * caveState.dripPanR[d];

            caveState.dripEnv[d] *= caveState.dripDecay[d];
            caveState.dripElapsed[d] += 1.0f;
        }

        outL[i] = ambL + dripSumL * 0.4f;
        outR[i] = ambR + dripSumR * 0.4f;
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Type 14: Bees
// ──────────────────────────────────────────────────────────────────────────────
void UnderstoryProcessor::synthBees (float* outL, float* outR, int n,
                                      float density, float tone, float /*texture*/)
{
    const float sr    = (float)currentSampleRate;
    const float twoPi = juce::MathConstants<float>::twoPi;

    // density → active bees (1-6)
    int activeBees = 1 + (int)(density * (float)(BeeState::kBees - 1) + 0.5f);
    activeBees = juce::jlimit (1, BeeState::kBees, activeBees);

    // tone → pitch range (lower = bumblebee 120-250Hz, higher = honeybee 200-350Hz)
    float freqLow  = 120.0f + tone * 80.0f;
    float freqHigh = 250.0f + tone * 100.0f;

    // normalization for additive: sin + 0.5*sin + 0.33*sin + 0.25*sin
    static constexpr float kNorm = 1.0f / (1.0f + 0.5f + 0.33f + 0.25f);

    for (int i = 0; i < n; ++i)
    {
        float sumL = 0.0f, sumR = 0.0f;

        for (int b = 0; b < activeBees; ++b)
        {
            // ── fade in/out control ──────────────────────────────────────
            beeState.fadeTimer[b] -= 1.0f;
            if (beeState.fadeTimer[b] <= 0.0f)
            {
                // pick new target: either fade in or fade out
                if (beeState.envTarget[b] < 0.1f)
                    beeState.envTarget[b] = 0.5f + rng.nextFloat() * 0.5f;
                else
                    beeState.envTarget[b] = rng.nextFloat() < 0.3f ? 0.0f : (0.4f + rng.nextFloat() * 0.6f);

                // fade speed: 0.5-2s
                float fadeSec = 0.5f + rng.nextFloat() * 1.5f;
                beeState.envSpeed[b] = onePoleLPCoeff (1.0f / fadeSec, sr);
                // timer until next target change
                beeState.fadeTimer[b] = (1.0f + rng.nextFloat() * 4.0f) * sr;
            }

            // smooth approach to target
            beeState.env[b] += beeState.envSpeed[b] * (beeState.envTarget[b] - beeState.env[b]);

            if (beeState.env[b] < 0.001f) continue;

            // ── frequency with FM wobble ─────────────────────────────────
            float baseF = freqLow + (float)b * ((freqHigh - freqLow) / (float)(BeeState::kBees - 1));
            beeState.baseFreq[b] = baseF;

            beeState.lfoPhase[b] += twoPi * beeState.lfoRate[b] / sr;
            if (beeState.lfoPhase[b] > twoPi) beeState.lfoPhase[b] -= twoPi;

            float freq = baseF + beeState.lfoDepth[b] * std::sin (beeState.lfoPhase[b]);
            freq = juce::jlimit (50.0f, sr * 0.45f, freq);

            // ── additive synthesis (approximating sawtooth) ──────────────
            beeState.phase[b] += twoPi * freq / sr;
            if (beeState.phase[b] > twoPi) beeState.phase[b] -= twoPi;

            float ph = beeState.phase[b];
            float sample = (std::sin (ph)
                          + 0.5f  * std::sin (2.0f * ph)
                          + 0.33f * std::sin (3.0f * ph)
                          + 0.25f * std::sin (4.0f * ph)) * kNorm;

            sample *= beeState.env[b];

            // stereo spread
            float pan = (float)b / (float)(BeeState::kBees - 1);
            sumL += sample * std::sqrt (1.0f - pan);
            sumR += sample * std::sqrt (pan);
        }

        outL[i] = sumL * (0.25f / (float)BeeState::kBees);
        outR[i] = sumR * (0.25f / (float)BeeState::kBees);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// State save/restore
// ──────────────────────────────────────────────────────────────────────────────
void UnderstoryProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void UnderstoryProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* UnderstoryProcessor::createEditor()
{
    return new UnderstoryEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UnderstoryProcessor();
}
