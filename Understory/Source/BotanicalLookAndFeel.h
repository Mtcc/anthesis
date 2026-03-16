#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// shared color constants — Understory edition: forest floor, deep green, earth tones
namespace UC
{
    inline juce::Colour deepForest()    { return juce::Colour (0xFF050C08); }
    inline juce::Colour forestFloor()   { return juce::Colour (0xFF0D1A0F); }
    inline juce::Colour moss()          { return juce::Colour (0xFF1A3020); }
    inline juce::Colour canopy()        { return juce::Colour (0xFF2A4A32); }
    inline juce::Colour fernGreen()     { return juce::Colour (0xFF4A7A50); }
    inline juce::Colour dewGreen()      { return juce::Colour (0xFF7ABF88); }
    inline juce::Colour mistWhite()     { return juce::Colour (0xFFD8E8DC); }
    inline juce::Colour earthBrown()    { return juce::Colour (0xFF3A2818); }
    inline juce::Colour barkDark()      { return juce::Colour (0xFF221810); }
    inline juce::Colour lichenGold()    { return juce::Colour (0xFFB8A055); }
    inline juce::Colour parchment()     { return juce::Colour (0xFFE0D8C8); }
    inline juce::Colour shadowGreen()   { return juce::Colour (0xFF0A1208); }
}

class UnderstoryLookAndFeel : public juce::LookAndFeel_V4
{
public:
    UnderstoryLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    void drawLabel (juce::Graphics&, juce::Label&) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;

    // draw leaf/frond overlay for macro knobs
    void drawMacroLeaves (juce::Graphics&, juce::Rectangle<float> bounds,
                          float value, float phaseOffset);

private:
    juce::Font serifFont { juce::FontOptions ("Georgia", 12.0f, juce::Font::plain) };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UnderstoryLookAndFeel)
};
