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

AlluviumEditor::AlluviumEditor (AlluviumProcessor& p)
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

    configKnob (timeKnob,     timeLabel,     "Time");
    configKnob (feedbackKnob, feedbackLabel, "Feedback");
    configKnob (sedimentKnob, sedimentLabel, "Sediment");
    configKnob (currentKnob,  currentLabel,  "Current");
    configKnob (mixKnob,      mixLabel,      "Mix");
    configKnob (natureKnob,   natureLabel,   "Nature");
    configKnob (macro1Knob,   macro1Label,   "Sediment");
    configKnob (macro2Knob,   macro2Label,   "Current");

    timeAttach     = std::make_unique<Attachment> (proc.apvts, "time",     timeKnob);
    feedbackAttach = std::make_unique<Attachment> (proc.apvts, "feedback", feedbackKnob);
    sedimentAttach = std::make_unique<Attachment> (proc.apvts, "sediment", sedimentKnob);
    currentAttach  = std::make_unique<Attachment> (proc.apvts, "current",  currentKnob);
    mixAttach      = std::make_unique<Attachment> (proc.apvts, "mix",      mixKnob);
    natureAttach   = std::make_unique<Attachment> (proc.apvts, "nature",   natureKnob);
    macro1Attach   = std::make_unique<Attachment> (proc.apvts, "macro1",   macro1Knob);
    macro2Attach   = std::make_unique<Attachment> (proc.apvts, "macro2",   macro2Knob);

    // presets
    presetBox.addItem ("Clear Stream",      1);
    presetBox.addItem ("Ryuichi Sakamoto",  2);
    presetBox.addItem ("Boards of Canada",  3);
    presetBox.addItem ("Huerco S.",         4);
    presetBox.addItem ("Grouper",           5);
    presetBox.addItem ("Deep Silt",         6);
    presetBox.setSelectedId (1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        struct P { float time, feedback, sediment, current, mix; };
        static const P ps[] = {
            { 0.50f, 0.30f, 0.10f, 0.10f, 0.35f },  // Clear Stream
            { 0.55f, 0.40f, 0.30f, 0.40f, 0.40f },  // Ryuichi Sakamoto
            { 0.60f, 0.50f, 0.55f, 0.50f, 0.45f },  // Boards of Canada
            { 0.50f, 0.45f, 0.70f, 0.60f, 0.40f },  // Huerco S.
            { 0.65f, 0.55f, 0.50f, 0.70f, 0.50f },  // Grouper
            { 0.70f, 0.60f, 0.90f, 0.80f, 0.45f },  // Deep Silt
        };
        int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx < 6)
        {
            proc.apvts.getParameter ("time")    ->setValueNotifyingHost (ps[idx].time);
            proc.apvts.getParameter ("feedback")->setValueNotifyingHost (ps[idx].feedback);
            proc.apvts.getParameter ("sediment")->setValueNotifyingHost (ps[idx].sediment);
            proc.apvts.getParameter ("current") ->setValueNotifyingHost (ps[idx].current);
            proc.apvts.getParameter ("mix")     ->setValueNotifyingHost (ps[idx].mix);
        }
    };
    addAndMakeVisible (presetBox);

    particles.reset (getLocalBounds().toFloat());
    startTimerHz ((int) kTimerHz);
}

AlluviumEditor::~AlluviumEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

// ── timer ─────────────────────────────────────────────────────────────────────
void AlluviumEditor::timerCallback()
{
    constexpr float dt = 1.0f / kTimerHz;
    animTime    += dt;
    breathPhase += dt * 0.18f;
    idleBreath  += dt * 0.10f;

    lightDriftX = std::sin (animTime * 0.019f) * 20.0f;
    lightDriftY = std::sin (animTime * 0.012f) * 14.0f;

    macro1Phase += dt * 0.07f;
    macro2Phase += dt * 0.05f;

    // idle detection
    float maxSig = 0.0f;
    for (int i = 0; i < AlluviumProcessor::kScopeSize; ++i)
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

    // copy scope snapshot
    if (!isIdle)
    {
        int wPos = proc.scopeWritePos.load (std::memory_order_relaxed);
        for (int i = 0; i < AlluviumProcessor::kScopeSize; ++i)
        {
            int src = (wPos - AlluviumProcessor::kScopeSize + i + AlluviumProcessor::kScopeSize)
                      & (AlluviumProcessor::kScopeSize - 1);
            scopeIn  [i] = proc.scopeInput  [src];
            scopeOut [i] = proc.scopeOutput [src];
        }
    }

    particles.update (dt, getLocalBounds().toFloat());
    repaint();
}

// ── paint ─────────────────────────────────────────────────────────────────────
void AlluviumEditor::paint (juce::Graphics& g)
{
    paintBackground (g);
    paintStream     (g);
    paintSectionDivider (g, kHeaderH + kGardenH + 4.0f);

    // macro section label
    g.setFont (labelFont().italicised());
    g.setColour (AC::parchment().withAlpha (0.28f));
    g.drawText ("Macros", 0, (int)(kHeaderH + kGardenH + 6.0f), kW, 14,
                juce::Justification::centred);

    paintFooter (g);
}

void AlluviumEditor::paintOverChildren (juce::Graphics& g)
{
    // petal overlays on top of macro knobs
    auto drawPetals = [&] (juce::Slider& knob, float phase)
    {
        float val = (float) knob.getValue();
        if (val < 0.01f) return;
        laf.drawMacroPetals (g, knob.getBoundsInParent().toFloat(), val, phase);
    };
    drawPetals (macro1Knob, macro1Phase);
    drawPetals (macro2Knob, macro2Phase);

    particles.draw (g, getLocalBounds().toFloat());
}

void AlluviumEditor::paintBackground (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // base: deep forest
    g.setColour (AC::deepForest());
    g.fillRect (bounds);

    // moss gradient rising from bottom
    {
        juce::ColourGradient grad (AC::deepForest(), bounds.getCentreX(), bounds.getY(),
                                    AC::moss().withAlpha (0.35f), bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRect (bounds);
    }

    // subtle lavender tint in upper corners — water/sky feel
    {
        juce::ColourGradient lavGrad (AC::lavenderMist().withAlpha (0.05f), 0, 0,
                                       AC::lavenderMist().withAlpha (0.0f), (float) kW * 0.4f, kHeaderH * 2.0f, false);
        g.setGradientFill (lavGrad);
        g.fillRect (bounds);
    }

    // sediment grain texture (deterministic)
    g.setColour (AC::barkBrown().withAlpha (0.018f));
    {
        juce::Random texRng (99);
        for (int i = 0; i < 200; ++i)
        {
            float x0 = texRng.nextFloat() * (float) kW;
            float y0 = texRng.nextFloat() * (float) kH;
            float cx = x0 + (texRng.nextFloat() - 0.5f) * 50.0f;
            float cy = y0 + (texRng.nextFloat() - 0.5f) * 30.0f;
            float x1 = cx + (texRng.nextFloat() - 0.5f) * 70.0f;
            float y1 = cy + (texRng.nextFloat() - 0.5f) * 20.0f;
            juce::Path grain;
            grain.startNewSubPath (x0, y0);
            grain.quadraticTo (cx, cy, x1, y1);
            g.strokePath (grain, juce::PathStrokeType (0.30f));
        }
    }

    // ambient light sources (drift slowly)
    auto drawGlow = [&] (float cx, float cy, float r, juce::Colour col, float alpha)
    {
        float breathMod = 0.88f + 0.12f * std::sin (breathPhase);
        float gx = cx + lightDriftX;
        float gy = cy + lightDriftY;
        juce::ColourGradient glow (col.withAlpha (alpha * breathMod), gx, gy,
                                    col.withAlpha (0.0f), gx + r, gy, true);
        g.setGradientFill (glow);
        g.fillEllipse (gx - r, gy - r, r * 2.0f, r * 2.0f);
    };

    drawGlow ((float) kW * 0.15f, (float) kH * 0.10f, 220.0f, AC::lavenderMist(), 0.10f);
    drawGlow ((float) kW * 0.82f, (float) kH * 0.70f, 170.0f, AC::moss(),         0.15f);

    // header panel with moss gradient
    {
        juce::ColourGradient headerGrad (AC::moss().withAlpha (0.45f), 0, 0,
                                          AC::deepForest().withAlpha (0.0f), 0, kHeaderH, false);
        g.setGradientFill (headerGrad);
        g.fillRect (0.0f, 0.0f, (float) kW, kHeaderH);
    }
    g.setColour (AC::barkBrown().withAlpha (0.35f));
    g.drawLine (0.0f, kHeaderH, (float) kW, kHeaderH, 1.0f);

    // title
    g.setFont (titleFont());
    g.setColour (AC::parchment());
    g.drawText ("Alluvium", 18, 8, 160, 28, juce::Justification::centredLeft);

    g.setFont (labelFont().italicised());
    g.setColour (AC::parchment().withAlpha (0.38f));
    g.drawText ("organic tape delay", 18, 34, 180, 16, juce::Justification::centredLeft);
}

void AlluviumEditor::paintStream (juce::Graphics& g)
{
    float gx = 16.0f, gy = kHeaderH + 8.0f;
    float gw = (float) kW - 32.0f, gh = kGardenH - 16.0f;
    auto  area = juce::Rectangle<float> (gx, gy, gw, gh);

    // panel
    g.setColour (AC::canopyGreen().withAlpha (0.14f));
    g.fillRoundedRectangle (area, 10.0f);

    // organic border
    {
        juce::Path border;
        border.addRoundedRectangle (area.reduced (0.5f), 10.0f);
        g.setColour (AC::barkBrown().withAlpha (0.45f));
        g.strokePath (border, juce::PathStrokeType (1.2f));
        g.setColour (AC::lavenderMist().withAlpha (0.06f));
        g.strokePath (border, juce::PathStrokeType (3.5f));
    }

    g.saveState();
    g.reduceClipRegion (area.toNearestInt());

    float centY    = gy + gh * 0.5f;
    float sediment = proc.smoothSediment;

    if (isIdle)
    {
        // gentle idle: single wavy horizontal line that degrades rightward
        juce::Path idleLine;
        constexpr int kSteps = 300;

        for (int px = 0; px <= kSteps; ++px)
        {
            float t  = (float) px / (float) kSteps;
            float vx = gx + t * gw;
            float vy = centY + 6.0f * std::sin (t * juce::MathConstants<float>::twoPi * 1.8f
                                + animTime * 0.25f)
                              + 2.0f * std::sin (t * juce::MathConstants<float>::twoPi * 4.3f
                                + animTime * 0.40f);
            // sediment degradation toward right: add y jitter
            float xNorm = t;
            float jitter = (rng.nextFloat() - 0.5f) * sediment * 8.0f * xNorm;
            vy += jitter;

            if (px == 0) idleLine.startNewSubPath (vx, vy);
            else         idleLine.lineTo (vx, vy);
        }

        float alpha = 1.0f - sediment * 0.5f;
        g.setColour (AC::luminousGreen().withAlpha (0.35f * alpha));
        g.strokePath (idleLine, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
    }
    else
    {
        constexpr int kPts   = AlluviumProcessor::kScopeSize / 2;
        float         yScale = gh * 0.40f;
        float         stepX  = gw / (float)(kPts - 1);

        // ── input ribbon (thin warm amber) ──────────────────────────────
        juce::Path inPath;
        for (int i = 0; i < kPts; ++i)
        {
            float t   = (float) i / (float) kPts;
            float vx  = gx + (float) i * stepX;
            float vy  = centY - juce::jlimit (-1.0f, 1.0f, scopeIn[i]) * yScale;

            float xNorm = t;
            float jitter = (rng.nextFloat() - 0.5f) * sediment * 8.0f * xNorm;
            vy += jitter;

            if (i == 0) inPath.startNewSubPath (vx, vy);
            else        inPath.lineTo (vx, vy);
        }
        g.setColour (AC::warmAmber().withAlpha (0.22f));
        g.strokePath (inPath, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));

        // ── output ribbon (thicker luminous green, degrading rightward) ──
        // build points with sediment degradation (jitter + crumble + alpha decay)
        {
            juce::Path outPath;
            bool       pathStarted = false;
            juce::Random crumbleRng (42 + (int)(animTime * 100.0f));

            for (int i = 0; i < kPts; ++i)
            {
                float t     = (float) i / (float) kPts;
                float xNorm = t;

                // crumble: skip 20% of points at xNorm > 0.7
                if (xNorm > 0.7f && crumbleRng.nextFloat() < 0.20f)
                {
                    if (pathStarted)
                    {
                        // draw accumulated path before gap
                        float alpha = 1.0f - sediment * xNorm * 0.6f;
                        g.setColour (AC::luminousGreen().withAlpha (juce::jlimit (0.0f, 0.88f, 0.88f * alpha)));
                        g.strokePath (outPath, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                                      juce::PathStrokeType::rounded));
                        outPath.clear();
                        pathStarted = false;
                    }
                    continue;
                }

                float vx = gx + (float) i * stepX;
                float vy = centY - juce::jlimit (-1.0f, 1.0f, scopeOut[i]) * yScale;

                float jitter = (rng.nextFloat() - 0.5f) * sediment * 8.0f * xNorm;
                vy += jitter;

                if (!pathStarted)
                {
                    outPath.startNewSubPath (vx, vy);
                    pathStarted = true;
                }
                else
                {
                    outPath.lineTo (vx, vy);
                }
            }

            if (pathStarted)
            {
                g.setColour (AC::luminousGreen().withAlpha (0.88f));
                g.strokePath (outPath, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                              juce::PathStrokeType::rounded));
            }
        }
    }

    // label
    g.setFont (labelFont().italicised());
    g.setColour (AC::parchment().withAlpha (0.28f));
    g.drawText ("Stream", (int) gx + 8, (int) gy + 4, 100, 16,
                juce::Justification::centredLeft);

    g.restoreState();
}

void AlluviumEditor::paintSectionDivider (juce::Graphics& g, float y)
{
    for (int i = 0; i < 9; ++i)
    {
        float x  = (float) kW * (0.12f + (float) i * 0.095f);
        float sz = (i % 2 == 0) ? 2.5f : 1.8f;
        // alternate lavender and bark for a watery feel
        auto col = (i % 3 == 0) ? AC::lavenderMist() : AC::barkBrown();
        g.setColour (col.withAlpha (0.38f));
        g.fillEllipse (x - sz * 0.5f, y - sz * 0.5f, sz, sz);
    }
}

void AlluviumEditor::paintFooter (juce::Graphics& g)
{
    float fy = (float) kH - kFooterH;
    g.setColour (AC::deepEarth().withAlpha (0.65f));
    g.fillRect (0.0f, fy, (float) kW, kFooterH);
    g.setColour (AC::barkBrown().withAlpha (0.32f));
    g.drawLine (0.0f, fy, (float) kW, fy, 0.8f);

    g.setFont (labelFont().italicised());
    g.setColour (AC::parchment().withAlpha (0.28f));
    g.drawText ("Anthesis  \xe2\x80\x94  Alluvium  v1.0",
                16, (int) fy + 4, kW - 32, (int) kFooterH - 8,
                juce::Justification::centredLeft);
    g.drawText ("tape delay  \xc2\xb7  wow/flutter  \xc2\xb7  organic degradation",
                0, (int) fy + 4, kW - 16, (int) kFooterH - 8,
                juce::Justification::centredRight);
}

void AlluviumEditor::resized()
{
    presetBox.setBounds (kW - 210, 14, 196, 28);

    float macroY  = kHeaderH + kGardenH + 22.0f;
    float macro1X = (float) kW * 0.5f - kMacroSize - 20.0f;
    float macro2X = (float) kW * 0.5f + 20.0f;

    macro1Knob.setBounds  ((int) macro1X, (int) macroY,              (int) kMacroSize, (int) kMacroSize);
    macro2Knob.setBounds  ((int) macro2X, (int) macroY,              (int) kMacroSize, (int) kMacroSize);
    macro1Label.setBounds ((int) macro1X, (int)(macroY + kMacroSize), (int) kMacroSize, 16);
    macro2Label.setBounds ((int) macro2X, (int)(macroY + kMacroSize), (int) kMacroSize, 16);

    float knobY   = macroY + kMacroH + 2.0f;
    float spacing = (float) kW / 6.0f;

    juce::Slider* knobs[]  = { &timeKnob, &feedbackKnob, &sedimentKnob, &currentKnob, &mixKnob, &natureKnob };
    juce::Label*  labels[] = { &timeLabel, &feedbackLabel, &sedimentLabel, &currentLabel, &mixLabel, &natureLabel };

    for (int i = 0; i < 6; ++i)
    {
        float cx = spacing * (0.5f + (float) i);
        knobs[i]->setBounds  ((int)(cx - kKnobSize * 0.5f), (int) knobY,
                               (int) kKnobSize, (int) kKnobSize);
        labels[i]->setBounds ((int)(cx - kKnobSize * 0.5f), (int)(knobY + kKnobSize),
                               (int) kKnobSize, 16);
    }
}
