# Anthesis

*A six-plugin VST3/AU suite for organic, botanical sound design.*

Anthesis is the precise moment a flower is fully open and functional. This suite lives in that moment — warm, luminous, alive. Built with JUCE on Apple Silicon, designed for Ableton.

---

## Plugins

| Plugin | Type | Character |
|--------|------|-----------|
| **Mycelium** | Reverb | Natural space — forest, cave, greenhouse. The hidden connective tissue beneath sound. |
| **Resin** | Saturation | Warm tape/tube character. Amber-colored, organic, preserving. |
| **Aureole** | Chorus / Modulation | The luminous ring around a light source seen through leaves. Shimmer, halo, living motion. |
| **Pollen** | Granular | Soft cloud textures, ambient scatter. Airborne and adrift. |
| **Corolla** | Spectral / Harmonic | Harmonic bloom that unfolds slowly. Sounds open and evolve. |
| **Alluvium** | Organic Delay | Tape-style delay with wow/flutter and pitch drift. Sediment carried downstream. |

---

## Aesthetic

Studio Ghibli forests. Makoto Shinkai light. Mushishi. Enchanted forest at twilight.

Every UI element is procedurally drawn in JUCE — no imported textures. The organic feel comes from bezier curves, gradient fills, layered translucent drawing, and slow animation. Each plugin breathes.

---

## Building

Requires JUCE as a git submodule:

```bash
git submodule update --init --recursive
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Targets: VST3, AU (macOS). Apple Silicon.

---

## Color Palette

| Name | Hex |
|------|-----|
| Deep forest | `#0a0f0a` |
| Luminous green | `#7aefb2` |
| Warm amber | `#e8a849` |
| Golden hour | `#f5c96a` |
| Petal pink | `#e8a0b4` |
| Lavender mist | `#b8a0d4` |
| Parchment | `#f0e6d3` |
| Bark brown | `#4a3728` |

---

*Part of the Anthesis Suite. Built with JUCE + Claude Code.*
