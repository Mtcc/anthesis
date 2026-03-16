#include "BotanicalLookAndFeel.h"

AureoleLookAndFeel::AureoleLookAndFeel()
{
    // Aureole: primary amber/golden palette — no luminous green
    setColour (juce::Slider::thumbColourId,                AC::goldenHour());
    setColour (juce::Slider::rotarySliderFillColourId,     AC::warmAmber());
    setColour (juce::Slider::rotarySliderOutlineColourId,  AC::barkBrown());
    setColour (juce::Label::textColourId,                  AC::parchment());
    setColour (juce::ComboBox::backgroundColourId,         AC::deepEarth());
    setColour (juce::ComboBox::outlineColourId,            AC::barkBrown());
    setColour (juce::ComboBox::textColourId,               AC::parchment());
    setColour (juce::ComboBox::arrowColourId,              AC::warmAmber());
    setColour (juce::PopupMenu::backgroundColourId,        AC::deepEarth());
    setColour (juce::PopupMenu::textColourId,              AC::parchment());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, AC::barkBrown().brighter (0.3f));
    setColour (juce::PopupMenu::highlightedTextColourId,   AC::goldenHour());
}

void AureoleLookAndFeel::drawRotarySlider (juce::Graphics& g,
    int x, int y, int w, int h,
    float sliderPos, float startAngle, float endAngle,
    juce::Slider& /*slider*/)
{
    auto bounds = juce::Rectangle<float> ((float)x, (float)y, (float)w, (float)h).reduced (4.0f);
    auto centre = bounds.getCentre();
    float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;

    // ── 1. body: warm concentric rings (bark-like, amber-toned) ──────────
    constexpr int kRings = 5;
    for (int ring = kRings; ring >= 1; --ring)
    {
        float r     = radius * (float)ring / (float)kRings;
        float alpha = 0.07f + (float)(kRings - ring) * 0.11f;
        auto  col   = (ring % 2 == 0) ? AC::barkBrown() : AC::deepEarth();
        g.setColour (col.withAlpha (alpha));
        g.fillEllipse (centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);
    }

    // warm amber centre highlight
    {
        float cr = radius * 0.20f;
        juce::ColourGradient cg (AC::warmAmber().withAlpha (0.18f), centre.x, centre.y,
                                  AC::warmAmber().withAlpha (0.0f),  centre.x + cr, centre.y, true);
        g.setGradientFill (cg);
        g.fillEllipse (centre.x - cr, centre.y - cr, cr * 2.0f, cr * 2.0f);
    }

    // outer rim
    g.setColour (AC::barkBrown().withAlpha (0.65f));
    g.drawEllipse (bounds.reduced (1.0f), 1.2f);

    // ── 2. value arc as warm amber vine ───────────────────────────────────
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

        // golden ghost offset (lighter catching vine edge)
        g.setColour (AC::goldenHour().withAlpha (0.30f));
        g.strokePath (vinePath,
            juce::PathStrokeType (thick * 1.4f,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // main warm amber vine
        g.setColour (AC::warmAmber().withAlpha (0.92f));
        g.strokePath (vinePath,
            juce::PathStrokeType (thick,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // ── 3. indicator: golden glowing dot ──────────────────────────────────
    float dotX = centre.x + arcR * std::sin (angle);
    float dotY = centre.y - arcR * std::cos (angle);

    // warm amber glow halo
    juce::ColourGradient glow (AC::warmAmber().withAlpha (0.45f), dotX, dotY,
                                AC::warmAmber().withAlpha (0.0f),  dotX + 10.0f, dotY, true);
    g.setGradientFill (glow);
    g.fillEllipse (dotX - 10.0f, dotY - 10.0f, 20.0f, 20.0f);

    // golden hour bright core
    g.setColour (AC::goldenHour());
    g.fillEllipse (dotX - 3.5f, dotY - 3.5f, 7.0f, 7.0f);

    // parchment highlight centre
    g.setColour (AC::parchment().withAlpha (0.6f));
    g.fillEllipse (dotX - 1.5f, dotY - 1.5f, 3.0f, 3.0f);
}

void AureoleLookAndFeel::drawMacroPetals (juce::Graphics& g,
    juce::Rectangle<float> bounds, float value, float phaseOffset)
{
    if (value < 0.02f) return;

    auto  centre  = bounds.getCentre();
    float radius  = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    float petalR  = radius * 1.15f;

    int   maxPetals  = 6;
    float petalCount = value * (float)maxPetals;
    int   fullPetals = (int)petalCount;
    float partial    = petalCount - (float)fullPetals;

    auto drawPetal = [&] (float angle, float scale)
    {
        if (scale < 0.02f) return;

        float px = centre.x + petalR * std::sin (angle);
        float py = centre.y - petalR * std::cos (angle);

        juce::Path petal;
        float pw = 5.0f * scale, ph = 9.0f * scale;
        petal.addEllipse (-pw * 0.5f, -ph * 0.5f, pw, ph);

        juce::AffineTransform t = juce::AffineTransform::rotation (angle)
                                    .translated (px, py);

        // amber → golden as value increases
        auto col = AC::warmAmber().interpolatedWith (AC::goldenHour(), value);
        g.setColour (col.withAlpha (0.55f * scale));
        g.fillPath (petal, t);
        g.setColour (col.withAlpha (0.22f * scale));
        g.strokePath (petal, juce::PathStrokeType (0.6f), t);
    };

    for (int p = 0; p < fullPetals; ++p)
    {
        float a = phaseOffset + (float)p * juce::MathConstants<float>::twoPi / (float)maxPetals;
        drawPetal (a, 1.0f);
    }
    if (fullPetals < maxPetals)
    {
        float a = phaseOffset + (float)fullPetals * juce::MathConstants<float>::twoPi / (float)maxPetals;
        drawPetal (a, partial);
    }
}

void AureoleLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.setFont (serifFont.withHeight (label.getFont().getHeight()));
    g.setColour (AC::parchment().withAlpha (0.85f));
    g.drawFittedText (label.getText(), label.getLocalBounds(),
                      label.getJustificationType(), 2, 0.85f);
}

void AureoleLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h,
    bool /*isDown*/, int /*bx*/, int /*by*/, int /*bw*/, int /*bh*/, juce::ComboBox&)
{
    auto bounds = juce::Rectangle<float> (0, 0, (float)w, (float)h);
    g.setColour (AC::deepEarth());
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (AC::barkBrown());
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    // small leaf glyph as dropdown arrow
    float ax = (float)w - 16.0f, ay = (float)h * 0.5f;
    juce::Path leaf;
    leaf.startNewSubPath (ax, ay - 4.0f);
    leaf.quadraticTo (ax + 5.0f, ay, ax, ay + 4.0f);
    leaf.quadraticTo (ax - 5.0f, ay, ax, ay - 4.0f);
    g.setColour (AC::warmAmber());
    g.fillPath (leaf);
}

void AureoleLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font AureoleLookAndFeel::getLabelFont (juce::Label&)
{
    return serifFont;
}

juce::Font AureoleLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return serifFont.withHeight (12.0f);
}
