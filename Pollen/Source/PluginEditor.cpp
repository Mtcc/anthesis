#include "PluginEditor.h"

namespace
{
    constexpr float kHeaderH   = 58.0f;
    constexpr float kCloudH    = 160.0f;  // pollen cloud display height
    constexpr float kMacroH    = 110.0f;
    constexpr float kFooterH   = 42.0f;
    constexpr float kKnobSize  = 62.0f;
    constexpr float kMacroSize = 86.0f;
    constexpr float kTimerHz   = 25.0f;

    juce::Font titleFont() { return juce::Font (juce::FontOptions ("Georgia", 20.0f, juce::Font::plain)); }
    juce::Font labelFont() { return juce::Font (juce::FontOptions ("Georgia", 11.0f, juce::Font::plain)); }
}

PollenEditor::PollenEditor (PollenProcessor& p)
    : AudioProcessorEditor (p), proc (p), particles (rng)
{
    setSize (kW, kH);
    setLookAndFeel (&laf);

    auto configKnob = [&] (juce::Slider& s, juce::Label& l, const juce::String& name)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 14);
        s.setColour (juce::Slider::textBoxTextColourId,       AC::goldenHour());
        s.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (s);

        l.setText (name, juce::dontSendNotification);
        l.setFont (labelFont());
        l.setColour (juce::Label::textColourId, AC::parchment().withAlpha (0.75f));
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };

    configKnob (scatterKnob,   scatterLabel,   "Scatter");
    configKnob (floatKnob,     floatLabel,     "Float");
    configKnob (grainSizeKnob, grainSizeLabel, "Size");
    configKnob (densityKnob,   densityLabel,   "Density");
    configKnob (spreadKnob,    spreadLabel,    "Spread");
    configKnob (mixKnob,       mixLabel,       "Mix");
    configKnob (macro1Knob,    macro1Label,    "Scatter");
    configKnob (macro2Knob,    macro2Label,    "Float");
    configKnob (natureKnob,    natureLabel,    "Nature");
    natureAttach = std::make_unique<Attachment> (proc.apvts, "nature", natureKnob);

    scatterAttach   = std::make_unique<Attachment> (proc.apvts, "scatter",    scatterKnob);
    floatAttach     = std::make_unique<Attachment> (proc.apvts, "grain_float", floatKnob);
    grainSizeAttach = std::make_unique<Attachment> (proc.apvts, "grain_size", grainSizeKnob);
    densityAttach   = std::make_unique<Attachment> (proc.apvts, "density",    densityKnob);
    spreadAttach    = std::make_unique<Attachment> (proc.apvts, "spread",     spreadKnob);
    mixAttach       = std::make_unique<Attachment> (proc.apvts, "mix",        mixKnob);
    macro1Attach    = std::make_unique<Attachment> (proc.apvts, "macro1",     macro1Knob);
    macro2Attach    = std::make_unique<Attachment> (proc.apvts, "macro2",     macro2Knob);

    // presets
    presetBox.addItem ("Morning Cloud",        1);
    presetBox.addItem ("Grouper",              2);
    presetBox.addItem ("Julianna Barwick",     3);
    presetBox.addItem ("Huerco S.",            4);
    presetBox.addItem ("Kaitlyn Aurelia Smith",5);
    presetBox.addItem ("Dispersal",            6);
    presetBox.setSelectedId (1, juce::dontSendNotification);

    presetBox.onChange = [this]
    {
        struct P { float scatter, floatAmt, grainSize, density, spread, mix; };
        static const P ps[] = {
            { 0.15f, 0.10f, 0.50f, 0.70f, 0.50f, 0.50f }, // Morning Cloud
            { 0.25f, 0.30f, 0.50f, 0.60f, 0.50f, 0.60f }, // Grouper
            { 0.20f, 0.15f, 0.50f, 0.80f, 0.50f, 0.65f }, // Julianna Barwick
            { 0.50f, 0.40f, 0.50f, 0.60f, 0.50f, 0.55f }, // Huerco S.
            { 0.35f, 0.60f, 0.50f, 0.60f, 0.60f, 0.60f }, // Kaitlyn Aurelia Smith
            { 0.80f, 0.70f, 0.60f, 0.60f, 0.70f, 0.50f }, // Dispersal
        };
        int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx < 6)
        {
            proc.apvts.getParameter ("scatter")   ->setValueNotifyingHost (ps[idx].scatter);
            proc.apvts.getParameter ("grain_float")->setValueNotifyingHost (ps[idx].floatAmt);
            proc.apvts.getParameter ("grain_size")->setValueNotifyingHost (ps[idx].grainSize);
            proc.apvts.getParameter ("density")   ->setValueNotifyingHost (ps[idx].density);
            proc.apvts.getParameter ("spread")    ->setValueNotifyingHost (ps[idx].spread);
            proc.apvts.getParameter ("mix")       ->setValueNotifyingHost (ps[idx].mix);
        }
    };
    addAndMakeVisible (presetBox);

    // seed idle dots with spread-out starting positions
    for (int i = 0; i < 4; ++i)
    {
        idleDots[i].x     = 16.0f + rng.nextFloat() * ((float) kW - 32.0f);
        idleDots[i].y     = kHeaderH + 8.0f + rng.nextFloat() * (kCloudH - 16.0f);
        idleDots[i].vx    = 8.0f + rng.nextFloat() * 12.0f;
        idleDots[i].vy    = 0.0f;
        idleDots[i].env   = 0.3f + rng.nextFloat() * 0.4f;
        idleDots[i].phase = rng.nextFloat() * juce::MathConstants<float>::twoPi;
    }

    particles.reset (getLocalBounds().toFloat());
    startTimerHz ((int) kTimerHz);
}

PollenEditor::~PollenEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

// ── timer ─────────────────────────────────────────────────────────────────────
void PollenEditor::timerCallback()
{
    constexpr float dt = 1.0f / kTimerHz;
    animTime    += dt;
    breathPhase += dt * 0.18f;
    idleBreath  += dt * 0.10f;

    lightDriftX = std::sin (animTime * 0.019f) * 28.0f;
    lightDriftY = std::sin (animTime * 0.013f) * 18.0f;

    macro1Phase += dt * 0.08f;
    macro2Phase += dt * 0.06f;

    // idle detection
    float maxSig = 0.0f;
    for (int i = 0; i < PollenProcessor::kScopeSize; ++i)
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

    // copy grain display snapshot from processor
    for (int i = 0; i < 16; ++i)
        grainSnap[i] = proc.grainDisplay[i];

    // update idle dots
    if (isIdle)
    {
        const float cloudTop    = kHeaderH + 8.0f;
        const float cloudBottom = kHeaderH + kCloudH - 8.0f;
        const float cloudLeft   = 16.0f;
        const float cloudRight  = (float) kW - 16.0f;

        for (auto& dot : idleDots)
        {
            dot.phase += dt * 0.9f;
            dot.x     += dot.vx * dt;
            dot.y     += std::sin (dot.phase) * 3.0f * dt;
            dot.env    = 0.35f + 0.25f * std::sin (dot.phase * 1.3f);

            dot.y = juce::jlimit (cloudTop, cloudBottom, dot.y);

            if (dot.x > cloudRight + 6.0f)
            {
                dot.x  = cloudLeft - 4.0f;
                dot.y  = cloudTop + rng.nextFloat() * (cloudBottom - cloudTop);
                dot.vx = 8.0f + rng.nextFloat() * 12.0f;
            }
        }
    }

    particles.update (dt, getLocalBounds().toFloat());
    repaint();
}

// ── paint ─────────────────────────────────────────────────────────────────────
void PollenEditor::paint (juce::Graphics& g)
{
    paintBackground  (g);
    paintPollenCloud (g);
    paintSectionDivider (g, kHeaderH + kCloudH + 4.0f);

    // macro section label (behind knobs)
    g.setFont (labelFont().italicised());
    g.setColour (AC::parchment().withAlpha (0.28f));
    g.drawText ("Macros", 0, (int)(kHeaderH + kCloudH + 6.0f), kW, 14,
                juce::Justification::centred);

    paintFooter (g);
}

void PollenEditor::paintOverChildren (juce::Graphics& g)
{
    // petal overlays sit on top of macro knobs
    auto drawPetals = [&] (juce::Slider& knob, float phase)
    {
        float val = (float) knob.getValue();
        if (val < 0.01f) return;
        laf.drawMacroPetals (g, knob.getBoundsInParent().toFloat(), val, phase);
    };
    drawPetals (macro1Knob, macro1Phase);
    drawPetals (macro2Knob, macro2Phase);

    // pollen drift particles float above everything
    particles.draw (g, getLocalBounds().toFloat());
}

void PollenEditor::paintBackground (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // base — very dark, slightly warm
    g.setColour (AC::deepForest());
    g.fillRect (bounds);

    // warm amber glow from top-left (spring morning light)
    {
        juce::ColourGradient grad (AC::goldenHour().withAlpha (0.09f),
                                    0.0f, 0.0f,
                                    AC::deepForest().withAlpha (0.0f),
                                    (float) kW * 0.55f, (float) kH * 0.5f, false);
        g.setGradientFill (grad);
        g.fillRect (bounds);
    }

    // light green ambient from bottom-right
    {
        juce::ColourGradient grad (AC::luminousGreen().withAlpha (0.06f),
                                    (float) kW, (float) kH,
                                    AC::deepForest().withAlpha (0.0f),
                                    (float) kW * 0.45f, (float) kH * 0.45f, false);
        g.setGradientFill (grad);
        g.fillRect (bounds);
    }

    // fine grain texture — airier than Resin
    g.setColour (AC::luminousGreen().withAlpha (0.014f));
    {
        juce::Random texRng (99);
        for (int i = 0; i < 180; ++i)
        {
            float x0 = texRng.nextFloat() * (float) kW;
            float y0 = texRng.nextFloat() * (float) kH;
            float cx = x0 + (texRng.nextFloat() - 0.5f) * 30.0f;
            float cy = y0 + (texRng.nextFloat() - 0.5f) * 30.0f;
            float x1 = cx + (texRng.nextFloat() - 0.5f) * 50.0f;
            float y1 = cy + (texRng.nextFloat() - 0.5f) * 40.0f;
            juce::Path grain;
            grain.startNewSubPath (x0, y0);
            grain.quadraticTo (cx, cy, x1, y1);
            g.strokePath (grain, juce::PathStrokeType (0.28f));
        }
    }

    // drifting ambient glow spots
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

    drawGlow ((float) kW * 0.15f, (float) kH * 0.10f, 220.0f, AC::goldenHour(),    0.15f);
    drawGlow ((float) kW * 0.82f, (float) kH * 0.72f, 175.0f, AC::luminousGreen(), 0.12f);

    // header panel
    {
        juce::ColourGradient headerGrad (AC::canopyGreen().withAlpha (0.45f), 0, 0,
                                          AC::deepForest().withAlpha (0.0f), 0, kHeaderH, false);
        g.setGradientFill (headerGrad);
        g.fillRect (0.0f, 0.0f, (float) kW, kHeaderH);
    }
    g.setColour (AC::barkBrown().withAlpha (0.35f));
    g.drawLine (0.0f, kHeaderH, (float) kW, kHeaderH, 1.0f);

    // title
    g.setFont (titleFont());
    g.setColour (AC::parchment());
    g.drawText ("Pollen", 18, 8, 140, 28, juce::Justification::centredLeft);

    g.setFont (labelFont().italicised());
    g.setColour (AC::parchment().withAlpha (0.40f));
    g.drawText ("soft granular processor", 18, 34, 200, 16, juce::Justification::centredLeft);
}

void PollenEditor::paintPollenCloud (juce::Graphics& g)
{
    float gx = 16.0f, gy = kHeaderH + 8.0f;
    float gw = (float) kW - 32.0f, gh = kCloudH - 16.0f;
    auto area = juce::Rectangle<float> (gx, gy, gw, gh);

    // panel background
    g.setColour (AC::canopyGreen().withAlpha (0.14f));
    g.fillRoundedRectangle (area, 10.0f);

    // organic border
    {
        juce::Path border;
        border.addRoundedRectangle (area.reduced (0.5f), 10.0f);
        g.setColour (AC::barkBrown().withAlpha (0.45f));
        g.strokePath (border, juce::PathStrokeType (1.2f));
        g.setColour (AC::luminousGreen().withAlpha (0.06f));
        g.strokePath (border, juce::PathStrokeType (3.0f));
    }

    g.saveState();
    g.reduceClipRegion (area.toNearestInt());

    if (isIdle)
    {
        // idle state: 4 softly drifting pollen dots
        for (auto& dot : idleDots)
        {
            float dotR = 3.0f + dot.env * 4.0f;

            // soft glow
            juce::ColourGradient glow (AC::luminousGreen().withAlpha (dot.env * 0.50f),
                                        dot.x, dot.y,
                                        AC::luminousGreen().withAlpha (0.0f),
                                        dot.x + dotR * 2.8f, dot.y, true);
            g.setGradientFill (glow);
            g.fillEllipse (dot.x - dotR * 2.8f, dot.y - dotR * 2.8f,
                           dotR * 5.6f, dotR * 5.6f);

            // core dot
            g.setColour (AC::luminousGreen().withAlpha (dot.env * 0.75f));
            g.fillEllipse (dot.x - dotR, dot.y - dotR, dotR * 2.0f, dotR * 2.0f);
        }
    }
    else
    {
        // active: draw up to 16 grain dots
        for (int gi = 0; gi < 16; ++gi)
        {
            auto& gd = grainSnap[gi];
            if (!gd.active || gd.env < 0.01f) continue;

            float dotX = gx + gd.xNorm * gw;
            float dotY = gy + gh * 0.5f + (gd.yNorm - 0.5f) * gh * 0.7f;
            float dotR = gd.env * 6.0f;  // grows/shrinks with envelope

            // soft radial glow
            juce::ColourGradient glow (AC::luminousGreen().withAlpha (gd.env * 0.50f),
                                        dotX, dotY,
                                        AC::luminousGreen().withAlpha (0.0f),
                                        dotX + dotR * 3.0f, dotY, true);
            g.setGradientFill (glow);
            g.fillEllipse (dotX - dotR * 3.0f, dotY - dotR * 3.0f,
                           dotR * 6.0f, dotR * 6.0f);

            // bright core
            g.setColour (AC::luminousGreen().withAlpha (gd.env * 0.80f));
            g.fillEllipse (dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);

            // golden highlight centre
            g.setColour (AC::goldenHour().withAlpha (gd.env * 0.55f));
            g.fillEllipse (dotX - dotR * 0.4f, dotY - dotR * 0.4f,
                           dotR * 0.8f, dotR * 0.8f);
        }
    }

    // label — italic Georgia parchment
    g.setFont (juce::Font (juce::FontOptions ("Georgia", 11.0f, juce::Font::italic)));
    g.setColour (AC::parchment().withAlpha (0.28f));
    g.drawText ("Pollen Cloud", (int) gx + 8, (int) gy + 4, 100, 16,
                juce::Justification::centredLeft);

    g.restoreState();
}

void PollenEditor::paintSectionDivider (juce::Graphics& g, float y)
{
    for (int i = 0; i < 9; ++i)
    {
        float x  = (float) kW * (0.12f + (float) i * 0.095f);
        float sz = (i % 2 == 0) ? 2.5f : 1.8f;
        g.setColour (AC::barkBrown().withAlpha (0.38f));
        g.fillEllipse (x - sz * 0.5f, y - sz * 0.5f, sz, sz);
    }
}

void PollenEditor::paintFooter (juce::Graphics& g)
{
    float fy = (float) kH - kFooterH;
    g.setColour (AC::deepEarth().withAlpha (0.65f));
    g.fillRect (0.0f, fy, (float) kW, kFooterH);
    g.setColour (AC::barkBrown().withAlpha (0.30f));
    g.drawLine (0.0f, fy, (float) kW, fy, 0.8f);

    g.setFont (labelFont().italicised());
    g.setColour (AC::parchment().withAlpha (0.28f));
    g.drawText ("Anthesis  \xe2\x80\x94  Pollen  v1.0",
                16, (int) fy + 4, kW - 32, (int) kFooterH - 8,
                juce::Justification::centredLeft);
    g.drawText ("granular cloud  \xc2\xb7  raised-cosine envelope  \xc2\xb7  scatter",
                0, (int) fy + 4, kW - 16, (int) kFooterH - 8,
                juce::Justification::centredRight);
}

void PollenEditor::resized()
{
    presetBox.setBounds (kW - 210, 14, 196, 28);

    float macroY  = kHeaderH + kCloudH + 22.0f;
    float macro1X = (float) kW * 0.5f - kMacroSize - 20.0f;
    float macro2X = (float) kW * 0.5f + 20.0f;

    macro1Knob.setBounds  ((int) macro1X, (int) macroY, (int) kMacroSize, (int) kMacroSize);
    macro2Knob.setBounds  ((int) macro2X, (int) macroY, (int) kMacroSize, (int) kMacroSize);
    macro1Label.setBounds ((int) macro1X, (int)(macroY + kMacroSize), (int) kMacroSize, 16);
    macro2Label.setBounds ((int) macro2X, (int)(macroY + kMacroSize), (int) kMacroSize, 16);

    float knobY   = macroY + kMacroH + 2.0f;
    float spacing = (float) kW / 7.0f;

    juce::Slider* knobs[]  = { &scatterKnob, &floatKnob, &grainSizeKnob,
                                &densityKnob, &spreadKnob, &mixKnob, &natureKnob };
    juce::Label*  labels[] = { &scatterLabel, &floatLabel, &grainSizeLabel,
                                &densityLabel, &spreadLabel, &mixLabel, &natureLabel };

    for (int i = 0; i < 7; ++i)
    {
        float cx = spacing * (0.5f + (float) i);
        knobs[i]->setBounds  ((int)(cx - kKnobSize * 0.5f), (int) knobY,
                               (int) kKnobSize, (int) kKnobSize);
        labels[i]->setBounds ((int)(cx - kKnobSize * 0.5f), (int)(knobY + kKnobSize),
                               (int) kKnobSize, 16);
    }
}
