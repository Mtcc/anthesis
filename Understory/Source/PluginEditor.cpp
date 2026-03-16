#include "PluginEditor.h"

namespace
{
    constexpr float kHeaderH   = 58.0f;
    constexpr float kScopeH    = 160.0f;
    constexpr float kMacroH    = 108.0f;
    constexpr float kFooterH   = 40.0f;
    constexpr float kKnobSize  = 62.0f;
    constexpr float kMacroSize = 82.0f;
    constexpr float kTimerHz   = 25.0f;

    juce::Font titleFont() { return juce::Font (juce::FontOptions ("Georgia", 20.0f, juce::Font::plain)); }
    juce::Font labelFont() { return juce::Font (juce::FontOptions ("Georgia", 11.0f, juce::Font::plain)); }

    // type names in display order
    const char* kTypeNames[] = {
        "Rain", "Wind", "Stream", "Wind Chimes",
        "Crickets", "Birds", "Leaves", "Thunder",
        "Ocean", "Fire", "Frogs", "Cicadas",
        "Owls", "Cave Drips", "Bees"
    };
}

UnderstoryEditor::UnderstoryEditor (UnderstoryProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setSize (kW, kH);
    setLookAndFeel (&laf);

    // ── type selector ─────────────────────────────────────────────────────
    for (int i = 0; i < 15; ++i)
        typeBox.addItem (kTypeNames[i], i + 1);
    typeBox.setSelectedId (1, juce::dontSendNotification);
    addAndMakeVisible (typeBox);

    // AudioParameterInt: APVTS wraps int as 0-based, ComboBox uses 1-based IDs
    // We handle this manually via onChange rather than attachment to avoid mismatch
    typeBox.onChange = [this]
    {
        int sel = typeBox.getSelectedId() - 1;  // 0-based
        if (auto* param = proc.apvts.getParameter ("type"))
        {
            // normalise: range 0-14, so normalised = sel / 14
            param->setValueNotifyingHost ((float)sel / 14.0f);
        }
    };

    // sync combo to current parameter on open
    {
        int currentType = (int)(*proc.apvts.getRawParameterValue ("type"));
        typeBox.setSelectedId (currentType + 1, juce::dontSendNotification);
    }

    // ── knob factory ──────────────────────────────────────────────────────
    auto configKnob = [&] (juce::Slider& s, juce::Label& l, const juce::String& name)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 14);
        s.setColour (juce::Slider::textBoxTextColourId,       UC::dewGreen());
        s.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (s);

        l.setText (name, juce::dontSendNotification);
        l.setFont (labelFont());
        l.setColour (juce::Label::textColourId, UC::parchment().withAlpha (0.75f));
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };

    configKnob (densityKnob, densityLabel, "Density");
    configKnob (toneKnob,    toneLabel,    "Tone");
    configKnob (textureKnob, textureLabel, "Texture");
    configKnob (widthKnob,   widthLabel,   "Width");
    configKnob (levelKnob,   levelLabel,   "Level");
    configKnob (macro1Knob,  macro1Label,  "Macro 1");
    configKnob (macro2Knob,  macro2Label,  "Macro 2");

    densityAttach = std::make_unique<Attachment> (proc.apvts, "density", densityKnob);
    toneAttach    = std::make_unique<Attachment> (proc.apvts, "tone",    toneKnob);
    textureAttach = std::make_unique<Attachment> (proc.apvts, "texture", textureKnob);
    widthAttach   = std::make_unique<Attachment> (proc.apvts, "width",   widthKnob);
    levelAttach   = std::make_unique<Attachment> (proc.apvts, "level",   levelKnob);
    macro1Attach  = std::make_unique<Attachment> (proc.apvts, "macro1",  macro1Knob);
    macro2Attach  = std::make_unique<Attachment> (proc.apvts, "macro2",  macro2Knob);

    startTimerHz ((int)kTimerHz);
}

UnderstoryEditor::~UnderstoryEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

// ── timer ─────────────────────────────────────────────────────────────────────
void UnderstoryEditor::timerCallback()
{
    constexpr float dt = 1.0f / kTimerHz;
    animTime    += dt;
    breathPhase += dt * 0.16f;
    macro1Phase += dt * 0.07f;
    macro2Phase += dt * 0.05f;

    // copy scope data
    int wPos = proc.scopeWritePos.load (std::memory_order_relaxed);
    for (int i = 0; i < UnderstoryProcessor::kScopeSize; ++i)
    {
        int src = (wPos - UnderstoryProcessor::kScopeSize + i
                   + UnderstoryProcessor::kScopeSize) & (UnderstoryProcessor::kScopeSize - 1);
        scopeData[i] = proc.scopeBuffer[src];
    }

    // sync combo box to type parameter (handles DAW automation)
    {
        int currentType = (int)(*proc.apvts.getRawParameterValue ("type"));
        int comboSel    = typeBox.getSelectedId() - 1;
        if (currentType != comboSel)
            typeBox.setSelectedId (currentType + 1, juce::dontSendNotification);
    }

    repaint();
}

// ── paint ─────────────────────────────────────────────────────────────────────
void UnderstoryEditor::paint (juce::Graphics& g)
{
    paintBackground (g);
    paintScope (g);
    paintSectionDivider (g, kHeaderH + kScopeH + 4.0f);

    // macro section header
    g.setFont (labelFont().italicised());
    g.setColour (UC::parchment().withAlpha (0.25f));
    g.drawText ("Macros", 0, (int)(kHeaderH + kScopeH + 6.0f), kW, 14,
                juce::Justification::centred);

    paintFooter (g);
}

void UnderstoryEditor::paintOverChildren (juce::Graphics& g)
{
    // leaf overlays on macro knobs
    auto drawLeaves = [&] (juce::Slider& knob, float phase)
    {
        float val = (float)knob.getValue();
        if (val < 0.01f) return;
        laf.drawMacroLeaves (g, knob.getBoundsInParent().toFloat(), val, phase);
    };
    drawLeaves (macro1Knob, macro1Phase);
    drawLeaves (macro2Knob, macro2Phase);
}

void UnderstoryEditor::paintBackground (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // forest floor base gradient
    {
        juce::ColourGradient grad (UC::shadowGreen(), bounds.getCentreX(), bounds.getY(),
                                    UC::deepForest(),  bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRect (bounds);
    }

    // organic grain texture (deterministic seed)
    g.setColour (UC::moss().withAlpha (0.018f));
    {
        juce::Random texRng (42);
        for (int i = 0; i < 200; ++i)
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
            g.strokePath (grain, juce::PathStrokeType (0.28f));
        }
    }

    // ambient green light pools — forest canopy feel
    float breathMod = 0.88f + 0.12f * std::sin (breathPhase);

    auto drawGlow = [&] (float cx, float cy, float r, juce::Colour col, float alpha)
    {
        juce::ColourGradient glow (col.withAlpha (alpha * breathMod), cx, cy,
                                    col.withAlpha (0.0f), cx + r, cy, true);
        g.setGradientFill (glow);
        g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    };

    drawGlow ((float)kW * 0.15f, (float)kH * 0.10f, 220.0f, UC::fernGreen(),  0.09f);
    drawGlow ((float)kW * 0.82f, (float)kH * 0.72f, 180.0f, UC::dewGreen(),   0.06f);
    drawGlow ((float)kW * 0.48f, (float)kH * 0.38f, 140.0f, UC::lichenGold(), 0.04f);

    // header panel
    {
        juce::ColourGradient hg (UC::barkDark().withAlpha (0.65f), 0, 0,
                                  UC::deepForest().withAlpha (0.0f), 0, kHeaderH, false);
        g.setGradientFill (hg);
        g.fillRect (0.0f, 0.0f, (float)kW, kHeaderH);
    }
    g.setColour (UC::earthBrown().withAlpha (0.40f));
    g.drawLine (0.0f, kHeaderH, (float)kW, kHeaderH, 1.0f);

    // title
    g.setFont (titleFont());
    g.setColour (UC::parchment());
    g.drawText ("Understory", 18, 8, 220, 28, juce::Justification::centredLeft);

    g.setFont (labelFont().italicised());
    g.setColour (UC::fernGreen().withAlpha (0.65f));
    g.drawText ("nature texture synthesizer", 18, 34, 240, 16,
                juce::Justification::centredLeft);
}

// ── Scope / waveform display ───────────────────────────────────────────────────
void UnderstoryEditor::paintScope (juce::Graphics& g)
{
    float gx = 16.0f, gy = kHeaderH + 8.0f;
    float gw = (float)kW - 32.0f, gh = kScopeH - 16.0f;
    auto  area = juce::Rectangle<float> (gx, gy, gw, gh);

    // panel background
    g.setColour (UC::fernGreen().withAlpha (0.05f));
    g.fillRoundedRectangle (area, 10.0f);

    // organic border
    {
        juce::Path border;
        border.addRoundedRectangle (area.reduced (0.5f), 10.0f);
        g.setColour (UC::earthBrown().withAlpha (0.45f));
        g.strokePath (border, juce::PathStrokeType (1.2f));
        g.setColour (UC::fernGreen().withAlpha (0.05f));
        g.strokePath (border, juce::PathStrokeType (3.5f));
    }

    g.saveState();
    g.reduceClipRegion (area.toNearestInt());

    // centre line
    float centY = gy + gh * 0.5f;
    g.setColour (UC::earthBrown().withAlpha (0.18f));
    g.drawLine (gx, centY, gx + gw, centY, 0.7f);

    // ── waveform vine ─────────────────────────────────────────────────────
    constexpr int kPts   = UnderstoryProcessor::kScopeSize / 2;
    float         yScale = gh * 0.42f;
    float         stepX  = gw / (float)(kPts - 1);

    // check if there's any signal
    float maxSig = 0.0f;
    for (int i = 0; i < kPts; ++i)
        maxSig = juce::jmax (maxSig, std::abs (scopeData[i]));

    if (maxSig < 0.001f)
    {
        // idle: gentle breathing vine
        float amp    = 6.0f + 4.0f * std::sin (breathPhase * 1.8f);
        float period = gw / 2.0f;

        juce::Path vine;
        for (int px = 0; px <= (int)gw; px += 2)
        {
            float t  = (float)px / gw;
            float vx = gx + (float)px + std::sin (t * 12.0f + animTime * 0.30f) * 1.0f;
            float vy = centY + amp * std::sin (t * juce::MathConstants<float>::twoPi / period
                           + animTime * 0.15f);
            if (px == 0) vine.startNewSubPath (vx, vy);
            else         vine.lineTo (vx, vy);
        }
        g.setColour (UC::fernGreen().withAlpha (0.14f));
        g.strokePath (vine, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        g.setColour (UC::dewGreen().withAlpha (0.35f));
        g.strokePath (vine, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }
    else
    {
        // active waveform
        juce::Path wavePath;
        for (int i = 0; i < kPts; ++i)
        {
            float t  = (float)i / (float)kPts;
            float vx = gx + (float)i * stepX
                       + std::sin (t * 14.0f + animTime * 0.45f) * 0.8f;
            float vy = centY - juce::jlimit (-1.0f, 1.0f, scopeData[i]) * yScale;

            if (i == 0) wavePath.startNewSubPath (vx, vy);
            else        wavePath.lineTo (vx, vy);
        }

        // glow pass
        g.setColour (UC::fernGreen().withAlpha (0.10f));
        g.strokePath (wavePath, juce::PathStrokeType (4.5f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        // main vine
        g.setColour (UC::dewGreen().withAlpha (0.78f));
        g.strokePath (wavePath, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
    }

    // panel label
    g.setFont (juce::Font (juce::FontOptions ("Georgia", 11.0f, juce::Font::italic)));
    g.setColour (UC::parchment().withAlpha (0.25f));
    g.drawText ("Output", (int)gx + 8, (int)gy + 4, 80, 16,
                juce::Justification::centredLeft);

    g.restoreState();
}

void UnderstoryEditor::paintSectionDivider (juce::Graphics& g, float y)
{
    for (int i = 0; i < 9; ++i)
    {
        float x  = (float)kW * (0.12f + (float)i * 0.095f);
        float sz = (i % 2 == 0) ? 2.4f : 1.7f;
        g.setColour (UC::earthBrown().withAlpha (0.38f));
        g.fillEllipse (x - sz * 0.5f, y - sz * 0.5f, sz, sz);
    }
}

void UnderstoryEditor::paintFooter (juce::Graphics& g)
{
    float fy = (float)kH - kFooterH;
    g.setColour (UC::barkDark().withAlpha (0.68f));
    g.fillRect (0.0f, fy, (float)kW, kFooterH);
    g.setColour (UC::earthBrown().withAlpha (0.35f));
    g.drawLine (0.0f, fy, (float)kW, fy, 0.8f);

    g.setFont (labelFont().italicised());
    g.setColour (UC::parchment().withAlpha (0.28f));
    g.drawText ("Anthesis  \xe2\x80\x94  Understory  v1.0",
                16, (int)fy + 4, kW - 32, (int)kFooterH - 8,
                juce::Justification::centredLeft);
    g.drawText ("procedural nature textures  \xc2\xb7  forest floor  \xc2\xb7  pure synthesis",
                0, (int)fy + 4, kW - 16, (int)kFooterH - 8,
                juce::Justification::centredRight);
}

// ── layout ────────────────────────────────────────────────────────────────────
void UnderstoryEditor::resized()
{
    // type selector in header (right side)
    typeBox.setBounds (kW - 220, 14, 206, 28);

    // macro knobs: centred below scope panel
    float macroY  = kHeaderH + kScopeH + 20.0f;
    float macro1X = (float)kW * 0.5f - kMacroSize - 22.0f;
    float macro2X = (float)kW * 0.5f + 22.0f;

    macro1Knob.setBounds ((int)macro1X, (int)macroY, (int)kMacroSize, (int)kMacroSize);
    macro2Knob.setBounds ((int)macro2X, (int)macroY, (int)kMacroSize, (int)kMacroSize);
    macro1Label.setBounds ((int)macro1X, (int)(macroY + kMacroSize), (int)kMacroSize, 16);
    macro2Label.setBounds ((int)macro2X, (int)(macroY + kMacroSize), (int)kMacroSize, 16);

    // 5 main knobs in bottom row
    float knobY   = macroY + kMacroH + 4.0f;
    float spacing = (float)kW / 5.0f;

    juce::Slider* knobs[]  = { &densityKnob, &toneKnob, &textureKnob, &widthKnob, &levelKnob };
    juce::Label*  labels[] = { &densityLabel, &toneLabel, &textureLabel, &widthLabel, &levelLabel };

    for (int i = 0; i < 5; ++i)
    {
        float cx = spacing * (0.5f + (float)i);
        knobs[i]->setBounds  ((int)(cx - kKnobSize * 0.5f), (int)knobY,
                               (int)kKnobSize, (int)kKnobSize);
        labels[i]->setBounds ((int)(cx - kKnobSize * 0.5f), (int)(knobY + kKnobSize),
                               (int)kKnobSize, 16);
    }
}
