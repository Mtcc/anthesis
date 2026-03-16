#include "PluginEditor.h"

namespace
{
    constexpr float kHeaderH   = 58.0f;
    constexpr float kNetworkH  = 160.0f;
    constexpr float kMacroH    = 110.0f;
    constexpr float kFooterH   = 42.0f;
    constexpr float kKnobSize  = 62.0f;
    constexpr float kMacroSize = 86.0f;
    constexpr float kTimerHz   = 25.0f;

    juce::Font titleFont() { return juce::Font (juce::FontOptions ("Georgia", 20.0f, juce::Font::plain)); }
    juce::Font labelFont() { return juce::Font (juce::FontOptions ("Georgia", 11.0f, juce::Font::plain)); }
}

MyceliumEditor::MyceliumEditor (MyceliumProcessor& p)
    : AudioProcessorEditor (p), proc (p), rng (42), particles (rng)
{
    setSize (kW, kH);
    setLookAndFeel (&laf);

    auto configKnob = [&] (juce::Slider& s, juce::Label& l, const juce::String& name)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 14);
        s.setColour (juce::Slider::textBoxTextColourId,       AC::lavenderMist());
        s.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (s);

        l.setText (name, juce::dontSendNotification);
        l.setFont (labelFont());
        l.setColour (juce::Label::textColourId, AC::parchment().withAlpha (0.75f));
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };

    configKnob (canopyKnob,   canopyLabel,   "Canopy");
    configKnob (predelayKnob, predelayLabel, "Pre-Delay");
    configKnob (decayKnob,    decayLabel,    "Decay");
    configKnob (dampingKnob,  dampingLabel,  "Damping");
    configKnob (widthKnob,    widthLabel,    "Width");
    configKnob (mixKnob,      mixLabel,      "Mix");
    configKnob (natureKnob,   natureLabel,   "Nature");
    configKnob (macro1Knob,   macro1Label,   "Canopy");
    configKnob (macro2Knob,   macro2Label,   "Root Depth");

    canopyAttach   = std::make_unique<Attachment> (proc.apvts, "canopy",   canopyKnob);
    predelayAttach = std::make_unique<Attachment> (proc.apvts, "predelay", predelayKnob);
    decayAttach    = std::make_unique<Attachment> (proc.apvts, "decay",    decayKnob);
    dampingAttach  = std::make_unique<Attachment> (proc.apvts, "damping",  dampingKnob);
    widthAttach    = std::make_unique<Attachment> (proc.apvts, "width",    widthKnob);
    mixAttach      = std::make_unique<Attachment> (proc.apvts, "mix",      mixKnob);
    natureAttach   = std::make_unique<Attachment> (proc.apvts, "nature",   natureKnob);
    macro1Attach   = std::make_unique<Attachment> (proc.apvts, "macro1",   macro1Knob);
    macro2Attach   = std::make_unique<Attachment> (proc.apvts, "macro2",   macro2Knob);

    // preset dropdown
    presetBox.addItem ("Morning Greenhouse", 1);
    presetBox.addItem ("Ichiko Aoba",        2);
    presetBox.addItem ("Hiroshi Yoshimura",  3);
    presetBox.addItem ("Julianna Barwick",   4);
    presetBox.addItem ("Sigur Ros",          5);
    presetBox.addItem ("Deep Cave",          6);
    presetBox.setSelectedId (1, juce::dontSendNotification);

    presetBox.onChange = [this]
    {
        struct P { float canopy, decay, damping, mix; };
        static const P ps[] = {
            { 0.05f, 0.40f, 0.70f, 0.30f },  // Morning Greenhouse
            { 0.30f, 0.70f, 0.60f, 0.25f },  // Ichiko Aoba
            { 0.50f, 0.85f, 0.45f, 0.35f },  // Hiroshi Yoshimura (decay mapped ~1.5 in 0-1 range clamps to 0.85)
            { 0.45f, 0.70f, 0.50f, 0.45f },  // Julianna Barwick
            { 0.70f, 0.85f, 0.35f, 0.50f },  // Sigur Ros
            { 1.00f, 0.90f, 0.20f, 0.40f },  // Deep Cave
        };
        int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx < 6)
        {
            proc.apvts.getParameter ("canopy") ->setValueNotifyingHost (ps[idx].canopy);
            proc.apvts.getParameter ("decay")  ->setValueNotifyingHost (ps[idx].decay);
            proc.apvts.getParameter ("damping")->setValueNotifyingHost (ps[idx].damping);
            proc.apvts.getParameter ("mix")    ->setValueNotifyingHost (ps[idx].mix);
        }
    };
    addAndMakeVisible (presetBox);

    particles.reset (getLocalBounds().toFloat());
    startTimerHz ((int)kTimerHz);
}

MyceliumEditor::~MyceliumEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

// ── timer ──────────────────────────────────────────────────────────────────────
void MyceliumEditor::timerCallback()
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
    for (int i = 0; i < MyceliumProcessor::kScopeSize; ++i)
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

    // smooth amplitude for branch pulse
    signalAmplitude += (1.0f / (kTimerHz * 0.15f)) * (maxSig - signalAmplitude);
    signalAmplitude  = juce::jlimit (0.0f, 1.0f, signalAmplitude);

    // copy scope data
    if (!isIdle)
    {
        int wPos = proc.scopeWritePos.load (std::memory_order_relaxed);
        for (int i = 0; i < MyceliumProcessor::kScopeSize; ++i)
        {
            int src = (wPos - MyceliumProcessor::kScopeSize + i + MyceliumProcessor::kScopeSize)
                      & (MyceliumProcessor::kScopeSize - 1);
            scopeIn  [i] = proc.scopeInput  [src];
            scopeOut [i] = proc.scopeOutput [src];
        }
    }

    particles.update (dt, getLocalBounds().toFloat());
    repaint();
}

// ── paint ─────────────────────────────────────────────────────────────────────
void MyceliumEditor::paint (juce::Graphics& g)
{
    paintBackground      (g);
    paintMyceliumNetwork (g);
    paintSectionDivider  (g, kHeaderH + kNetworkH + 4.0f);

    // macro section watermark
    g.setFont (labelFont().italicised());
    g.setColour (AC::parchment().withAlpha (0.28f));
    g.drawText ("Macros", 0, (int)(kHeaderH + kNetworkH + 6.0f), kW, 14,
                juce::Justification::centred);

    paintFooter (g);
}

void MyceliumEditor::paintOverChildren (juce::Graphics& g)
{
    // spore-arm overlays sit on top of macro knobs
    auto drawSpores = [&] (juce::Slider& knob, float phase)
    {
        float val = (float)knob.getValue();
        if (val < 0.01f) return;
        laf.drawMacroPetals (g, knob.getBoundsInParent().toFloat(), val, phase);
    };
    drawSpores (macro1Knob, macro1Phase);
    drawSpores (macro2Knob, macro2Phase);

    // particles float above everything
    particles.draw (g, getLocalBounds().toFloat());
}

void MyceliumEditor::paintBackground (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // base fill
    g.setColour (AC::deepForest());
    g.fillRect (bounds);

    // moss gradient from bottom
    {
        juce::ColourGradient grad (AC::deepForest(), bounds.getCentreX(), bounds.getY(),
                                    AC::moss().withAlpha (0.38f), bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRect (bounds);
    }

    // bark grain texture (seeded, deterministic)
    g.setColour (AC::barkBrown().withAlpha (0.022f));
    {
        juce::Random texRng (42);
        for (int i = 0; i < 240; ++i)
        {
            float x0 = texRng.nextFloat() * (float)kW;
            float y0 = texRng.nextFloat() * (float)kH;
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

    // ambient lavender + green glows (drift slowly over time)
    auto drawGlow = [&] (float cx, float cy, float r, float alpha, bool useLavender)
    {
        float breathMod = 0.85f + 0.15f * std::sin (breathPhase);
        float gx = cx + lightDriftX;
        float gy = cy + lightDriftY;
        juce::Colour col = useLavender
            ? AC::lavenderMist().interpolatedWith (AC::luminousGreen(), 0.35f)
            : AC::luminousGreen();
        juce::ColourGradient glow (col.withAlpha (alpha * breathMod), gx, gy,
                                    col.withAlpha (0.0f), gx + r, gy, true);
        g.setGradientFill (glow);
        g.fillEllipse (gx - r, gy - r, r * 2.0f, r * 2.0f);
    };

    drawGlow ((float)kW * 0.16f, (float)kH * 0.11f, 230.0f, 0.11f, true);
    drawGlow ((float)kW * 0.80f, (float)kH * 0.68f, 180.0f, 0.08f, false);

    // header panel
    {
        juce::ColourGradient headerGrad (AC::moss().withAlpha (0.50f), 0, 0,
                                          AC::deepForest().withAlpha (0.0f), 0, kHeaderH, false);
        g.setGradientFill (headerGrad);
        g.fillRect (0.0f, 0.0f, (float)kW, kHeaderH);
    }
    g.setColour (AC::barkBrown().withAlpha (0.4f));
    g.drawLine (0.0f, kHeaderH, (float)kW, kHeaderH, 1.0f);

    // title
    g.setFont (titleFont());
    g.setColour (AC::parchment());
    g.drawText ("Mycelium", 18, 8, 200, 28, juce::Justification::centredLeft);

    g.setFont (labelFont().italicised());
    g.setColour (AC::parchment().withAlpha (0.40f));
    g.drawText ("algorithmic reverb", 18, 34, 200, 16, juce::Justification::centredLeft);
}

void MyceliumEditor::paintMyceliumNetwork (juce::Graphics& g)
{
    float gx = 16.0f, gy = kHeaderH + 8.0f;
    float gw = (float)kW - 32.0f, gh = kNetworkH - 16.0f;
    auto area = juce::Rectangle<float> (gx, gy, gw, gh);

    // panel
    g.setColour (AC::canopyGreen().withAlpha (0.18f));
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

    // center point: 40% from left, vertically centered
    float cx = gx + gw * 0.40f;
    float cy = gy + gh * 0.50f;

    // read decay param for branch length scaling
    float decayVal = *proc.apvts.getRawParameterValue ("decay");

    // seeded random for deterministic branch layout (seed=42)
    juce::Random layoutRng (42);

    // branch data: angle, length, curve
    constexpr int kNumBranches = 6;
    struct BranchDef
    {
        float angle;       // radians from center
        float length;      // base length
        float curveBias;   // how much control point deviates
    };

    BranchDef branches[kNumBranches];
    for (int b = 0; b < kNumBranches; ++b)
    {
        branches[b].angle     = layoutRng.nextFloat() * juce::MathConstants<float>::twoPi;
        branches[b].length    = 60.0f + layoutRng.nextFloat() * 80.0f;
        branches[b].curveBias = 20.0f + layoutRng.nextFloat() * 30.0f;
    }

    float branchThickness = 1.5f;
    if (!isIdle)
        branchThickness = 1.5f + signalAmplitude * 2.5f;

    for (int b = 0; b < kNumBranches; ++b)
    {
        auto& br = branches[b];

        // animate endpoint with slow sine offset
        float timeOff = animTime * 0.12f + (float)b * 1.1f;
        float lengthScale = juce::jmap (decayVal, 0.0f, 1.0f, 0.55f, 1.35f);

        float endX, endY;
        if (isIdle)
        {
            // breathe in and out on sine cycle
            float breathScale = 0.75f + 0.25f * std::sin (idleBreath * 1.4f + (float)b * 0.8f);
            float len = br.length * lengthScale * breathScale;
            endX = cx + std::cos (br.angle + std::sin (timeOff) * 0.15f) * len;
            endY = cy + std::sin (br.angle + std::sin (timeOff) * 0.15f) * len;
        }
        else
        {
            float len = br.length * lengthScale * (1.0f + signalAmplitude * 0.3f);
            endX = cx + std::cos (br.angle + std::sin (timeOff) * 0.12f) * len;
            endY = cy + std::sin (br.angle + std::sin (timeOff) * 0.12f) * len;
        }

        // bezier control point
        float ctrlX = cx + std::cos (br.angle + 0.4f) * br.curveBias * lengthScale
                         + std::sin (timeOff * 0.7f) * 8.0f;
        float ctrlY = cy + std::sin (br.angle + 0.4f) * br.curveBias * lengthScale
                         + std::cos (timeOff * 0.7f) * 8.0f;

        juce::Path branch;
        branch.startNewSubPath (cx, cy);
        branch.quadraticTo (ctrlX, ctrlY, endX, endY);

        // main branch
        g.setColour (AC::luminousGreen().withAlpha (0.40f));
        g.strokePath (branch, juce::PathStrokeType (branchThickness,
                                                     juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));

        // midpoint for sub-branches
        float midX = (cx + endX) * 0.5f + (ctrlX - (cx + endX) * 0.5f) * 0.5f;
        float midY = (cy + endY) * 0.5f + (ctrlY - (cy + endY) * 0.5f) * 0.5f;

        // 2 sub-branches from midpoint
        constexpr int kSubBranches = 2;
        for (int s = 0; s < kSubBranches; ++s)
        {
            float subAngle = br.angle + (s == 0 ? 0.55f : -0.55f)
                            + std::sin (timeOff * 0.5f + (float)s) * 0.12f;
            float subLen = br.length * lengthScale * 0.45f;

            float subEndX = midX + std::cos (subAngle) * subLen;
            float subEndY = midY + std::sin (subAngle) * subLen;
            float subCtrlX = midX + std::cos (subAngle + 0.3f) * subLen * 0.5f;
            float subCtrlY = midY + std::sin (subAngle + 0.3f) * subLen * 0.5f;

            juce::Path subBranch;
            subBranch.startNewSubPath (midX, midY);
            subBranch.quadraticTo (subCtrlX, subCtrlY, subEndX, subEndY);

            g.setColour (AC::luminousGreen().withAlpha (0.22f));
            g.strokePath (subBranch, juce::PathStrokeType (branchThickness * 0.6f,
                                                            juce::PathStrokeType::curved,
                                                            juce::PathStrokeType::rounded));

            // lavender glow dot at sub-branch endpoint
            float dotAlpha = isIdle ? 0.30f : (0.30f + signalAmplitude * 0.45f);
            juce::ColourGradient dotGlow (AC::lavenderMist().withAlpha (dotAlpha),
                                           subEndX, subEndY,
                                           AC::lavenderMist().withAlpha (0.0f),
                                           subEndX + 7.0f, subEndY, true);
            g.setGradientFill (dotGlow);
            g.fillEllipse (subEndX - 7.0f, subEndY - 7.0f, 14.0f, 14.0f);
            g.setColour (AC::lavenderMist().withAlpha (dotAlpha * 1.5f));
            g.fillEllipse (subEndX - 2.0f, subEndY - 2.0f, 4.0f, 4.0f);
        }

        // lavender glow dot at main branch endpoint
        float endDotAlpha = isIdle ? 0.35f : (0.35f + signalAmplitude * 0.50f);
        juce::ColourGradient endDotGlow (AC::lavenderMist().withAlpha (endDotAlpha),
                                          endX, endY,
                                          AC::lavenderMist().withAlpha (0.0f),
                                          endX + 9.0f, endY, true);
        g.setGradientFill (endDotGlow);
        g.fillEllipse (endX - 9.0f, endY - 9.0f, 18.0f, 18.0f);
        g.setColour (AC::lavenderMist().withAlpha (endDotAlpha * 1.6f));
        g.fillEllipse (endX - 2.5f, endY - 2.5f, 5.0f, 5.0f);
    }

    // center node glow
    {
        float nodeAlpha = isIdle ? 0.30f : (0.30f + signalAmplitude * 0.40f);
        juce::ColourGradient nodeGlow (AC::luminousGreen().withAlpha (nodeAlpha),
                                        cx, cy,
                                        AC::luminousGreen().withAlpha (0.0f),
                                        cx + 14.0f, cy, true);
        g.setGradientFill (nodeGlow);
        g.fillEllipse (cx - 14.0f, cy - 14.0f, 28.0f, 28.0f);
        g.setColour (AC::luminousGreen().withAlpha (nodeAlpha * 2.0f));
        g.fillEllipse (cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);
    }

    // waveform overlay when active
    if (!isIdle)
    {
        constexpr int kPts = MyceliumProcessor::kScopeSize / 2;
        float yScale = gh * 0.30f;

        // input (thin lavender)
        auto inPath = buildVinePath (scopeIn, kPts, area, yScale);
        g.setColour (AC::lavenderMist().withAlpha (0.20f));
        g.strokePath (inPath, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));

        // output (green + lavender ghost)
        auto outPath = buildVinePath (scopeOut, kPts, area, yScale);
        g.setColour (AC::lavenderMist().withAlpha (0.14f));
        g.strokePath (outPath, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        g.setColour (AC::luminousGreen().withAlpha (0.70f));
        g.strokePath (outPath, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
    }

    // label
    g.setFont (labelFont().italicised());
    g.setColour (AC::parchment().withAlpha (0.28f));
    g.drawText ("Mycelium Network", (int)gx + 8, (int)gy + 4, 160, 16,
                juce::Justification::centredLeft);

    g.restoreState();
}

juce::Path MyceliumEditor::buildVinePath (const float* samples, int count,
                                           juce::Rectangle<float> area, float yScale) const
{
    juce::Path path;
    float stepX = area.getWidth() / (float)(count - 1);
    float centY = area.getCentreY();

    for (int i = 0; i < count; ++i)
    {
        float t  = (float)i / (float)count;
        float vx = area.getX() + (float)i * stepX
                   + std::sin (t * 16.0f + animTime * 0.55f) * 1.1f;
        float vy = centY - juce::jlimit (-1.0f, 1.0f, samples[i]) * yScale;

        if (i == 0) path.startNewSubPath (vx, vy);
        else        path.lineTo (vx, vy);
    }
    return path;
}

void MyceliumEditor::paintSectionDivider (juce::Graphics& g, float y)
{
    for (int i = 0; i < 9; ++i)
    {
        float x  = (float)kW * (0.12f + (float)i * 0.095f);
        float sz = (i % 2 == 0) ? 2.5f : 1.8f;
        g.setColour (AC::barkBrown().withAlpha (0.40f));
        g.fillEllipse (x - sz * 0.5f, y - sz * 0.5f, sz, sz);
    }
}

void MyceliumEditor::paintFooter (juce::Graphics& g)
{
    float fy = (float)kH - kFooterH;
    g.setColour (AC::deepEarth().withAlpha (0.65f));
    g.fillRect (0.0f, fy, (float)kW, kFooterH);
    g.setColour (AC::barkBrown().withAlpha (0.35f));
    g.drawLine (0.0f, fy, (float)kW, fy, 0.8f);

    g.setFont (labelFont().italicised());
    g.setColour (AC::parchment().withAlpha (0.28f));
    g.drawText ("Anthesis  \xe2\x80\x94  Mycelium  v1.0",
                16, (int)fy + 4, kW - 32, (int)kFooterH - 8,
                juce::Justification::centredLeft);
    g.drawText ("Schroeder reverb  \xc2\xb7  8 combs  \xc2\xb7  4 allpass diffusers",
                0, (int)fy + 4, kW - 16, (int)kFooterH - 8,
                juce::Justification::centredRight);
}

void MyceliumEditor::resized()
{
    presetBox.setBounds (kW - 210, 14, 196, 28);

    float macroY  = kHeaderH + kNetworkH + 22.0f;
    float macro1X = (float)kW * 0.5f - kMacroSize - 20.0f;
    float macro2X = (float)kW * 0.5f + 20.0f;

    macro1Knob.setBounds ((int)macro1X, (int)macroY, (int)kMacroSize, (int)kMacroSize);
    macro2Knob.setBounds ((int)macro2X, (int)macroY, (int)kMacroSize, (int)kMacroSize);
    macro1Label.setBounds ((int)macro1X, (int)(macroY + kMacroSize), (int)kMacroSize, 16);
    macro2Label.setBounds ((int)macro2X, (int)(macroY + kMacroSize), (int)kMacroSize, 16);

    float knobY   = macroY + kMacroH + 2.0f;
    float spacing = (float)kW / 7.0f;

    juce::Slider* knobs[]  = { &canopyKnob, &predelayKnob, &decayKnob, &dampingKnob, &widthKnob, &mixKnob, &natureKnob };
    juce::Label*  labels[] = { &canopyLabel, &predelayLabel, &decayLabel, &dampingLabel, &widthLabel, &mixLabel, &natureLabel };

    for (int i = 0; i < 7; ++i)
    {
        float kCx = spacing * (0.5f + (float)i);
        knobs[i]->setBounds  ((int)(kCx - kKnobSize * 0.5f), (int)knobY,
                               (int)kKnobSize, (int)kKnobSize);
        labels[i]->setBounds ((int)(kCx - kKnobSize * 0.5f), (int)(knobY + kKnobSize),
                               (int)kKnobSize, 16);
    }
}
