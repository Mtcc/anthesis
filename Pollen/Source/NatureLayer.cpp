#include "NatureLayer.h"

void NatureLayer::prepare (double sampleRate, int)
{
    sr = sampleRate;
    b0=b1=b2=b3=b4=b5=b6=0.0f;
    hpfL1=hpfL2=hpfR1=hpfR2=0.0f;
    gustLevel  = 0.4f;
    gustTarget = 0.4f + rng.nextFloat() * 0.3f;
    gustTimer  = rng.nextFloat() * 1.5f + 0.3f;
    rustleTimer = rng.nextFloat() * 3.0f + 1.0f;
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

    // HPF coefficient for leaf rustle texture: two-pole at ~2.2kHz
    const float hpfCoeff = 1.0f - std::exp (-twoPi * 2200.0f / (float)sr);
    // per-sample gust smoothing: very slow (aim for ~0.3s time constant)
    const float gustSmooth = std::exp (-dt / 0.30f);
    // rustle decay
    const float rDecay = std::exp (-1.0f / ((float)sr * 0.012f)); // ~12ms sharp rustle

    auto* outL = buffer.getWritePointer (0);
    auto* outR = (numChannels > 1) ? buffer.getWritePointer (1) : buffer.getWritePointer (0);

    for (int i = 0; i < numSamples; ++i)
    {
        // --- gust timer ---
        gustTimer -= dt;
        if (gustTimer <= 0.0f)
        {
            gustTarget  = rng.nextFloat() * 0.85f + 0.05f; // 0.05–0.90
            gustTimer   = rng.nextFloat() * 2.2f + 0.25f;  // new gust every 0.25-2.45s
        }
        // smooth toward target
        gustLevel += (1.0f - gustSmooth) * (gustTarget - gustLevel);

        // --- leaf rustle noise (separate L/R for stereo) ---
        auto applyHpf2 = [] (float in, float& s1, float& s2, float c) -> float {
            float hp1 = in - s1; s1 += c * (in - s1);
            float hp2 = hp1 - s2; s2 += c * (hp1 - s2);
            return hp2;
        };

        float leafL = applyHpf2 (pinkNoise(), hpfL1, hpfL2, hpfCoeff) * gustLevel;
        float leafR = applyHpf2 (pinkNoise(), hpfR1, hpfR2, hpfCoeff) * gustLevel;

        // --- occasional sharp rustle (single leaf snap) ---
        rustleTimer -= dt;
        if (rustleTimer <= 0.0f)
        {
            rustleEnvL = 0.5f + rng.nextFloat() * 0.5f;
            rustleEnvR = rustleEnvL * (0.7f + rng.nextFloat() * 0.3f);
            rustleDecay = std::exp (-1.0f / ((float)sr * (0.006f + rng.nextFloat() * 0.018f)));
            rustleTimer = rng.nextFloat() * 3.5f + 0.4f;
        }
        rustleEnvL *= rDecay;
        rustleEnvR *= rDecay;
        float rustleNoiseL = (rng.nextFloat() * 2.0f - 1.0f) * rustleEnvL;
        float rustleNoiseR = (rng.nextFloat() * 2.0f - 1.0f) * rustleEnvR;
        // HPF the rustle too
        float rHL = applyHpf2 (rustleNoiseL, hpfL1, hpfL2, hpfCoeff) * 0.0f + rustleNoiseL; // raw is fine
        float rHR = applyHpf2 (rustleNoiseR, hpfR1, hpfR2, hpfCoeff) * 0.0f + rustleNoiseR;

        // --- mix ---
        const float g = amount * 0.030f;
        outL[i] += (leafL * 0.8f + rHL * 0.25f) * g;
        outR[i] += (leafR * 0.8f + rHR * 0.25f) * g;
    }
}
