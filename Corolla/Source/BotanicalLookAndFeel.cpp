#include "BotanicalLookAndFeel.h"

CorollaLookAndFeel::CorollaLookAndFeel()
{
    // petalPink as the primary accent colour
    setColour (juce::Slider::thumbColourId,               AC::petalPink());
    setColour (juce::Slider::rotarySliderFillColourId,    AC::luminousGreen());
    setColour (juce::Slider::rotarySliderOutlineColourId, AC::barkBrown());
    setColour (juce::Label::textColourId,                 AC::parchment());
    setColour (juce::ComboBox::backgroundColourId,        AC::deepEarth());
    setColour (juce::ComboBox::outlineColourId,           AC::barkBrown());
    setColour (juce::ComboBox::textColourId,              AC::parchment());
    setColour (juce::ComboBox::arrowColourId,             AC::petalPink());
    setColour (juce::PopupMenu::backgroundColourId,       AC::deepEarth());
    setColour (juce::PopupMenu::textColourId,             AC::parchment());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, AC::canopyGreen());
    setColour (juce::PopupMenu::highlightedTextColourId,  AC::lavenderMist());
}

void CorollaLookAndFeel::drawRotarySlider (juce::Graphics& g,
    int x, int y, int w, int h,
    float sliderPos, float startAngle, float endAngle,
    juce::Slider& /*slider*/)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    auto centre = bounds.getCentre();
    float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;

    // ── 1. body: concentric tree-ring layers ──────────────────────────────
    constexpr int kRings = 5;
    for (int ring = kRings; ring >= 1; --ring)
    {
        float r     = radius * (float) ring / (float) kRings;
        float alpha = 0.08f + (float) (kRings - ring) * 0.12f;
        auto  col   = (ring % 2 == 0) ? AC::barkBrown() : AC::deepEarth();
        g.setColour (col.withAlpha (alpha));
        g.fillEllipse (centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);
    }

    // subtle centre highlight in petalPink
    {
        float cr = radius * 0.18f;
        juce::ColourGradient cg (AC::petalPink().withAlpha (0.14f), centre.x, centre.y,
                                 AC::petalPink().withAlpha (0.0f), centre.x + cr, centre.y, true);
        g.setGradientFill (cg);
        g.fillEllipse (centre.x - cr, centre.y - cr, cr * 2.0f, cr * 2.0f);
    }

    // outer rim
    g.setColour (AC::barkBrown().withAlpha (0.7f));
    g.drawEllipse (bounds.reduced (1.0f), 1.2f);

    // ── 2. value arc: luminousGreen vine with petalPink ghost ─────────────
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

            float a = startAngle + t * (endAngle - startAngle);
            float wobble = std::sin (t * juce::MathConstants<float>::twoPi * 2.5f) * (radius * 0.018f);
            float r = arcR + wobble;

            float px = centre.x + r * std::sin (a);
            float py = centre.y - r * std::cos (a);

            if (i == 0) vinePath.startNewSubPath (px, py);
            else        vinePath.lineTo (px, py);
        }

        // petalPink ghost offset
        g.setColour (AC::petalPink().withAlpha (0.25f));
        g.strokePath (vinePath,
            juce::PathStrokeType (thickness * 1.4f,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // main luminous green vine
        g.setColour (AC::luminousGreen().withAlpha (0.92f));
        g.strokePath (vinePath,
            juce::PathStrokeType (thickness,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // ── 3. indicator: petalPink glowing dot ──────────────────────────────
    float dotX = centre.x + arcR * std::sin (angle);
    float dotY = centre.y - arcR * std::cos (angle);

    // lavender halo
    juce::ColourGradient glow (AC::lavenderMist().withAlpha (0.30f), dotX, dotY,
                               AC::lavenderMist().withAlpha (0.0f), dotX + 10.0f, dotY, true);
    g.setGradientFill (glow);
    g.fillEllipse (dotX - 10.0f, dotY - 10.0f, 20.0f, 20.0f);

    // bright petalPink core
    g.setColour (AC::petalPink());
    g.fillEllipse (dotX - 3.5f, dotY - 3.5f, 7.0f, 7.0f);
    g.setColour (AC::parchment().withAlpha (0.6f));
    g.fillEllipse (dotX - 1.5f, dotY - 1.5f, 3.0f, 3.0f);
}

void CorollaLookAndFeel::drawMacroPetals (juce::Graphics& g,
    juce::Rectangle<float> bounds, float value, float phaseOffset)
{
    if (value < 0.02f) return;

    auto centre = bounds.getCentre();
    float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    float petalR = radius * 1.15f;   // petals extend just beyond the knob edge

    int maxPetals    = 6;
    float petalCount = value * (float) maxPetals;
    int   fullPetals = (int) petalCount;
    float partial    = petalCount - (float) fullPetals;

    auto drawPetal = [&] (float angle, float scale)
    {
        if (scale < 0.02f) return;

        float px = centre.x + petalR * std::sin (angle);
        float py = centre.y - petalR * std::cos (angle);

        juce::Path petal;
        float pw = 4.0f * scale, ph = 9.0f * scale;
        petal.addEllipse (-pw * 0.5f, -ph * 0.5f, pw, ph);

        juce::AffineTransform t = juce::AffineTransform::rotation (angle)
                                    .translated (px, py);

        // colour transitions luminousGreen → petalPink with value
        auto col = AC::luminousGreen().interpolatedWith (AC::petalPink(), value);
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

void CorollaLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.setFont (serifFont.withHeight (label.getFont().getHeight()));
    g.setColour (AC::parchment().withAlpha (0.85f));
    g.drawFittedText (label.getText(), label.getLocalBounds(),
                      label.getJustificationType(), 2, 0.85f);
}

void CorollaLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h,
    bool /*isDown*/, int /*bx*/, int /*by*/, int /*bw*/, int /*bh*/, juce::ComboBox&)
{
    auto bounds = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (AC::deepEarth());
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (AC::barkBrown());
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    // small petal leaf glyph as dropdown arrow
    float ax = w - 16.0f, ay = h * 0.5f;
    juce::Path leaf;
    leaf.startNewSubPath (ax, ay - 4.0f);
    leaf.quadraticTo (ax + 5.0f, ay, ax, ay + 4.0f);
    leaf.quadraticTo (ax - 5.0f, ay, ax, ay - 4.0f);
    g.setColour (AC::petalPink());
    g.fillPath (leaf);
}

void CorollaLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font CorollaLookAndFeel::getLabelFont (juce::Label&)
{
    return serifFont;
}

juce::Font CorollaLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return serifFont.withHeight (12.0f);
}
