#include "NatureLayer.h"

// pentatonic chime frequencies (E major pentatonic, 4 octaves)
static constexpr float kChimeFreqs[8] = {
    329.63f,  // E4
    493.88f,  // B4
    659.26f,  // E5
    830.61f,  // G#5
    987.77f,  // B5
    1318.51f, // E6
    1661.22f, // G#6
    1975.53f  // B6
};

void NatureLayer::prepare (double sampleRate, int)
{
    sr = sampleRate;
    b0=b1=b2=b3=b4=b5=b6=0.0f;
    breezeLpfL=breezeLpfR=0.0f;

    for (int i = 0; i < kNumChimes; ++i)
    {
        chimes[i].freq      = kChimeFreqs[i];
        chimes[i].env       = 0.0f;
        chimes[i].phase     = 0.0f;
        chimes[i].timer     = rng.nextFloat() * 6.0f + 0.5f; // staggered start
        // decay time: lower chimes ring longer (~5s), higher chimes shorter (~2s)
        float t             = (float)i / (float)(kNumChimes - 1);
        float decaySecs     = 5.0f - t * 3.0f;  // 5s → 2s across octaves
        chimes[i].decayRate = std::exp (-1.0f / (float)(sampleRate * (double)decaySecs));
        chimes[i].pan       = (rng.nextFloat() - 0.5f) * 1.4f; // -0.7 to +0.7
    }
}

float NatureLayer::pinkNoise() noexcept
{
    float w = rng.nextFloat() * 2.0f - 1.0f;
    b0=0.99886f*b0+w*0.0555179f; b1=0.99332f*b1+w*0.0750759f;
    b2=0.96900f*b2+w*0.1538520f; b3=0.86650f*b3+w*0.3104856f;
    b4=0.55000f*b4+w*0.5329522f; b5=-0.7616f*b5-w*0.0168980f;
    float out=(b0+b1+b2+b3+b4+b5+b6+w*0.5362f)*0.11f;
    b6=w*0.115926f; return out;
}

void NatureLayer::process (juce::AudioBuffer<float>& buffer, float amount) noexcept
{
    if (amount < 0.001f) return;
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    const float twoPi     = juce::MathConstants<float>::twoPi;
    const float dt        = 1.0f / (float)sr;
    const float breezeLpf = 1.0f - std::exp (-twoPi * 550.0f / (float)sr);

    auto* outL = buffer.getWritePointer (0);
    auto* outR = (numChannels > 1) ? buffer.getWritePointer (1) : buffer.getWritePointer (0);

    for (int i = 0; i < numSamples; ++i)
    {
        // --- gentle breeze (pink noise LPF, very quiet) ---
        float pinkL = pinkNoise();
        float pinkR = pinkNoise();
        breezeLpfL += breezeLpf * (pinkL - breezeLpfL);
        breezeLpfR += breezeLpf * (pinkR - breezeLpfR);
        float breezeL = breezeLpfL * 0.18f;
        float breezeR = breezeLpfR * 0.18f;

        // --- chimes ---
        float chimeSumL = 0.0f, chimeSumR = 0.0f;
        for (auto& c : chimes)
        {
            // trigger check
            c.timer -= dt;
            if (c.timer <= 0.0f && c.env < 0.005f)
            {
                c.env    = 0.55f + rng.nextFloat() * 0.45f; // velocity
                c.detune = (rng.nextFloat() - 0.5f) * 0.008f; // ±0.4% detune
                // randomise next trigger: 1.5-14 seconds
                c.timer  = rng.nextFloat() * 12.5f + 1.5f;
            }

            // sine oscillator with exponential decay
            c.env   *= c.decayRate;
            float actualFreq = c.freq * (1.0f + c.detune);
            float sample     = std::sin (c.phase) * c.env;
            c.phase += twoPi * actualFreq * dt;
            if (c.phase > twoPi) c.phase -= twoPi;

            // stereo pan
            float panL = 0.5f - c.pan * 0.5f;
            float panR = 0.5f + c.pan * 0.5f;
            chimeSumL += sample * panL;
            chimeSumR += sample * panR;
        }

        // --- mix ---
        const float g = amount * 0.032f;
        outL[i] += (breezeL + chimeSumL * 0.55f) * g;
        outR[i] += (breezeR + chimeSumR * 0.55f) * g;
    }
}
