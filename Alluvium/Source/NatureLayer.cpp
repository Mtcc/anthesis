#include "NatureLayer.h"

void NatureLayer::prepare (double sampleRate, int)
{
    sr = sampleRate;
    b0=b1=b2=b3=b4=b5=b6=0.0f;
    rumbleLpfL=rumbleLpfR=0.0f;

    // 3 formants with different sweep ranges and rates
    // formant 0: low rumble-ish (150-450Hz), slow sweep
    // formant 1: mid burble  (300-900Hz), medium sweep
    // formant 2: high splash (600-1800Hz), faster sweep
    float centers[3]  = { 280.0f,  550.0f, 1100.0f };
    float sweeps[3]   = { 150.0f,  320.0f,  700.0f };
    float lfoRates[3] = {   0.13f,   0.21f,    0.37f };
    float phases[3]   = {   0.0f,   2.09f,    4.19f }; // 0, 120°, 240° offsets

    for (int i = 0; i < kNumFormants; ++i)
    {
        formants[i].centerFreq = centers[i];
        formants[i].sweepRange = sweeps[i];
        formants[i].lfoRate    = lfoRates[i];
        formants[i].lfoPhase   = phases[i];
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
    const float rumbleLpfCoeff = 1.0f - std::exp (-twoPi * 180.0f / (float)sr);

    // Formant gains (lower formants louder = realistic water spectrum)
    const float formantGains[3] = { 0.55f, 0.40f, 0.28f };

    auto* outL = buffer.getWritePointer (0);
    auto* outR = (numChannels > 1) ? buffer.getWritePointer (1) : buffer.getWritePointer (0);

    for (int i = 0; i < numSamples; ++i)
    {
        float noiseL = pinkNoise();
        float noiseR = pinkNoise();

        // --- 3 formant bandpass filters ---
        float streamL = 0.0f, streamR = 0.0f;
        for (int f = 0; f < kNumFormants; ++f)
        {
            auto& fm = formants[f];

            // LFO-driven center frequency
            fm.lfoPhase += twoPi * fm.lfoRate * dt;
            float currentFreq = fm.centerFreq + std::sin (fm.lfoPhase) * (fm.sweepRange * 0.5f);
            currentFreq = juce::jlimit (60.0f, (float)sr * 0.45f, currentFreq);

            // bandpass = LPF1 - LPF2 (different cutoffs)
            // coeff1 = narrow, coeff2 = wide (subtraction gives band)
            float c1 = 1.0f - std::exp (-twoPi * currentFreq / (float)sr);
            float c2 = 1.0f - std::exp (-twoPi * currentFreq * 2.2f / (float)sr);

            // L channel
            fm.lpf1L += c1 * (noiseL - fm.lpf1L);
            fm.lpf2L += c2 * (noiseL - fm.lpf2L);
            float bpL = (fm.lpf1L - fm.lpf2L) * formantGains[f];

            // R channel (same formant, different noise)
            fm.lpf1R += c1 * (noiseR - fm.lpf1R);
            fm.lpf2R += c2 * (noiseR - fm.lpf2R);
            float bpR = (fm.lpf1R - fm.lpf2R) * formantGains[f];

            streamL += bpL;
            streamR += bpR;
        }

        // --- low rumble for depth ---
        rumbleLpfL += rumbleLpfCoeff * (noiseL - rumbleLpfL);
        rumbleLpfR += rumbleLpfCoeff * (noiseR - rumbleLpfR);

        // --- mix ---
        const float g = amount * 0.038f;
        outL[i] += (streamL * 0.7f + rumbleLpfL * 0.18f) * g;
        outR[i] += (streamR * 0.7f + rumbleLpfR * 0.18f) * g;
    }
}
