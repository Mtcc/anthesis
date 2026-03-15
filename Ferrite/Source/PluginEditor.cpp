#include "PluginEditor.h"

namespace
{
    constexpr float kHeaderH   = 58.0f;
    constexpr float kGardenH   = 160.0f;
    constexpr float kMacroH    = 110.0f;
    constexpr float kFooterH   = 42.0f;
    constexpr float kKnobSize  = 62.0f;
    constexpr float kMacroSize = 86.0f;
    constexpr float kTimerHz   = 25.0f;

    juce::Font titleFont() { return juce::Font (juce::FontOptions ("Georgia", 20.0f, juce::Font::plain)); }
    juce::Font labelFont() { return juce::Font (juce::FontOptions ("Georgia", 11.0f, juce::Font::plain)); }
}

FerriteEditor::FerriteEditor (FerriteProcessor& p)
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

    configKnob (driveKnob,  driveLabel,  "Drive");
    configKnob (ageKnob,    ageLabel,    "Age");
    configKnob (toneKnob,   toneLabel,   "Tone");
    configKnob (outputKnob, outputLabel, "Output");
    configKnob (mixKnob,    mixLabel,    "Mix");
    configKnob (macro1Knob, macro1Label, "Flow");
    configKnob (macro2Knob, macro2Label, "Age \xe2\x98\x86");  // Age ☆

    driveAttach  = std::make_unique<Attachment> (proc.apvts, "drive",  driveKnob);
    ageAttach    = std::make_unique<Attachment> (proc.apvts, "age",    ageKnob);
    toneAttach   = std::make_unique<Attachment> (proc.apvts, "tone",   toneKnob);
    outputAttach = std::make_unique<Attachment> (proc.apvts, "output", outputKnob);
    mixAttach    = std::make_unique<Attachment> (proc.apvts, "mix",    mixKnob);
    macro1Attach = std::make_unique<Attachment> (proc.apvts, "macro1", macro1Knob);
    macro2Attach = std::make_unique<Attachment> (proc.apvts, "macro2", macro2Knob);

    // preset dropdown
    presetBox.addItem ("Fresh Ferrite",       1);
    presetBox.addItem ("Hiroshi Yoshimura", 2);
    presetBox.addItem ("Cocteau Twins",     3);
    presetBox.addItem ("Grouper",           4);
    presetBox.addItem ("Boards of Canada",  5);
    presetBox.addItem ("Aged Circuit",      6);
    presetBox.setSelectedId (1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        struct P { float drive, age, tone, output, mix; };
        static const P ps[] = {
            { 0.20f, 0.10f, 0.90f, 0.70f, 1.00f },
            { 0.35f, 0.30f, 0.75f, 0.68f, 0.85f },
            { 0.45f, 0.20f, 0.85f, 0.72f, 0.90f },
            { 0.60f, 0.70f, 0.40f, 0.65f, 0.75f },
            { 0.50f, 0.65f, 0.50f, 0.68f, 0.80f },
            { 0.80f, 0.90f, 0.35f, 0.60f, 0.70f },
        };
        int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx < 6)
        {
            proc.apvts.getParameter ("drive") ->setValueNotifyingHost (ps[idx].drive);
            proc.apvts.getParameter ("age")   ->setValueNotifyingHost (ps[idx].age);
            proc.apvts.getParameter ("tone")  ->setValueNotifyingHost (ps[idx].tone);
            proc.apvts.getParameter ("output")->setValueNotifyingHost (ps[idx].output);
            proc.apvts.getParameter ("mix")   ->setValueNotifyingHost (ps[idx].mix);
        }
    };
    addAndMakeVisible (presetBox);

    particles.reset (getLocalBounds().toFloat());
    startTimerHz ((int) kTimerHz);
}

FerriteEditor::~FerriteEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

// ── timer ─────────────────────────────────────────────────────────────────────
void FerriteEditor::timerCallback()
{
    constexpr float dt = 1.0f / kTimerHz;
    animTime    += dt;
    breathPhase += dt * 0.18f;
    idleBreath  += dt * 0.10f;

    lightDriftX = std::sin (animTime * 0.021f) * 22.0f;
    lightDriftY = std::sin (animTime * 0.013f) * 16.0f;

    macro1Phase += dt * 0.08f;
    macro2Phase += dt * 0.06f;

    // idle detection
    float maxSig = 0.0f;
    for (int i = 0; i < FerriteProcessor::kScopeSize; ++i)
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

    // copy scope data
    if (!isIdle)
    {
        int wPos = proc.scopeWritePos.load (std::memory_order_relaxed);
        for (int i = 0; i < FerriteProcessor::kScopeSize; ++i)
        {
            int src = (wPos - FerriteProcessor::kScopeSize + i + FerriteProcessor::kScopeSize)
                      & (FerriteProcessor::kScopeSize - 1);
            scopeIn  [i] = proc.scopeInput  [src];
            scopeOut [i] = proc.scopeOutput [src];
        }
    }

    particles.update (dt, getLocalBounds().toFloat());
    repaint();
}

// ── paint ─────────────────────────────────────────────────────────────────────
// paint() runs BEFORE child components; paintOverChildren() runs AFTER.
void FerriteEditor::paint (juce::Graphics& g)
{
    paintBackground  (g);
    paintAmberGarden (g);
    paintSectionDivider (g, kHeaderH + kGardenH + 4.0f);

    // macro section header (behind knobs is fine)
    g.setFont (labelFont().italicised());
    g.setColour (AC::parchment().withAlpha (0.28f));
    g.drawText ("Macros", 0, (int)(kHeaderH + kGardenH + 6.0f), kW, 14,
                juce::Justification::centred);

    paintFooter (g);
}

void FerriteEditor::paintOverChildren (juce::Graphics& g)
{
    // petal overlays sit ON TOP of macro knobs
    auto drawPetals = [&] (juce::Slider& knob, float phase)
    {
        float val = (float) knob.getValue();
        if (val < 0.01f) return;
        laf.drawMacroPetals (g, knob.getBoundsInParent().toFloat(), val, phase);
    };
    drawPetals (macro1Knob, macro1Phase);
    drawPetals (macro2Knob, macro2Phase);

    // particles float above everything
    particles.draw (g, getLocalBounds().toFloat());
}

void FerriteEditor::paintBackground (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // base
    g.setColour (AC::deepForest());
    g.fillRect (bounds);

    // moss gradient rising from bottom
    {
        juce::ColourGradient grad (AC::deepForest(), bounds.getCentreX(), bounds.getY(),
                                    AC::moss().withAlpha (0.38f), bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRect (bounds);
    }

    // bark grain texture (seeded, deterministic)
    g.setColour (AC::barkBrown().withAlpha (0.025f));
    {
        juce::Random texRng (42);
        for (int i = 0; i < 240; ++i)
        {
            float x0 = texRng.nextFloat() * (float) kW;
            float y0 = texRng.nextFloat() * (float) kH;
            float cx = x0 + (texRng.nextFloat() - 0.5f) * 40.0f;
            float cy = y0 + (texRng.nextFloat() - 0.5f) * 40.0f;
            float x1 = cx + (texRng.nextFloat() - 0.5f) * 60.0f;
            float y1 = cy + (texRng.nextFloat() - 0.5f) * 50.0f;
            juce::Path grain;
            grain.startNewSubPath (x0, y0);
            grain.quadraticTo (cx, cy, x1, y1);
            g.strokePath (grain, juce::PathStrokeType (0.35f));
        }
    }

    // ambient glow sources (drift slowly over time)
    auto drawGlow = [&] (float cx, float cy, float r, float alpha)
    {
        float breathMod = 0.85f + 0.15f * std::sin (breathPhase);
        float gx = cx + lightDriftX;
        float gy = cy + lightDriftY;
        juce::ColourGradient glow (AC::warmAmber().withAlpha (alpha * breathMod), gx, gy,
                                    AC::warmAmber().withAlpha (0.0f), gx + r, gy, true);
        g.setGradientFill (glow);
        g.fillEllipse (gx - r, gy - r, r * 2.0f, r * 2.0f);
    };

    drawGlow ((float) kW * 0.16f, (float) kH * 0.11f, 230.0f, 0.13f);
    drawGlow ((float) kW * 0.80f, (float) kH * 0.68f, 180.0f, 0.09f);

    // header panel
    {
        juce::ColourGradient headerGrad (AC::moss().withAlpha (0.50f), 0, 0,
                                          AC::deepForest().withAlpha (0.0f), 0, kHeaderH, false);
        g.setGradientFill (headerGrad);
        g.fillRect (0.0f, 0.0f, (float) kW, kHeaderH);
    }
    g.setColour (AC::barkBrown().withAlpha (0.4f));
    g.drawLine (0.0f, kHeaderH, (float) kW, kHeaderH, 1.0f);

    // title
    g.setFont (titleFont());
    g.setColour (AC::parchment());
    g.drawText ("Ferrite", 18, 8, 140, 28, juce::Justification::centredLeft);

    g.setFont (labelFont().italicised());
    g.setColour (AC::parchment().withAlpha (0.40f));
    g.drawText ("transformer saturation", 18, 34, 160, 16, juce::Justification::centredLeft);
}

void FerriteEditor::paintAmberGarden (juce::Graphics& g)
{
    float gx = 16.0f, gy = kHeaderH + 8.0f;
    float gw = (float) kW - 32.0f, gh = kGardenH - 16.0f;
    auto area = juce::Rectangle<float> (gx, gy, gw, gh);

    // panel
    g.setColour (AC::canopyGreen().withAlpha (0.20f));
    g.fillRoundedRectangle (area, 10.0f);

    // organic border
    {
        juce::Path border;
        border.addRoundedRectangle (area.reduced (0.5f), 10.0f);
        g.setColour (AC::barkBrown().withAlpha (0.55f));
        g.strokePath (border, juce::PathStrokeType (1.2f));
        g.setColour (AC::luminousGreen().withAlpha (0.05f));
        g.strokePath (border, juce::PathStrokeType (3.5f));
    }

    g.saveState();
    g.reduceClipRegion (area.toNearestInt());

    float centY = gy + gh * 0.5f;

    if (isIdle)
    {
        // breathing vine
        float amp    = 10.0f + 7.0f * std::sin (idleBreath * 2.0f);
        float period = gw / 2.2f;

        juce::Path vine;
        for (int px = 0; px <= (int) gw; px += 2)
        {
            float t    = (float) px / gw;
            float vx   = gx + (float) px + std::sin (t * 14.0f + animTime * 0.35f) * 1.4f;
            float vy   = centY + amp * std::sin (t * juce::MathConstants<float>::twoPi / period
                             + animTime * 0.20f);
            if (px == 0) vine.startNewSubPath (vx, vy);
            else         vine.lineTo (vx, vy);
        }
        g.setColour (AC::warmAmber().withAlpha (0.18f));
        g.strokePath (vine, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        g.setColour (AC::luminousGreen().withAlpha (0.50f));
        g.strokePath (vine, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        for (float t = 0.08f; t < 1.0f; t += 0.16f)
        {
            float vx  = gx + t * gw;
            float vy  = centY + amp * std::sin (t * juce::MathConstants<float>::twoPi / period
                            + animTime * 0.20f);
            float bud = 3.2f + 1.4f * std::sin (idleBreath + t * 9.0f);
            g.setColour (AC::luminousGreen().withAlpha (0.35f));
            g.fillEllipse (vx - bud, vy - bud, bud * 2.0f, bud * 2.0f);
        }
    }
    else
    {
        constexpr int kPts = FerriteProcessor::kScopeSize / 2;
        float yScale = gh * 0.40f;

        // input (thin amber)
        auto inPath = buildVinePath (scopeIn, kPts, area, yScale);
        g.setColour (AC::warmAmber().withAlpha (0.25f));
        g.strokePath (inPath, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));

        // output (thick green + amber ghost)
        auto outPath = buildVinePath (scopeOut, kPts, area, yScale);
        g.setColour (AC::warmAmber().withAlpha (0.15f));
        g.strokePath (outPath, juce::PathStrokeType (4.2f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        g.setColour (AC::luminousGreen().withAlpha (0.85f));
        g.strokePath (outPath, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
    }

    // label
    g.setFont (labelFont().italicised());
    g.setColour (AC::parchment().withAlpha (0.28f));
    g.drawText ("Iron Garden", (int) gx + 8, (int) gy + 4, 130, 16,
                juce::Justification::centredLeft);

    g.restoreState();
}

juce::Path FerriteEditor::buildVinePath (const float* samples, int count,
                                        juce::Rectangle<float> area, float yScale) const
{
    juce::Path path;
    float stepX = area.getWidth() / (float) (count - 1);
    float centY = area.getCentreY();

    for (int i = 0; i < count; ++i)
    {
        float t  = (float) i / (float) count;
        float vx = area.getX() + (float) i * stepX
                   + std::sin (t * 16.0f + animTime * 0.55f) * 1.1f;
        float vy = centY - juce::jlimit (-1.0f, 1.0f, samples[i]) * yScale;

        if (i == 0) path.startNewSubPath (vx, vy);
        else        path.lineTo (vx, vy);
    }
    return path;
}

void FerriteEditor::paintSectionDivider (juce::Graphics& g, float y)
{
    for (int i = 0; i < 9; ++i)
    {
        float x  = (float) kW * (0.12f + (float) i * 0.095f);
        float sz = (i % 2 == 0) ? 2.5f : 1.8f;
        g.setColour (AC::barkBrown().withAlpha (0.40f));
        g.fillEllipse (x - sz * 0.5f, y - sz * 0.5f, sz, sz);
    }
}

void FerriteEditor::paintFooter (juce::Graphics& g)
{
    float fy = (float) kH - kFooterH;
    g.setColour (AC::deepEarth().withAlpha (0.65f));
    g.fillRect (0.0f, fy, (float) kW, kFooterH);
    g.setColour (AC::barkBrown().withAlpha (0.35f));
    g.drawLine (0.0f, fy, (float) kW, fy, 0.8f);

    g.setFont (labelFont().italicised());
    g.setColour (AC::parchment().withAlpha (0.28f));
    g.drawText ("Anthesis  \xe2\x80\x94  Ferrite  v1.0",
                16, (int) fy + 4, kW - 32, (int) kFooterH - 8,
                juce::Justification::centredLeft);
    g.drawText ("4\xc3\x97 oversampled  \xc2\xb7  tube/transformer  \xc2\xb7  de-emphasis",
                0, (int) fy + 4, kW - 16, (int) kFooterH - 8,
                juce::Justification::centredRight);
}

void FerriteEditor::resized()
{
    presetBox.setBounds (kW - 210, 14, 196, 28);

    float macroY  = kHeaderH + kGardenH + 22.0f;
    float macro1X = (float) kW * 0.5f - kMacroSize - 20.0f;
    float macro2X = (float) kW * 0.5f + 20.0f;

    macro1Knob.setBounds ((int) macro1X, (int) macroY, (int) kMacroSize, (int) kMacroSize);
    macro2Knob.setBounds ((int) macro2X, (int) macroY, (int) kMacroSize, (int) kMacroSize);
    macro1Label.setBounds ((int) macro1X, (int)(macroY + kMacroSize),     (int) kMacroSize, 16);
    macro2Label.setBounds ((int) macro2X, (int)(macroY + kMacroSize),     (int) kMacroSize, 16);

    float knobY   = macroY + kMacroH + 2.0f;
    float spacing = (float) kW / 5.0f;

    juce::Slider* knobs[]  = { &driveKnob, &ageKnob, &toneKnob, &outputKnob, &mixKnob };
    juce::Label*  labels[] = { &driveLabel, &ageLabel, &toneLabel, &outputLabel, &mixLabel };

    for (int i = 0; i < 5; ++i)
    {
        float cx = spacing * (0.5f + (float) i);
        knobs[i]->setBounds  ((int)(cx - kKnobSize * 0.5f), (int) knobY,
                               (int) kKnobSize, (int) kKnobSize);
        labels[i]->setBounds ((int)(cx - kKnobSize * 0.5f), (int)(knobY + kKnobSize),
                               (int) kKnobSize, 16);
    }
}
