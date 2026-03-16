#include "NatureLayer.h"

void NatureLayer::prepare (double sampleRate, int)
{
    sr = sampleRate;
    b0=b1=b2=b3=b4=b5=b6=0.0f;
    airLpfL=airLpfR=0.0f;

    // 4 birds with different pitch ranges and panning
    float minFreqs[4] = { 700.0f, 1200.0f, 1800.0f, 2600.0f };
    float maxFreqs[4] = { 1200.0f, 2000.0f, 3000.0f, 4000.0f };
    float pans[4]     = { -0.55f, 0.35f, -0.25f, 0.65f };

    for (int i = 0; i < kNumBirds; ++i)
    {
        birds[i].baseFreqMin = minFreqs[i];
        birds[i].baseFreqMax = maxFreqs[i];
        birds[i].pan         = pans[i];
        birds[i].freq        = minFreqs[i] + rng.nextFloat() * (maxFreqs[i] - minFreqs[i]);
        birds[i].timer       = rng.nextFloat() * 5.0f + 1.0f; // staggered starts
        birds[i].env         = 0.0f;
        birds[i].trillLeft   = 0;
        birds[i].trillPause  = 0.0f;
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

void NatureLayer::triggerBird (Bird& b, juce::Random& rng, double sr)
{
    // pick a random frequency in bird's range
    b.freq      = b.baseFreqMin + rng.nextFloat() * (b.baseFreqMax - b.baseFreqMin);
    b.env       = 0.35f + rng.nextFloat() * 0.65f;

    // random chirp direction: rising (positive slope) or falling (negative)
    float glideHz   = (30.0f + rng.nextFloat() * 120.0f); // total Hz change over chirp
    bool  rising    = rng.nextFloat() > 0.4f; // 60% rising
    // slope per sample = total glide / chirp duration in samples
    float chirpSecs = 0.05f + rng.nextFloat() * 0.20f; // 50–250ms chirp
    b.decayRate     = std::exp (-1.0f / (float)(sr * (double)chirpSecs));
    b.freqSlope     = (rising ? 1.0f : -1.0f) * glideHz / (float)(sr * (double)chirpSecs);

    // chance of trill (20%)
    b.trillLeft  = (rng.nextFloat() < 0.20f) ? (2 + (int)(rng.nextFloat() * 4.0f)) : 0;
    b.trillPause = 0.0f;

    // next trigger interval: 1.5-10 seconds
    b.timer = rng.nextFloat() * 8.5f + 1.5f;
}

void NatureLayer::process (juce::AudioBuffer<float>& buffer, float amount) noexcept
{
    if (amount < 0.001f) return;
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    const float twoPi     = juce::MathConstants<float>::twoPi;
    const float dt        = 1.0f / (float)sr;
    const float airLpf    = 1.0f - std::exp (-twoPi * 500.0f / (float)sr);

    auto* outL = buffer.getWritePointer (0);
    auto* outR = (numChannels > 1) ? buffer.getWritePointer (1) : buffer.getWritePointer (0);

    for (int i = 0; i < numSamples; ++i)
    {
        // --- morning air (very quiet pink LPF) ---
        airLpfL += airLpf * (pinkNoise() - airLpfL);
        airLpfR += airLpf * (pinkNoise() - airLpfR);

        // --- birds ---
        float birdSumL = 0.0f, birdSumR = 0.0f;

        for (auto& b : birds)
        {
            // trill inter-pulse pause
            if (b.trillPause > 0.0f)
            {
                b.trillPause -= dt;
                if (b.trillPause <= 0.0f && b.trillLeft > 0)
                {
                    // start next trill pulse
                    b.env = 0.30f + rng.nextFloat() * 0.40f;
                    b.decayRate = std::exp (-1.0f / (float)(sr * 0.055));
                    b.freqSlope = b.freqSlope * 0.6f; // shorter glide in trill
                    --b.trillLeft;
                    b.trillPause = 0.0f;
                }
            }

            // main trigger check
            if (b.timer > 0.0f)
                b.timer -= dt;

            if (b.timer <= 0.0f && b.env < 0.003f && b.trillPause <= 0.0f)
                triggerBird (b, rng, sr);

            // detect end of chirp → start trill pause if trills remain
            if (b.env > 0.0f && b.env < 0.003f && b.trillLeft > 0 && b.trillPause <= 0.0f)
            {
                b.env = 0.0f;
                b.trillPause = 0.04f + rng.nextFloat() * 0.06f; // 40-100ms gap
            }

            // advance oscillator
            b.env  *= b.decayRate;
            b.freq += b.freqSlope;
            b.freq  = juce::jlimit (200.0f, 6000.0f, b.freq);
            float s = std::sin (b.phase) * b.env;
            b.phase += twoPi * b.freq * dt;
            if (b.phase > twoPi) b.phase -= twoPi;

            // stereo pan
            birdSumL += s * (0.5f - b.pan * 0.5f);
            birdSumR += s * (0.5f + b.pan * 0.5f);
        }

        // --- mix ---
        const float g = amount * 0.028f;
        outL[i] += (airLpfL * 0.06f + birdSumL * 0.5f) * g;
        outR[i] += (airLpfR * 0.06f + birdSumR * 0.5f) * g;
    }
}
