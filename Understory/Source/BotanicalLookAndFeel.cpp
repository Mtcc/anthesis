#include "BotanicalLookAndFeel.h"

UnderstoryLookAndFeel::UnderstoryLookAndFeel()
{
    // Understory: deep forest floor palette — dark greens and earthy tones
    setColour (juce::Slider::thumbColourId,                UC::dewGreen());
    setColour (juce::Slider::rotarySliderFillColourId,     UC::fernGreen());
    setColour (juce::Slider::rotarySliderOutlineColourId,  UC::earthBrown());
    setColour (juce::Label::textColourId,                  UC::parchment());
    setColour (juce::ComboBox::backgroundColourId,         UC::barkDark());
    setColour (juce::ComboBox::outlineColourId,            UC::earthBrown());
    setColour (juce::ComboBox::textColourId,               UC::parchment());
    setColour (juce::ComboBox::arrowColourId,              UC::dewGreen());
    setColour (juce::PopupMenu::backgroundColourId,        UC::barkDark());
    setColour (juce::PopupMenu::textColourId,              UC::parchment());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, UC::earthBrown().brighter (0.3f));
    setColour (juce::PopupMenu::highlightedTextColourId,   UC::dewGreen());
}

void UnderstoryLookAndFeel::drawRotarySlider (juce::Graphics& g,
    int x, int y, int w, int h,
    float sliderPos, float startAngle, float endAngle,
    juce::Slider& /*slider*/)
{
    auto bounds = juce::Rectangle<float> ((float)x, (float)y, (float)w, (float)h).reduced (4.0f);
    auto centre = bounds.getCentre();
    float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;

    // ── 1. body: dark forest floor concentric rings ───────────────────────
    constexpr int kRings = 5;
    for (int ring = kRings; ring >= 1; --ring)
    {
        float r     = radius * (float)ring / (float)kRings;
        float alpha = 0.06f + (float)(kRings - ring) * 0.10f;
        auto  col   = (ring % 2 == 0) ? UC::earthBrown() : UC::barkDark();
        g.setColour (col.withAlpha (alpha));
        g.fillEllipse (centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);
    }

    // moss centre highlight
    {
        float cr = radius * 0.20f;
        juce::ColourGradient cg (UC::moss().withAlpha (0.22f), centre.x, centre.y,
                                  UC::moss().withAlpha (0.0f),  centre.x + cr, centre.y, true);
        g.setGradientFill (cg);
        g.fillEllipse (centre.x - cr, centre.y - cr, cr * 2.0f, cr * 2.0f);
    }

    // outer rim
    g.setColour (UC::earthBrown().withAlpha (0.65f));
    g.drawEllipse (bounds.reduced (1.0f), 1.2f);

    // ── 2. value arc as fern-green vine ───────────────────────────────────
    float angle  = startAngle + sliderPos * (endAngle - startAngle);
    float arcR   = radius * 0.78f;
    float thick  = radius * 0.11f;

    if (sliderPos > 0.001f)
    {
        juce::Path vinePath;
        constexpr int kSteps = 80;

        for (int i = 0; i <= kSteps; ++i)
        {
            float t = (float)i / (float)kSteps;
            if (t > sliderPos && i > 0) break;

            float a      = startAngle + t * (endAngle - startAngle);
            float wobble = std::sin (t * juce::MathConstants<float>::twoPi * 2.5f) * (radius * 0.016f);
            float r      = arcR + wobble;
            float px     = centre.x + r * std::sin (a);
            float py     = centre.y - r * std::cos (a);

            if (i == 0) vinePath.startNewSubPath (px, py);
            else        vinePath.lineTo (px, py);
        }

        // dew-green ghost offset
        g.setColour (UC::dewGreen().withAlpha (0.28f));
        g.strokePath (vinePath,
            juce::PathStrokeType (thick * 1.4f,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // main fern vine
        g.setColour (UC::fernGreen().withAlpha (0.90f));
        g.strokePath (vinePath,
            juce::PathStrokeType (thick,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // ── 3. indicator: dew-drop glowing dot ────────────────────────────────
    float dotX = centre.x + arcR * std::sin (angle);
    float dotY = centre.y - arcR * std::cos (angle);

    // green glow halo
    juce::ColourGradient glow (UC::dewGreen().withAlpha (0.40f), dotX, dotY,
                                UC::dewGreen().withAlpha (0.0f),  dotX + 10.0f, dotY, true);
    g.setGradientFill (glow);
    g.fillEllipse (dotX - 10.0f, dotY - 10.0f, 20.0f, 20.0f);

    // bright dot core
    g.setColour (UC::dewGreen());
    g.fillEllipse (dotX - 3.5f, dotY - 3.5f, 7.0f, 7.0f);

    // mist-white highlight centre
    g.setColour (UC::mistWhite().withAlpha (0.6f));
    g.fillEllipse (dotX - 1.5f, dotY - 1.5f, 3.0f, 3.0f);
}

void UnderstoryLookAndFeel::drawMacroLeaves (juce::Graphics& g,
    juce::Rectangle<float> bounds, float value, float phaseOffset)
{
    if (value < 0.02f) return;

    auto  centre  = bounds.getCentre();
    float radius  = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    float leafR   = radius * 1.15f;

    int   maxLeaves  = 6;
    float leafCount  = value * (float)maxLeaves;
    int   fullLeaves = (int)leafCount;
    float partial    = leafCount - (float)fullLeaves;

    auto drawLeaf = [&] (float angle, float scale)
    {
        if (scale < 0.02f) return;

        float px = centre.x + leafR * std::sin (angle);
        float py = centre.y - leafR * std::cos (angle);

        juce::Path leaf;
        float pw = 4.5f * scale, ph = 9.5f * scale;
        leaf.addEllipse (-pw * 0.5f, -ph * 0.5f, pw, ph);

        juce::AffineTransform t = juce::AffineTransform::rotation (angle)
                                    .translated (px, py);

        auto col = UC::fernGreen().interpolatedWith (UC::dewGreen(), value);
        g.setColour (col.withAlpha (0.55f * scale));
        g.fillPath (leaf, t);
        g.setColour (col.withAlpha (0.22f * scale));
        g.strokePath (leaf, juce::PathStrokeType (0.6f), t);
    };

    for (int p = 0; p < fullLeaves; ++p)
    {
        float a = phaseOffset + (float)p * juce::MathConstants<float>::twoPi / (float)maxLeaves;
        drawLeaf (a, 1.0f);
    }
    if (fullLeaves < maxLeaves)
    {
        float a = phaseOffset + (float)fullLeaves * juce::MathConstants<float>::twoPi / (float)maxLeaves;
        drawLeaf (a, partial);
    }
}

void UnderstoryLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.setFont (serifFont.withHeight (label.getFont().getHeight()));
    g.setColour (UC::parchment().withAlpha (0.85f));
    g.drawFittedText (label.getText(), label.getLocalBounds(),
                      label.getJustificationType(), 2, 0.85f);
}

void UnderstoryLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h,
    bool /*isDown*/, int /*bx*/, int /*by*/, int /*bw*/, int /*bh*/, juce::ComboBox&)
{
    auto bounds = juce::Rectangle<float> (0, 0, (float)w, (float)h);
    g.setColour (UC::barkDark());
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (UC::earthBrown());
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    // small leaf glyph as dropdown arrow
    float ax = (float)w - 16.0f, ay = (float)h * 0.5f;
    juce::Path leaf;
    leaf.startNewSubPath (ax, ay - 4.0f);
    leaf.quadraticTo (ax + 5.0f, ay, ax, ay + 4.0f);
    leaf.quadraticTo (ax - 5.0f, ay, ax, ay - 4.0f);
    g.setColour (UC::dewGreen());
    g.fillPath (leaf);
}

void UnderstoryLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font UnderstoryLookAndFeel::getLabelFont (juce::Label&)
{
    return serifFont;
}

juce::Font UnderstoryLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return serifFont.withHeight (12.0f);
}
