#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// shared color constants (mirrors AnthesisColors.h for standalone use)
namespace AC
{
    inline juce::Colour deepForest()    { return juce::Colour (0xFF0A0F0A); }
    inline juce::Colour moss()          { return juce::Colour (0xFF1A2E1A); }
    inline juce::Colour canopyGreen()   { return juce::Colour (0xFF2D5A3D); }
    inline juce::Colour luminousGreen() { return juce::Colour (0xFF7AEFB2); }
    inline juce::Colour warmAmber()     { return juce::Colour (0xFFE8A849); }
    inline juce::Colour goldenHour()    { return juce::Colour (0xFFF5C96A); }
    inline juce::Colour petalPink()     { return juce::Colour (0xFFE8A0B4); }
    inline juce::Colour lavenderMist()  { return juce::Colour (0xFFB8A0D4); }
    inline juce::Colour parchment()     { return juce::Colour (0xFFF0E6D3); }
    inline juce::Colour barkBrown()     { return juce::Colour (0xFF4A3728); }
    inline juce::Colour deepEarth()     { return juce::Colour (0xFF2A1F18); }
}

class AlluviumLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AlluviumLookAndFeel();

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

    // draw macro knob with lavender petal bloom overlay
    void drawMacroPetals (juce::Graphics&, juce::Rectangle<float> bounds,
                          float value, float phaseOffset);

private:
    juce::Font serifFont { juce::FontOptions ("Georgia", 12.0f, juce::Font::plain) };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlluviumLookAndFeel)
};
