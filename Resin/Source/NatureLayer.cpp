#include "NatureLayer.h"

void NatureLayer::prepare (double sampleRate, int)
{
    sr = sampleRate;
    b0=b1=b2=b3=b4=b5=b6=0.0f;
    hissLpfL=hissLpfR=hissHpfL=hissHpfH2L=0.0f;
    hissLpfR2=hissHpfR=hissHpfH2R=0.0f;
    crackleEnvL=crackleEnvR=0.0f;
    crackleTimerL = rng.nextFloat() * 0.8f + 0.2f;
    crackleTimerR = rng.nextFloat() * 0.8f + 0.3f;
    crackleLpfL=crackleLpfR=0.0f;
}

float NatureLayer::pinkNoise() noexcept
{
    float w = rng.nextFloat() * 2.0f - 1.0f;
    b0 = 0.99886f*b0 + w*0.0555179f;
    b1 = 0.99332f*b1 + w*0.0750759f;
    b2 = 0.96900f*b2 + w*0.1538520f;
    b3 = 0.86650f*b3 + w*0.3104856f;
    b4 = 0.55000f*b4 + w*0.5329522f;
    b5 = -0.7616f*b5 - w*0.0168980f;
    float out = (b0+b1+b2+b3+b4+b5+b6 + w*0.5362f) * 0.11f;
    b6 = w * 0.115926f;
    return out;
}

void NatureLayer::process (juce::AudioBuffer<float>& buffer, float amount) noexcept
{
    if (amount < 0.001f) return;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // filter coefficients
    const float lpfCoeff  = 1.0f - std::exp (-6.2832f *  7000.0f / (float)sr); // LPF @7kHz
    const float hpfCoeff  = 1.0f - std::exp (-6.2832f *   120.0f / (float)sr); // HPF @120Hz
    const float crackDecay = std::exp (-1.0f / ((float)sr * 0.0008f)); // ~0.8ms crackle
    const float crackLpf  = 1.0f - std::exp (-6.2832f *  3500.0f / (float)sr);
    const float dt        = 1.0f / (float)sr;
    const float gainScale = amount * 0.022f; // quiet by default

    auto applyToChannel = [&] (int ch, float& lpfSt, float& hpfSt, float& hpfH2,
                                float& crackEnv, float& crackTimer, float& crackLpfSt)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            // tape hiss: pink noise → LPF → HPF
            float pink = pinkNoise();
            lpfSt  += lpfCoeff * (pink  - lpfSt);
            float hp = lpfSt - hpfSt;
            hpfSt  += hpfCoeff * (lpfSt - hpfSt);
            // second HPF pass for steeper rolloff
            float hp2 = hp - hpfH2;
            hpfH2  += hpfCoeff * (hp    - hpfH2);
            float hiss = hp2 * 0.55f;

            // vinyl crackle
            crackTimer -= dt;
            if (crackTimer <= 0.0f)
            {
                crackEnv   = 0.65f + rng.nextFloat() * 0.35f;
                crackTimer = rng.nextFloat() * 1.4f + 0.15f; // 0.15-1.55s between crackles
            }
            crackEnv   *= crackDecay;
            float cw    = (rng.nextFloat() * 2.0f - 1.0f);
            crackLpfSt += crackLpf * (cw - crackLpfSt);
            float crack = crackLpfSt * crackEnv * 0.45f;

            data[i] += (hiss + crack) * gainScale;
        }
    };

    applyToChannel (0, hissLpfL, hissHpfL, hissHpfH2L, crackleEnvL, crackleTimerL, crackleLpfL);
    if (numChannels > 1)
        applyToChannel (1, hissLpfR2, hissHpfR, hissHpfH2R, crackleEnvR, crackleTimerR, crackleLpfR);
}
