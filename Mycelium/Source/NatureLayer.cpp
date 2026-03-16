#include "NatureLayer.h"

void NatureLayer::prepare (double sampleRate, int)
{
    sr = sampleRate;
    b0=b1=b2=b3=b4=b5=b6=0.0f;
    hissHpfL=hissHpfR=hissHpfH2L=hissHpfH2R=0.0f;
    for (auto& d : drops) d = Drop{};
    rainTimer = 0.3f;

    // seed crickets at different frequencies / phase offsets
    float baseFreqs[3] = { 3800.0f, 4200.0f, 4600.0f };
    float chirpRates[3] = { 17.5f, 19.2f, 16.8f };
    for (int i = 0; i < 3; ++i)
    {
        crickets[i].freq      = baseFreqs[i];
        crickets[i].chirpRate = chirpRates[i];
        crickets[i].phase     = rng.nextFloat() * 6.283f;
        crickets[i].chirpPhase = rng.nextFloat() * 6.283f;
        crickets[i].timer     = rng.nextFloat() * 3.0f + 0.5f;
        crickets[i].env       = 0.0f;
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
    const float hpfCoeff  = 1.0f - std::exp (-twoPi * 800.0f  / (float)sr); // HPF @800Hz
    const float dropDecAvg = std::exp (-1.0f / ((float)sr * 0.025f)); // ~25ms drop decay avg
    const float dropsPerSec = 25.0f; // at full amount

    // get output pointers
    auto* outL = buffer.getWritePointer (0);
    auto* outR = (numChannels > 1) ? buffer.getWritePointer (1) : buffer.getWritePointer (0);

    for (int i = 0; i < numSamples; ++i)
    {
        // --- rain hiss (pink noise HPF filtered) ---
        float pink = pinkNoise();
        float hpL = pink - hissHpfL; hissHpfL += hpfCoeff * (pink - hissHpfL);
        float hp2L = hpL - hissHpfH2L; hissHpfH2L += hpfCoeff * (hpL - hissHpfH2L);
        float pink2 = pinkNoise();
        float hpR = pink2 - hissHpfR; hissHpfR += hpfCoeff * (pink2 - hissHpfR);
        float hp2R = hpR - hissHpfH2R; hissHpfH2R += hpfCoeff * (hpR - hissHpfH2R);
        float hissL = hp2L * 0.35f, hissR = hp2R * 0.35f;

        // --- raindrop scheduler ---
        rainTimer -= dt;
        if (rainTimer <= 0.0f)
        {
            // find inactive drop slot
            for (auto& d : drops)
            {
                if (d.envL < 0.002f && d.envR < 0.002f)
                {
                    float dropSize  = rng.nextFloat();
                    d.decayRate     = 0.990f + dropSize * 0.009f; // 10-100ms decay
                    float pan       = (rng.nextFloat() - 0.5f) * 0.6f; // subtle stereo
                    d.gainL = 0.5f - pan * 0.5f;
                    d.gainR = 0.5f + pan * 0.5f;
                    d.envL = d.gainL * (0.3f + rng.nextFloat() * 0.7f);
                    d.envR = d.gainR * (0.3f + rng.nextFloat() * 0.7f);
                    d.lpfStateL = d.lpfStateR = 0.0f;
                    float dropLpfHz = 1500.0f + rng.nextFloat() * 2500.0f;
                    // store freq as coefficient approximation — fixed for this drop
                    (void) dropLpfHz; // will apply uniform coeff below
                    break;
                }
            }
            rainTimer = -std::log (1.0f - rng.nextFloat()) / (dropsPerSec * amount + 1.0f);
        }

        // --- process drops ---
        const float dropLpf = 1.0f - std::exp (-twoPi * 3000.0f / (float)sr);
        float dropSumL = 0.0f, dropSumR = 0.0f;
        for (auto& d : drops)
        {
            if (d.envL < 0.001f) continue;
            float noise = rng.nextFloat() * 2.0f - 1.0f;
            d.lpfStateL += dropLpf * (noise - d.lpfStateL);
            d.lpfStateR += dropLpf * (noise - d.lpfStateR);
            dropSumL += d.lpfStateL * d.envL;
            dropSumR += d.lpfStateR * d.envR;
            d.envL *= d.decayRate;
            d.envR *= d.decayRate;
        }

        // --- crickets (very quiet, distant) ---
        float cricketOut = 0.0f;
        for (auto& c : crickets)
        {
            c.timer -= dt;
            if (c.timer <= 0.0f && c.env < 0.005f)
            {
                c.env   = 0.18f + rng.nextFloat() * 0.12f;
                c.timer = rng.nextFloat() * 4.0f + 0.5f; // chirp then pause
            }
            // AM chirp modulation
            c.chirpPhase += twoPi * c.chirpRate * dt;
            float chirpAM = 0.5f + 0.5f * std::sin (c.chirpPhase);
            float sample  = std::sin (c.phase) * c.env * chirpAM;
            c.phase += twoPi * c.freq * dt;
            c.env *= c.decayRate;
            cricketOut += sample;
        }

        // --- mix ---
        const float g = amount * 0.028f;
        outL[i] += (hissL + dropSumL * 0.4f + cricketOut * 0.08f) * g;
        outR[i] += (hissR + dropSumR * 0.4f + cricketOut * 0.08f) * g;
    }
}
