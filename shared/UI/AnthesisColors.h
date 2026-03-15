#pragma once
#include <JuceHeader.h>

namespace Anthesis
{
    // Enchanted forest at magic hour
    namespace Colors
    {
        // Backgrounds
        constexpr uint32_t DeepForest   = 0xFF0A0F0A; // near-black forest green
        constexpr uint32_t Moss         = 0xFF1A2E1A; // depth layer
        constexpr uint32_t CanopyGreen  = 0xFF2D5A3D; // section backgrounds

        // Accents
        constexpr uint32_t LuminousGreen = 0xFF7AEFB2; // primary: waveform, active
        constexpr uint32_t WarmAmber    = 0xFFE8A849; // light sources, indicator dots
        constexpr uint32_t GoldenHour   = 0xFFF5C96A; // highlights, peaks
        constexpr uint32_t PetalPink    = 0xFFE8A0B4; // secondary accent
        constexpr uint32_t LavenderMist = 0xFFB8A0D4; // ethereal elements

        // Structure
        constexpr uint32_t Parchment   = 0xFFF0E6D3; // text, labels
        constexpr uint32_t BarkBrown   = 0xFF4A3728; // borders, knob rings
        constexpr uint32_t DeepEarth   = 0xFF2A1F18; // knob bodies

        // Helpers
        inline juce::Colour deepForest()   { return juce::Colour(DeepForest); }
        inline juce::Colour luminousGreen(){ return juce::Colour(LuminousGreen); }
        inline juce::Colour warmAmber()    { return juce::Colour(WarmAmber); }
        inline juce::Colour goldenHour()   { return juce::Colour(GoldenHour); }
        inline juce::Colour parchment()    { return juce::Colour(Parchment); }
        inline juce::Colour barkBrown()    { return juce::Colour(BarkBrown); }
        inline juce::Colour deepEarth()    { return juce::Colour(DeepEarth); }
        inline juce::Colour petalPink()    { return juce::Colour(PetalPink); }
        inline juce::Colour lavenderMist() { return juce::Colour(LavenderMist); }
    }
}
