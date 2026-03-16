#include "PluginEditor.h"

namespace
{
    constexpr float kHeaderH   = 58.0f;
    constexpr float kHaloH     = 168.0f;  // display panel height
    constexpr float kMacroH    = 110.0f;
    constexpr float kFooterH   = 42.0f;
    constexpr float kKnobSize  = 62.0f;
    constexpr float kMacroSize = 86.0f;
    constexpr float kTimerHz   = 30.0f;

    juce::Font titleFont() { return juce::Font (juce::FontOptions ("Georgia", 20.0f, juce::Font::plain)); }
    juce::Font labelFont() { return juce::Font (juce::FontOptions ("Georgia", 11.0f, juce::Font::plain)); }
}

AureoleEditor::AureoleEditor (AureoleProcessor& p)
    : AudioProcessorEditor (p), proc (p), particles (rng)
{
    setSize (kW, kH);
    setLookAndFeel (&laf);

    auto configKnob = [&] (juce::Slider& s, juce::Label& l, const juce::String& name)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 14);
        s.setColour (juce::Slider::textBoxTextColourId,       AC::warmAmber());
        s.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (s);

        l.setText (name, juce::dontSendNotification);
        l.setFont (labelFont());
        l.setColour (juce::Label::textColourId, AC::parchment().withAlpha (0.75f));
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };

    configKnob (rateKnob,   rateLabel,   "Rate");
    configKnob (depthKnob,  depthLabel,  "Depth");
    configKnob (spreadKnob, spreadLabel, "Spread");
    configKnob (warmthKnob, warmthLabel, "Warmth");
    configKnob (mixKnob,    mixLabel,    "Mix");
    configKnob (macro1Knob, macro1Label, "Drift");
    configKnob (macro2Knob, macro2Label, "Warmth");
    configKnob (natureKnob, natureLabel, "Nature");

    rateAttach   = std::make_unique<Attachment> (proc.apvts, "rate",   rateKnob);
    depthAttach  = std::make_unique<Attachment> (proc.apvts, "depth",  depthKnob);
    spreadAttach = std::make_unique<Attachment> (proc.apvts, "spread", spreadKnob);
    warmthAttach = std::make_unique<Attachment> (proc.apvts, "warmth", warmthKnob);
    mixAttach    = std::make_unique<Attachment> (proc.apvts, "mix",    mixKnob);
    macro1Attach = std::make_unique<Attachment> (proc.apvts, "macro1", macro1Knob);
    macro2Attach = std::make_unique<Attachment> (proc.apvts, "macro2", macro2Knob);
    natureAttach = std::make_unique<Attachment> (proc.apvts, "nature", natureKnob);

    // presets
    struct PresetData { const char* name; float rate, depth, warmth, spread, mix; };
    static const PresetData kPresets[] = {
        { "Morning Dew",       0.2f, 0.2f, 0.3f,  0.8f,  0.35f },
        { "Cocteau Twins",     0.4f, 0.6f, 0.7f,  0.9f,  0.60f },
        { "Slowdive",          0.35f,0.5f, 0.5f,  0.8f,  0.55f },
        { "My Bloody Valentine",0.5f,0.7f, 0.8f,  0.85f, 0.70f },
        { "Laraaji",           0.2f, 0.35f,0.4f,  0.8f,  0.40f },
        { "Deep Ensemble",     0.15f,0.85f,0.6f,  0.8f,  0.65f },
    };

    for (int i = 0; i < 6; ++i)
        presetBox.addItem (kPresets[i].name, i + 1);

    presetBox.setSelectedId (1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        struct PresetData { float rate, depth, warmth, spread, mix; };
        static const PresetData ps[] = {
            { 0.2f,  0.2f,  0.3f,  0.8f,  0.35f },
            { 0.4f,  0.6f,  0.7f,  0.9f,  0.60f },
            { 0.35f, 0.5f,  0.5f,  0.8f,  0.55f },
            { 0.5f,  0.7f,  0.8f,  0.85f, 0.70f },
            { 0.2f,  0.35f, 0.4f,  0.8f,  0.40f },
            { 0.15f, 0.85f, 0.6f,  0.8f,  0.65f },
        };
        int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx < 6)
        {
            proc.apvts.getParameter ("rate")  ->setValueNotifyingHost (ps[idx].rate);
            proc.apvts.getParameter ("depth") ->setValueNotifyingHost (ps[idx].depth);
            proc.apvts.getParameter ("warmth")->setValueNotifyingHost (ps[idx].warmth);
            proc.apvts.getParameter ("spread")->setValueNotifyingHost (ps[idx].spread);
            proc.apvts.getParameter ("mix")   ->setValueNotifyingHost (ps[idx].mix);
        }
    };
    addAndMakeVisible (presetBox);

    particles.reset (getLocalBounds().toFloat());
    startTimerHz ((int)kTimerHz);
}

AureoleEditor::~AureoleEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

// ── timer ──────────────────────────────────────────────────────────────────────
void AureoleEditor::timerCallback()
{
    constexpr float dt = 1.0f / kTimerHz;
    animTime    += dt;
    breathPhase += dt * 0.18f;
    idleBreath  += dt * 0.10f;

    lightDriftX = std::sin (animTime * 0.021f) * 22.0f;
    lightDriftY = std::sin (animTime * 0.013f) * 16.0f;

    macro1Phase += dt * 0.08f;
    macro2Phase += dt * 0.06f;

    // copy LFO phases from processor
    for (int v = 0; v < 4; ++v)
        lfoPhases[v] = proc.lfoPhaseReadout[v];

    // idle detection — check max output amplitude
    float maxSig = 0.0f;
    for (int i = 0; i < AureoleProcessor::kScopeSize; ++i)
        maxSig = juce::jmax (maxSig, std::abs (proc.scopeOutput[i]));

    if (maxSig > 0.001f)
    {
        idleTimer = 0.0f;
        isIdle    = false;
    }
    else
    {
        idleTimer += dt;
        if (idleTimer > 1.5f) isIdle = true;
    }

    // smooth amplitude for orbit pulse
    float targetAmp = isIdle ? 0.0f : maxSig;
    signalAmplitude += 0.15f * (targetAmp - signalAmplitude);

    // copy scope data when active
    if (!isIdle)
    {
        int wPos = proc.scopeWritePos.load (std::memory_order_relaxed);
        for (int i = 0; i < AureoleProcessor::kScopeSize; ++i)
        {
            int src = (wPos - AureoleProcessor::kScopeSize + i + AureoleProcessor::kScopeSize)
                      & (AureoleProcessor::kScopeSize - 1);
            scopeIn  [i] = proc.scopeInput  [src];
            scopeOut [i] = proc.scopeOutput [src];
        }
    }

    particles.update (dt, getLocalBounds().toFloat());
    repaint();
}

// ── paint ──────────────────────────────────────────────────────────────────────
void AureoleEditor::paint (juce::Graphics& g)
{
    paintBackground (g);
    paintHalo (g);
    paintSectionDivider (g, kHeaderH + kHaloH + 4.0f);

    // macro section header
    g.setFont (labelFont().italicised());
    g.setColour (AC::parchment().withAlpha (0.28f));
    g.drawText ("Macros", 0, (int)(kHeaderH + kHaloH + 6.0f), kW, 14,
                juce::Justification::centred);

    paintFooter (g);
}

void AureoleEditor::paintOverChildren (juce::Graphics& g)
{
    // petal overlays sit on top of macro knobs
    auto drawPetals = [&] (juce::Slider& knob, float phase)
    {
        float val = (float)knob.getValue();
        if (val < 0.01f) return;
        laf.drawMacroPetals (g, knob.getBoundsInParent().toFloat(), val, phase);
    };
    drawPetals (macro1Knob, macro1Phase);
    drawPetals (macro2Knob, macro2Phase);

    // firefly particles float above everything
    particles.draw (g, getLocalBounds().toFloat());
}

void AureoleEditor::paintBackground (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // deep forest base
    g.setColour (AC::deepForest());
    g.fillRect (bounds);

    // warm amber gradient — Aureole is golden, not green
    {
        juce::ColourGradient grad (AC::deepForest(), bounds.getCentreX(), bounds.getY(),
                                    AC::deepEarth().withAlpha (0.45f), bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRect (bounds);
    }

    // bark grain texture (deterministic seeded)
    g.setColour (AC::barkBrown().withAlpha (0.022f));
    {
        juce::Random texRng (99);
        for (int i = 0; i < 220; ++i)
        {
            float x0 = texRng.nextFloat() * (float)kW;
            float y0 = texRng.nextFloat() * (float)kH;
            float cx = x0 + (texRng.nextFloat() - 0.5f) * 38.0f;
            float cy = y0 + (texRng.nextFloat() - 0.5f) * 38.0f;
            float x1 = cx + (texRng.nextFloat() - 0.5f) * 58.0f;
            float y1 = cy + (texRng.nextFloat() - 0.5f) * 48.0f;
            juce::Path grain;
            grain.startNewSubPath (x0, y0);
            grain.quadraticTo (cx, cy, x1, y1);
            g.strokePath (grain, juce::PathStrokeType (0.32f));
        }
    }

    // ambient golden glow sources — warm amber + golden hour, no lavender
    auto drawGlow = [&] (float cx, float cy, float r, juce::Colour col, float alpha)
    {
        float breathMod = 0.85f + 0.15f * std::sin (breathPhase);
        float gx = cx + lightDriftX;
        float gy = cy + lightDriftY;
        juce::ColourGradient glow (col.withAlpha (alpha * breathMod), gx, gy,
                                    col.withAlpha (0.0f), gx + r, gy, true);
        g.setGradientFill (glow);
        g.fillEllipse (gx - r, gy - r, r * 2.0f, r * 2.0f);
    };

    drawGlow ((float)kW * 0.18f, (float)kH * 0.12f, 240.0f, AC::warmAmber(),  0.12f);
    drawGlow ((float)kW * 0.78f, (float)kH * 0.70f, 190.0f, AC::goldenHour(), 0.08f);
    drawGlow ((float)kW * 0.50f, (float)kH * 0.40f, 150.0f, AC::warmAmber(),  0.05f);

    // header panel
    {
        juce::ColourGradient headerGrad (AC::deepEarth().withAlpha (0.60f), 0, 0,
                                          AC::deepForest().withAlpha (0.0f), 0, kHeaderH, false);
        g.setGradientFill (headerGrad);
        g.fillRect (0.0f, 0.0f, (float)kW, kHeaderH);
    }
    g.setColour (AC::barkBrown().withAlpha (0.38f));
    g.drawLine (0.0f, kHeaderH, (float)kW, kHeaderH, 1.0f);

    // title
    g.setFont (titleFont());
    g.setColour (AC::parchment());
    g.drawText ("Aureole", 18, 8, 160, 28, juce::Justification::centredLeft);

    g.setFont (labelFont().italicised());
    g.setColour (AC::warmAmber().withAlpha (0.55f));
    g.drawText ("chorus / ensemble", 18, 34, 180, 16, juce::Justification::centredLeft);
}

// ── Halo display ──────────────────────────────────────────────────────────────
void AureoleEditor::paintHalo (juce::Graphics& g)
{
    float gx = 16.0f, gy = kHeaderH + 8.0f;
    float gw = (float)kW - 32.0f, gh = kHaloH - 16.0f;
    auto area = juce::Rectangle<float> (gx, gy, gw, gh);

    // panel background
    g.setColour (AC::warmAmber().withAlpha (0.06f));
    g.fillRoundedRectangle (area, 10.0f);

    // organic border
    {
        juce::Path border;
        border.addRoundedRectangle (area.reduced (0.5f), 10.0f);
        g.setColour (AC::barkBrown().withAlpha (0.50f));
        g.strokePath (border, juce::PathStrokeType (1.2f));
        g.setColour (AC::warmAmber().withAlpha (0.06f));
        g.strokePath (border, juce::PathStrokeType (3.5f));
    }

    g.saveState();
    g.reduceClipRegion (area.toNearestInt());

    // ── waveform display (behind halo) ────────────────────────────────────
    float centY = gy + gh * 0.5f;
    if (isIdle)
    {
        // breathing idle vine in amber
        float amp    = 8.0f + 5.0f * std::sin (idleBreath * 2.0f);
        float period = gw / 2.2f;

        juce::Path vine;
        for (int px = 0; px <= (int)gw; px += 2)
        {
            float t  = (float)px / gw;
            float vx = gx + (float)px + std::sin (t * 14.0f + animTime * 0.35f) * 1.2f;
            float vy = centY + amp * std::sin (t * juce::MathConstants<float>::twoPi / period
                           + animTime * 0.18f);
            if (px == 0) vine.startNewSubPath (vx, vy);
            else         vine.lineTo (vx, vy);
        }
        g.setColour (AC::warmAmber().withAlpha (0.15f));
        g.strokePath (vine, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        g.setColour (AC::goldenHour().withAlpha (0.40f));
        g.strokePath (vine, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }
    else
    {
        constexpr int kPts  = AureoleProcessor::kScopeSize / 2;
        float         yScale = gh * 0.35f;

        // input waveform — thin amber ghost
        auto inPath = buildVinePath (scopeIn, kPts, area, yScale);
        g.setColour (AC::warmAmber().withAlpha (0.20f));
        g.strokePath (inPath, juce::PathStrokeType (1.1f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));

        // output waveform — golden with warm amber ghost
        auto outPath = buildVinePath (scopeOut, kPts, area, yScale);
        g.setColour (AC::warmAmber().withAlpha (0.12f));
        g.strokePath (outPath, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        g.setColour (AC::goldenHour().withAlpha (0.80f));
        g.strokePath (outPath, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
    }

    // ── Halo orbiting voice points ────────────────────────────────────────
    {
        auto centre = area.getCentre();
        // orbit radius: base 50px, pulses with signal amplitude
        float breathMod   = 0.92f + 0.08f * std::sin (breathPhase * 0.7f);
        float ampPulse    = signalAmplitude * 22.0f;
        float orbitRadius = (50.0f + ampPulse) * breathMod;

        // thin orbit path
        g.setColour (AC::warmAmber().withAlpha (0.15f));
        g.drawEllipse (centre.x - orbitRadius, centre.y - orbitRadius,
                       orbitRadius * 2.0f, orbitRadius * 2.0f, 1.0f);

        // connect adjacent voices with faint lines
        juce::Point<float> voicePos[4];
        for (int v = 0; v < 4; ++v)
        {
            float angle = lfoPhases[v] - juce::MathConstants<float>::halfPi; // 0 = top
            voicePos[v] = { centre.x + orbitRadius * std::cos (angle),
                            centre.y + orbitRadius * std::sin (angle) };
        }

        // connection lines between adjacent voice pairs (0-1, 1-2, 2-3, 3-0)
        g.setColour (AC::warmAmber().withAlpha (0.10f));
        for (int v = 0; v < 4; ++v)
        {
            int next = (v + 1) % 4;
            g.drawLine (voicePos[v].x, voicePos[v].y, voicePos[next].x, voicePos[next].y, 0.8f);
        }

        // orbiting glowing dot per voice
        for (int v = 0; v < 4; ++v)
        {
            float dotX = voicePos[v].x;
            float dotY = voicePos[v].y;
            float glowR = 15.0f;

            juce::ColourGradient dotGlow (AC::goldenHour().withAlpha (0.55f), dotX, dotY,
                                           AC::goldenHour().withAlpha (0.0f),  dotX + glowR, dotY, true);
            g.setGradientFill (dotGlow);
            g.fillEllipse (dotX - glowR, dotY - glowR, glowR * 2.0f, glowR * 2.0f);

            // bright dot core
            g.setColour (AC::goldenHour());
            g.fillEllipse (dotX - 4.0f, dotY - 4.0f, 8.0f, 8.0f);

            // parchment specular centre
            g.setColour (AC::parchment().withAlpha (0.6f));
            g.fillEllipse (dotX - 1.8f, dotY - 1.8f, 3.6f, 3.6f);
        }
    }

    // label — top-left, italic Georgia
    g.setFont (juce::Font (juce::FontOptions ("Georgia", 11.0f, juce::Font::italic)));
    g.setColour (AC::parchment().withAlpha (0.28f));
    g.drawText ("Halo", (int)gx + 8, (int)gy + 4, 80, 16,
                juce::Justification::centredLeft);

    g.restoreState();
}

juce::Path AureoleEditor::buildVinePath (const float* samples, int count,
                                          juce::Rectangle<float> area, float yScale) const
{
    juce::Path path;
    float stepX = area.getWidth() / (float)(count - 1);
    float centY = area.getCentreY();

    for (int i = 0; i < count; ++i)
    {
        float t  = (float)i / (float)count;
        float vx = area.getX() + (float)i * stepX
                   + std::sin (t * 16.0f + animTime * 0.55f) * 1.0f;
        float vy = centY - juce::jlimit (-1.0f, 1.0f, samples[i]) * yScale;

        if (i == 0) path.startNewSubPath (vx, vy);
        else        path.lineTo (vx, vy);
    }
    return path;
}

void AureoleEditor::paintSectionDivider (juce::Graphics& g, float y)
{
    for (int i = 0; i < 9; ++i)
    {
        float x  = (float)kW * (0.12f + (float)i * 0.095f);
        float sz = (i % 2 == 0) ? 2.5f : 1.8f;
        g.setColour (AC::barkBrown().withAlpha (0.40f));
        g.fillEllipse (x - sz * 0.5f, y - sz * 0.5f, sz, sz);
    }
}

void AureoleEditor::paintFooter (juce::Graphics& g)
{
    float fy = (float)kH - kFooterH;
    g.setColour (AC::deepEarth().withAlpha (0.65f));
    g.fillRect (0.0f, fy, (float)kW, kFooterH);
    g.setColour (AC::barkBrown().withAlpha (0.35f));
    g.drawLine (0.0f, fy, (float)kW, fy, 0.8f);

    g.setFont (labelFont().italicised());
    g.setColour (AC::parchment().withAlpha (0.28f));
    g.drawText ("Anthesis  \xe2\x80\x94  Aureole  v1.0",
                16, (int)fy + 4, kW - 32, (int)kFooterH - 8,
                juce::Justification::centredLeft);
    g.drawText ("4-voice stereo chorus  \xc2\xb7  firefly ensemble  \xc2\xb7  warm amber",
                0, (int)fy + 4, kW - 16, (int)kFooterH - 8,
                juce::Justification::centredRight);
}

void AureoleEditor::resized()
{
    presetBox.setBounds (kW - 210, 14, 196, 28);

    float macroY  = kHeaderH + kHaloH + 22.0f;
    float macro1X = (float)kW * 0.5f - kMacroSize - 20.0f;
    float macro2X = (float)kW * 0.5f + 20.0f;

    macro1Knob.setBounds ((int)macro1X, (int)macroY, (int)kMacroSize, (int)kMacroSize);
    macro2Knob.setBounds ((int)macro2X, (int)macroY, (int)kMacroSize, (int)kMacroSize);
    macro1Label.setBounds ((int)macro1X, (int)(macroY + kMacroSize), (int)kMacroSize, 16);
    macro2Label.setBounds ((int)macro2X, (int)(macroY + kMacroSize), (int)kMacroSize, 16);

    float knobY   = macroY + kMacroH + 2.0f;
    float spacing = (float)kW / 6.0f;

    juce::Slider* knobs[]  = { &rateKnob, &depthKnob, &spreadKnob, &warmthKnob, &mixKnob, &natureKnob };
    juce::Label*  labels[] = { &rateLabel, &depthLabel, &spreadLabel, &warmthLabel, &mixLabel, &natureLabel };

    for (int i = 0; i < 6; ++i)
    {
        float cx = spacing * (0.5f + (float)i);
        knobs[i]->setBounds  ((int)(cx - kKnobSize * 0.5f), (int)knobY,
                               (int)kKnobSize, (int)kKnobSize);
        labels[i]->setBounds ((int)(cx - kKnobSize * 0.5f), (int)(knobY + kKnobSize),
                               (int)kKnobSize, 16);
    }
}
