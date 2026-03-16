#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

// ──────────────────────────────────────────────────────────────────────────────
// UnderstoryProcessor — procedural nature texture synthesizer
// IS_SYNTH: generates audio from scratch, no audio input consumed
// 15 texture types: Rain, Wind, Stream, Wind Chimes, Crickets, Birds, Leaves,
//   Thunder, Ocean, Fire, Frogs, Cicadas, Owls, Cave Drips, Bees
// ──────────────────────────────────────────────────────────────────────────────

class UnderstoryProcessor : public juce::AudioProcessor
{
public:
    UnderstoryProcessor();
    ~UnderstoryProcessor() override = default;

    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock   (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName()    const override { return "Understory"; }
    bool acceptsMidi()  const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int  getNumPrograms()                             override { return 1; }
    int  getCurrentProgram()                          override { return 0; }
    void setCurrentProgram (int)                      override {}
    const juce::String getProgramName (int)           override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // scope ring buffer — written by audio thread, read by editor timer
    static constexpr int kScopeSize = 512;
    float scopeBuffer[kScopeSize] = {};
    std::atomic<int> scopeWritePos { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ── Pink noise (Paul Kellet 7-filter approximation) ───────────────────
    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f, b3 = 0.0f,
          b4 = 0.0f, b5 = 0.0f, b6 = 0.0f;

    float pinkNoise() noexcept
    {
        float w = rng.nextFloat() * 2.0f - 1.0f;
        b0 = 0.99886f * b0 + w * 0.0555179f;
        b1 = 0.99332f * b1 + w * 0.0750759f;
        b2 = 0.96900f * b2 + w * 0.1538520f;
        b3 = 0.86650f * b3 + w * 0.3104856f;
        b4 = 0.55000f * b4 + w * 0.5329522f;
        b5 = -0.7616f * b5 - w * 0.0168980f;
        float out = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f) * 0.11f;
        b6 = w * 0.115926f;
        return out;
    }

    // separate pink noise instance for second channel (stereo independence)
    float rb0 = 0.0f, rb1 = 0.0f, rb2 = 0.0f, rb3 = 0.0f,
          rb4 = 0.0f, rb5 = 0.0f, rb6 = 0.0f;

    float pinkNoiseR() noexcept
    {
        float w = rng.nextFloat() * 2.0f - 1.0f;
        rb0 = 0.99886f * rb0 + w * 0.0555179f;
        rb1 = 0.99332f * rb1 + w * 0.0750759f;
        rb2 = 0.96900f * rb2 + w * 0.1538520f;
        rb3 = 0.86650f * rb3 + w * 0.3104856f;
        rb4 = 0.55000f * rb4 + w * 0.5329522f;
        rb5 = -0.7616f * rb5 - w * 0.0168980f;
        float out = (rb0 + rb1 + rb2 + rb3 + rb4 + rb5 + rb6 + w * 0.5362f) * 0.11f;
        rb6 = w * 0.115926f;
        return out;
    }

    // helper: one-pole IIR coefficient from cutoff Hz
    float onePoleLPCoeff (float cutoffHz, float sr) const noexcept
    {
        return 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cutoffHz / sr);
    }

    float onePoleHPCoeff (float cutoffHz, float sr) const noexcept
    {
        // HP complement: coeff is same shape, applied as y = x - lp_state
        return 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cutoffHz / sr);
    }

    // ── Type 0: Rain ──────────────────────────────────────────────────────
    struct RainState
    {
        // hiss filter states (2-pole HPF via two cascaded one-pole)
        float hpfL1 = 0.0f, hpfL2 = 0.0f;
        float hpfR1 = 0.0f, hpfR2 = 0.0f;

        // 16 raindrop pool
        static constexpr int kDrops = 16;
        float dropEnv[kDrops]    = {};  // current envelope amplitude
        float dropDecay[kDrops]  = {};  // per-sample decay multiplier
        float dropBpL[kDrops]    = {};  // bandpass state L
        float dropBpR[kDrops]    = {};  // bandpass state R
        float dropPanL[kDrops]   = {};  // panning gain L
        float dropPanR[kDrops]   = {};  // panning gain R
        float dropBpCoeff[kDrops]= {};  // bandpass filter coeff

        // Poisson accumulator for drop triggering
        float poissonAcc = 0.0f;
    };

    // ── Type 1: Wind ──────────────────────────────────────────────────────
    struct WindState
    {
        // low rumble LPF states
        float lpfLowL = 0.0f, lpfLowR = 0.0f;
        // high whistle HPF states
        float hpfHighL1 = 0.0f, hpfHighL2 = 0.0f;
        float hpfHighR1 = 0.0f, hpfHighR2 = 0.0f;
        // gust LFO
        float gustPhase  = 0.0f;
        float gustTarget = 0.5f;
        float gustActual = 0.5f;
        float gustTimer  = 0.0f;
    };

    // ── Type 2: Stream / Running Water ────────────────────────────────────
    struct StreamState
    {
        // 3 formant bandpass states (L and R)
        float bpStateL[3] = {};
        float bpStateR[3] = {};
        // LFO phase for each formant sweep
        float formantPhase[3] = {};
        // low rumble LPF
        float rumbleLpfL = 0.0f, rumbleLpfR = 0.0f;
    };

    // ── Type 3: Wind Chimes ───────────────────────────────────────────────
    struct ChimeState
    {
        static constexpr int kChimes = 8;
        float phase[kChimes]      = {};  // sine oscillator phase
        float env[kChimes]        = {};  // envelope
        float decay[kChimes]      = {};  // per-sample decay mult
        float panL[kChimes]       = {};  // gain L
        float panR[kChimes]       = {};  // gain R
        // Poisson trigger timers (samples until next trigger)
        float trigTimer[kChimes]  = {};
        // pitches: E pentatonic 329,415,494,659,784,988,1319,1976 Hz
        static constexpr float kFreqs[kChimes] = {
            329.0f, 415.0f, 494.0f, 659.0f,
            784.0f, 988.0f, 1319.0f, 1976.0f
        };
    };

    // ── Type 4: Crickets / Insects ────────────────────────────────────────
    struct CricketState
    {
        static constexpr int kCrickets = 6;
        float carrierPhase[kCrickets] = {};  // carrier sine phase
        float amPhase[kCrickets]      = {};  // AM modulator phase
        float env[kCrickets]          = {};  // burst envelope
        float envDecay[kCrickets]     = {};  // per-sample decay
        float burstTimer[kCrickets]   = {};  // samples until next burst
        float carrierFreq[kCrickets]  = {};  // Hz, set in prepareToPlay
        float amFreq[kCrickets]       = {};  // Hz chirp rate, set in prepareToPlay
        float panL[kCrickets]         = {};
        float panR[kCrickets]         = {};
    };

    // ── Type 5: Birds / Birdsong ──────────────────────────────────────────
    struct BirdState
    {
        static constexpr int kBirds = 4;
        float phase[kBirds]        = {};  // carrier sine phase
        float freqCurrent[kBirds]  = {};  // current frequency Hz
        float freqTarget[kBirds]   = {};  // glide target Hz
        float env[kBirds]          = {};  // peak envelope (decays)
        float envDecay[kBirds]     = {};  // per-sample decay multiplier
        float envAttack[kBirds]    = {};  // attack ramp 0→1
        float attackRate[kBirds]   = {};  // per-sample attack increment
        float trigTimer[kBirds]    = {};  // samples until next chirp
        int   trillCount[kBirds]   = {};  // remaining trill notes
        float trillTimer[kBirds]   = {};  // samples until next trill note
        float glideSpeed[kBirds]   = {};  // Hz per sample
        bool  glideUp[kBirds]      = {};  // glide direction
        float vibratoPhase[kBirds] = {};  // vibrato LFO phase
        float vibratoRate[kBirds]  = {};  // vibrato Hz (set per bird in prepare)
        float panL[kBirds]         = {};
        float panR[kBirds]         = {};
        float modPhase[kBirds]     = {};  // FM modulator phase
        float modRatio[kBirds]     = {};  // FM frequency ratio (1.2-2.5)
        float fmIndex[kBirds]      = {};  // FM modulation index
        float noiseBpL[kBirds]     = {};  // noise bandpass LP state
        float noiseBpH[kBirds]     = {};  // noise bandpass HP state
    };

    // ── Type 6: Leaves / Rustling ─────────────────────────────────────────
    struct LeavesState
    {
        // rustle HPF states
        float hpfL1 = 0.0f, hpfL2 = 0.0f;
        float hpfR1 = 0.0f, hpfR2 = 0.0f;
        // gust envelope
        float gustLevel   = 0.0f;
        float gustTarget  = 0.3f;
        float gustAttack  = 0.0f;  // attack coeff per sample
        float gustTimer   = 0.0f;  // samples until new gust target
        // leaf snap pool (Poisson)
        static constexpr int kSnaps = 8;
        float snapEnv[kSnaps]   = {};
        float snapDecay[kSnaps] = {};
        float snapPanL[kSnaps]  = {};
        float snapPanR[kSnaps]  = {};
        float snapTimer         = 0.0f;
    };

    // ── Type 7: Thunder / Storm ────────────────────────────────────────────
    struct ThunderState
    {
        // base rumble LPF
        float rumbleLpfL = 0.0f, rumbleLpfR = 0.0f;
        // rain bed HPF (2-pole)
        float rainHpfL1 = 0.0f, rainHpfL2 = 0.0f;
        float rainHpfR1 = 0.0f, rainHpfR2 = 0.0f;
        // thunder event pool
        static constexpr int kSlots = 3;
        float crackEnv[kSlots]      = {};  // crack envelope
        float crackDecay[kSlots]    = {};  // crack per-sample decay
        float rumbleEnv[kSlots]     = {};  // rumble envelope
        float rumbleDecay[kSlots]   = {};  // rumble per-sample decay
        float rumbleDelay[kSlots]   = {};  // samples until rumble starts
        float rumbleLpf[kSlots]     = {};  // rumble LPF state (mono, panned)
        float crackHpf[kSlots]      = {};  // crack HPF state
        float panL[kSlots]          = {};
        float panR[kSlots]          = {};
        float trigTimer             = 0.0f;
    };

    // ── Type 8: Ocean / Surf ─────────────────────────────────────────────
    struct OceanState
    {
        float wavePhase      = 0.0f;  // 0-1 repeating
        float wavePeriod     = 8.0f;  // seconds
        float nextPeriod     = 8.0f;
        // undertow LPF
        float undertowLpfL   = 0.0f, undertowLpfR = 0.0f;
        // wave body LPF
        float waveLpfL       = 0.0f, waveLpfR = 0.0f;
        // crash HPF (2-pole)
        float crashHpfL1     = 0.0f, crashHpfL2 = 0.0f;
        float crashHpfR1     = 0.0f, crashHpfR2 = 0.0f;
    };

    // ── Type 9: Fire / Campfire ──────────────────────────────────────────
    struct FireState
    {
        // base roar LPF
        float roarLpfL = 0.0f, roarLpfR = 0.0f;
        float roarLfoPhase = 0.0f;
        // hiss HPF (2-pole)
        float hissHpfL1 = 0.0f, hissHpfL2 = 0.0f;
        float hissHpfR1 = 0.0f, hissHpfR2 = 0.0f;
        // crackle pool
        static constexpr int kCrackles = 12;
        float crackleEnv[kCrackles]   = {};
        float crackleDecay[kCrackles] = {};
        float cracklePanL[kCrackles]  = {};
        float cracklePanR[kCrackles]  = {};
        float crackleHpf[kCrackles]   = {};
        float crackleAcc              = 0.0f;  // Poisson acc
        // pop pool
        static constexpr int kPops = 4;
        float popEnv[kPops]       = {};
        float popDecay[kPops]     = {};
        float popPhase[kPops]     = {};
        float popFreq[kPops]      = {};
        float popPanL[kPops]      = {};
        float popPanR[kPops]      = {};
        float popAcc              = 0.0f;
    };

    // ── Type 10: Frogs ───────────────────────────────────────────────────
    struct FrogState
    {
        // spring peepers
        static constexpr int kPeepers = 6;
        float peeperPhase[kPeepers]    = {};
        float peeperBaseFreq[kPeepers] = {};
        float peeperSweep[kPeepers]    = {};  // Hz to sweep up
        float peeperEnv[kPeepers]      = {};
        float peeperDecay[kPeepers]    = {};
        float peeperTimer[kPeepers]    = {};  // samples until next call
        float peeperElapsed[kPeepers]  = {};  // samples into current call
        float peeperDur[kPeepers]      = {};  // total call duration in samples
        float peeperPanL[kPeepers]     = {};
        float peeperPanR[kPeepers]     = {};
        float peeperNoiseBpL[kPeepers] = {};  // noise bandpass LP state
        float peeperNoiseBpH[kPeepers] = {};  // noise bandpass HP state
        // tree frogs
        static constexpr int kTreeFrogs = 3;
        float tfPhase[kTreeFrogs]      = {};
        float tfFreq1[kTreeFrogs]      = {};  // first note freq
        float tfFreq2[kTreeFrogs]      = {};  // second note freq
        float tfEnv[kTreeFrogs]        = {};
        float tfTimer[kTreeFrogs]      = {};
        float tfElapsed[kTreeFrogs]    = {};
        float tfDur1[kTreeFrogs]       = {};  // first note dur samples
        float tfDur2[kTreeFrogs]       = {};  // second note dur samples
        float tfPanL[kTreeFrogs]       = {};
        float tfPanR[kTreeFrogs]       = {};
        float tfFmPhase[kTreeFrogs]    = {};  // FM wobble phase
    };

    // ── Type 11: Cicadas ─────────────────────────────────────────────────
    struct CicadaState
    {
        static constexpr int kVoices = 6;
        float carrierPhase[kVoices] = {};
        float carrierFreq[kVoices]  = {};
        float amPhase[kVoices]      = {};
        float amFreq[kVoices]       = {};
        // swell envelope: 0=rising, 1=holding, 2=falling, 3=silence
        int   swellStage[kVoices]   = {};
        float swellEnv[kVoices]     = {};
        float swellTimer[kVoices]   = {};  // samples remaining in current stage
        float carrierPhase2[kVoices] = {};  // detuned carrier 2
        float carrierPhase3[kVoices] = {};  // detuned carrier 3
        float detune1[kVoices]       = {};  // Hz offset for carrier 2
        float detune2[kVoices]       = {};  // Hz offset for carrier 3
        float noiseBpL[kVoices]      = {};  // noise bandpass LP state
        float noiseBpH[kVoices]      = {};  // noise bandpass HP state
    };

    // ── Type 12: Owls / Night ────────────────────────────────────────────
    struct OwlState
    {
        static constexpr int kOwls = 2;
        float baseFreq[kOwls]    = {};
        float phase[kOwls]       = {};
        float env[kOwls]         = {};
        int   noteIndex[kOwls]   = {};  // which note in 5-note pattern (0-4), -1 = gap
        float noteTimer[kOwls]   = {};  // samples remaining in current note/gap
        float callTimer[kOwls]   = {};  // samples until next call sequence
        float noteFreq[kOwls]    = {};  // current note frequency
        float panL[kOwls]        = {};
        float panR[kOwls]        = {};
        float noiseBpL[kOwls]    = {};  // formant bandpass LP state
        float noiseBpH[kOwls]    = {};  // formant bandpass HP state
        // night air LPF
        float nightLpfL = 0.0f, nightLpfR = 0.0f;
    };

    // ── Type 13: Cave Drips ──────────────────────────────────────────────
    struct CaveState
    {
        static constexpr int kSlots = 12;
        float dripPhase[kSlots]     = {};
        float dripFreq[kSlots]      = {};
        float dripEnv[kSlots]       = {};
        float dripDecay[kSlots]     = {};
        float dripPanL[kSlots]      = {};
        float dripPanR[kSlots]      = {};
        float dripDelay[kSlots]     = {};  // start delay in samples (for echoes)
        float dripFreqStart[kSlots] = {};  // sweep start frequency
        float dripSweepRate[kSlots] = {};  // sweep duration in samples
        float dripElapsed[kSlots]   = {};  // samples into current drip
        float dripAcc               = 0.0f;
        // cave ambient LPF
        float ambientLpfL = 0.0f, ambientLpfR = 0.0f;
    };

    // ── Type 14: Bees ────────────────────────────────────────────────────
    struct BeeState
    {
        static constexpr int kBees = 6;
        float phase[kBees]      = {};
        float baseFreq[kBees]   = {};
        float lfoPhase[kBees]   = {};
        float lfoRate[kBees]    = {};
        float lfoDepth[kBees]   = {};  // Hz deviation
        float env[kBees]        = {};
        float envTarget[kBees]  = {};
        float envSpeed[kBees]   = {};  // per-sample approach rate
        float fadeTimer[kBees]  = {};  // samples until envTarget changes
    };

    // ── Per-type state instances ───────────────────────────────────────────
    RainState    rainState;
    WindState    windState;
    StreamState  streamState;
    ChimeState   chimeState;
    CricketState cricketState;
    BirdState    birdState;
    LeavesState  leavesState;
    ThunderState thunderState;
    OceanState   oceanState;
    FireState    fireState;
    FrogState    frogState;
    CicadaState  cicadaState;
    OwlState     owlState;
    CaveState    caveState;
    BeeState     beeState;

    // ── Per-type synthesis functions ──────────────────────────────────────
    void synthRain    (float* outL, float* outR, int n, float density, float tone, float texture);
    void synthWind    (float* outL, float* outR, int n, float density, float tone, float texture);
    void synthStream  (float* outL, float* outR, int n, float density, float tone, float texture);
    void synthChimes  (float* outL, float* outR, int n, float density, float tone, float texture);
    void synthCrickets(float* outL, float* outR, int n, float density, float tone, float texture);
    void synthBirds   (float* outL, float* outR, int n, float density, float tone, float texture);
    void synthLeaves  (float* outL, float* outR, int n, float density, float tone, float texture);
    void synthThunder (float* outL, float* outR, int n, float density, float tone, float texture);
    void synthOcean   (float* outL, float* outR, int n, float density, float tone, float texture);
    void synthFire    (float* outL, float* outR, int n, float density, float tone, float texture);
    void synthFrogs   (float* outL, float* outR, int n, float density, float tone, float texture);
    void synthCicadas (float* outL, float* outR, int n, float density, float tone, float texture);
    void synthOwls    (float* outL, float* outR, int n, float density, float tone, float texture);
    void synthCave    (float* outL, float* outR, int n, float density, float tone, float texture);
    void synthBees    (float* outL, float* outR, int n, float density, float tone, float texture);

    // ── Smoothed parameters ────────────────────────────────────────────────
    float smoothDensity  = 0.5f;
    float smoothTone     = 0.5f;
    float smoothTexture  = 0.5f;
    float smoothWidth    = 0.5f;
    float smoothLevel    = 0.6f;
    float smoothMacro1   = 0.0f;
    float smoothMacro2   = 0.0f;

    bool   isPrepared        = false;
    double currentSampleRate = 44100.0;

    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UnderstoryProcessor)
};
