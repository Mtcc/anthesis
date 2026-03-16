#include "BotanicalLookAndFeel.h"

AlluviumLookAndFeel::AlluviumLookAndFeel()
{
    setColour (juce::Slider::thumbColourId,                  AC::lavenderMist());
    setColour (juce::Slider::rotarySliderFillColourId,       AC::luminousGreen());
    setColour (juce::Slider::rotarySliderOutlineColourId,    AC::barkBrown());
    setColour (juce::Label::textColourId,                    AC::parchment());
    setColour (juce::ComboBox::backgroundColourId,           AC::deepEarth());
    setColour (juce::ComboBox::outlineColourId,              AC::barkBrown());
    setColour (juce::ComboBox::textColourId,                 AC::parchment());
    setColour (juce::ComboBox::arrowColourId,                AC::lavenderMist());
    setColour (juce::PopupMenu::backgroundColourId,          AC::deepEarth());
    setColour (juce::PopupMenu::textColourId,                AC::parchment());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, AC::moss());
    setColour (juce::PopupMenu::highlightedTextColourId,     AC::lavenderMist());
}

void AlluviumLookAndFeel::drawRotarySlider (juce::Graphics& g,
    int x, int y, int w, int h,
    float sliderPos, float startAngle, float endAngle,
    juce::Slider& /*slider*/)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    auto centre = bounds.getCentre();
    float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;

    // ── 1. body: concentric sediment-layer rings ────────────────────────
    constexpr int kRings = 5;
    for (int ring = kRings; ring >= 1; --ring)
    {
        float r     = radius * (float) ring / (float) kRings;
        float alpha = 0.07f + (float)(kRings - ring) * 0.10f;
        auto  col   = (ring % 2 == 0) ? AC::moss() : AC::deepForest();
        g.setColour (col.withAlpha (alpha));
        g.fillEllipse (centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);
    }
    // subtle lavender centre glow
    {
        float cr = radius * 0.18f;
        juce::ColourGradient cg (AC::lavenderMist().withAlpha (0.10f), centre.x, centre.y,
                                  AC::lavenderMist().withAlpha (0.0f), centre.x + cr, centre.y, true);
        g.setGradientFill (cg);
        g.fillEllipse (centre.x - cr, centre.y - cr, cr * 2.0f, cr * 2.0f);
    }
    // outer rim
    g.setColour (AC::barkBrown().withAlpha (0.6f));
    g.drawEllipse (bounds.reduced (1.0f), 1.2f);

    // ── 2. value arc as organic vine ────────────────────────────────────
    float angle    = startAngle + sliderPos * (endAngle - startAngle);
    float arcR     = radius * 0.78f;
    float thickness = radius * 0.11f;

    if (sliderPos > 0.001f)
    {
        juce::Path vinePath;
        constexpr int kSteps = 80;

        for (int i = 0; i <= kSteps; ++i)
        {
            float t = (float) i / (float) kSteps;
            if (t > sliderPos && i > 0) break;

            float a      = startAngle + t * (endAngle - startAngle);
            float wobble = std::sin (t * juce::MathConstants<float>::twoPi * 2.5f) * (radius * 0.018f);
            float r      = arcR + wobble;

            float px = centre.x + r * std::sin (a);
            float py = centre.y - r * std::cos (a);

            if (i == 0) vinePath.startNewSubPath (px, py);
            else        vinePath.lineTo (px, py);
        }

        // lavender ghost halo
        g.setColour (AC::lavenderMist().withAlpha (0.30f));
        g.strokePath (vinePath,
            juce::PathStrokeType (thickness * 1.4f,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // main luminous green vine
        g.setColour (AC::luminousGreen().withAlpha (0.90f));
        g.strokePath (vinePath,
            juce::PathStrokeType (thickness,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // ── 3. indicator: lavender mist dot ─────────────────────────────────
    float dotX = centre.x + arcR * std::sin (angle);
    float dotY = centre.y - arcR * std::cos (angle);

    // soft glow halo
    juce::ColourGradient glow (AC::lavenderMist().withAlpha (0.40f), dotX, dotY,
                               AC::lavenderMist().withAlpha (0.0f), dotX + 10.0f, dotY, true);
    g.setGradientFill (glow);
    g.fillEllipse (dotX - 10.0f, dotY - 10.0f, 20.0f, 20.0f);

    // bright core
    g.setColour (AC::lavenderMist());
    g.fillEllipse (dotX - 3.5f, dotY - 3.5f, 7.0f, 7.0f);
    g.setColour (AC::parchment().withAlpha (0.55f));
    g.fillEllipse (dotX - 1.5f, dotY - 1.5f, 3.0f, 3.0f);
}

void AlluviumLookAndFeel::drawMacroPetals (juce::Graphics& g,
    juce::Rectangle<float> bounds, float value, float phaseOffset)
{
    if (value < 0.02f) return;

    auto  centre  = bounds.getCentre();
    float radius  = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    float petalR  = radius * 1.15f;

    int   maxPetals  = 6;
    float petalCount = value * (float) maxPetals;
    int   fullPetals = (int) petalCount;
    float partial    = petalCount - (float) fullPetals;

    auto drawPetal = [&] (float angle, float scale)
    {
        if (scale < 0.02f) return;

        float px = centre.x + petalR * std::sin (angle);
        float py = centre.y - petalR * std::cos (angle);

        juce::Path petal;
        float pw = 5.0f * scale, ph = 9.0f * scale;
        petal.addEllipse (-pw * 0.5f, -ph * 0.5f, pw, ph);

        juce::AffineTransform t = juce::AffineTransform::rotation (angle).translated (px, py);

        // color transitions luminousGreen → lavenderMist
        auto col = AC::luminousGreen().interpolatedWith (AC::lavenderMist(), value);
        g.setColour (col.withAlpha (0.55f * scale));
        g.fillPath (petal, t);
        g.setColour (col.withAlpha (0.25f * scale));
        g.strokePath (petal, juce::PathStrokeType (0.6f), t);
    };

    for (int p = 0; p < fullPetals; ++p)
    {
        float angle = phaseOffset + (float) p * juce::MathConstants<float>::twoPi / (float) maxPetals;
        drawPetal (angle, 1.0f);
    }
    if (fullPetals < maxPetals)
    {
        float angle = phaseOffset + (float) fullPetals * juce::MathConstants<float>::twoPi / (float) maxPetals;
        drawPetal (angle, partial);
    }
}

void AlluviumLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.setFont (serifFont.withHeight (label.getFont().getHeight()));
    g.setColour (AC::parchment().withAlpha (0.85f));
    g.drawFittedText (label.getText(), label.getLocalBounds(),
                      label.getJustificationType(), 2, 0.85f);
}

void AlluviumLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h,
    bool /*isDown*/, int /*bx*/, int /*by*/, int /*bw*/, int /*bh*/, juce::ComboBox&)
{
    auto bounds = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (AC::deepEarth());
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (AC::barkBrown());
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    // small lavender leaf glyph as dropdown indicator
    float ax = w - 16.0f, ay = h * 0.5f;
    juce::Path leaf;
    leaf.startNewSubPath (ax, ay - 4.0f);
    leaf.quadraticTo (ax + 5.0f, ay, ax, ay + 4.0f);
    leaf.quadraticTo (ax - 5.0f, ay, ax, ay - 4.0f);
    g.setColour (AC::lavenderMist());
    g.fillPath (leaf);
}

void AlluviumLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font AlluviumLookAndFeel::getLabelFont (juce::Label&)
{
    return serifFont;
}

juce::Font AlluviumLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return serifFont.withHeight (12.0f);
}
