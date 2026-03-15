# Anthesis — Per-Plugin Claude Code Prompts

Each prompt below is self-contained. Open a Claude Code window in the plugin's directory,
type `/anthesis` first to load the shared rules, then paste the plugin-specific prompt.

**Workflow per plugin:**
1. `cd ~/anthesis/<PluginName>`
2. Open Claude Code: `claude` (or your alias)
3. Type `/anthesis` — this loads the shared skill
4. Paste the plugin prompt below

---

## RESIN — Tape/Tube Saturation

```
Build the Resin plugin — a warm tape/tube saturation plugin for the Anthesis suite.
Working directory: ~/anthesis/Resin/

PLUGIN IDENTITY
Name: Resin
Type: Saturation / Harmonic Distortion
Seasonal particle: amber drips (particles arc downward slowly, warm amber, 5-8px)
Plugin code: Rsn1

DSP ALGORITHM
Implement a soft saturation chain:
1. Input gain stage (pre-drive)
2. Waveshaper: blend between soft clip (tanh) and asymmetric fold for tube character
   - Even/odd harmonic ratio controlled by "Age" parameter
   - At Age=0: mostly even harmonics (clean tube bloom)
   - At Age=1: more odd harmonics + asymmetric clipping (older, darker circuit)
3. Tone filter: a single-pole low-pass on the wet signal (cutoff 4-16kHz, controlled by Age)
4. Output gain with soft limiter
5. Wet/dry mix

PARAMETERS
- drive: 0.0-1.0, default 0.3 (pre-gain into waveshaper, 1-24dB range internally)
- age: 0.0-1.0, default 0.3 (harmonic character — see DSP above)
- tone: 0.0-1.0, default 0.7 (post-saturation tone, maps to LPF cutoff 4kHz-16kHz)
- output: 0.0-1.0, default 0.7 (output level, maps to -12dB to +6dB)
- mix: 0.0-1.0, default 1.0 (wet/dry)
- macro1: 0.0-1.0, default 0.0 (Flow — controls drive + harmonic blend together)
- macro2: 0.0-1.0, default 0.0 (Age macro — same as age param, drives entire circuit age)

MACRO BEHAVIOR
- macro1 (Flow): maps to drive (0→0.8) and simultaneously blends harmonic character toward tube bloom
- macro2 (Age): maps directly to age parameter, also pulls tone down slightly at high values

PRESETS (implement as named presets in APVTS)
- "Fresh Resin" — drive 0.2, age 0.1, tone 0.9, mix 1.0 (clean, barely there)
- "Hiroshi Yoshimura" — drive 0.35, age 0.3, tone 0.75, mix 0.85 (warm, elemental)
- "Cocteau Twins" — drive 0.45, age 0.2, tone 0.85, mix 0.9 (bright harmonic shimmer)
- "Grouper" — drive 0.6, age 0.7, tone 0.4, mix 0.75 (dark, smeared, devotional)
- "Boards of Canada" — drive 0.5, age 0.65, tone 0.5, mix 0.8 (warm tape, memory)
- "Aged Circuit" — drive 0.8, age 0.9, tone 0.35, mix 0.7 (heavy, dark, old transformer)

UI LAYOUT (600x400)
Top section (full width): Plugin title "Resin" + preset dropdown
Middle section: Waveform display ("Amber Garden") — shows input/output comparison as two overlaid vines
Bottom section: 5 regular knobs (Drive, Age, Tone, Output, Mix) + 2 macro knobs (Flow, Age)
Macro knobs are larger, positioned left and right of center
All knobs use the botanical style from the skill

Apply the amber drip particle personality.
```

---

## MYCELIUM — Natural Space Reverb

```
Build the Mycelium plugin — an algorithmic reverb that evokes natural physical spaces.
Working directory: ~/anthesis/Mycelium/

PLUGIN IDENTITY
Name: Mycelium
Type: Algorithmic Reverb
Seasonal particle: rising spores (particles drift upward slowly, slight spiral path)
Plugin code: Myc1

DSP ALGORITHM
Implement a Schroeder/Moorer-style algorithmic reverb:
1. Pre-delay (0-100ms)
2. 8 parallel comb filters with prime-number delay times (tuned for space character)
3. 4 allpass diffusers in series
4. High/low shelf damping filters on the feedback path
5. Stereo widening on the output (Haas effect, 0-20ms offset)

Canopy macro morphs the space character by adjusting the comb filter delay times:
- Canopy=0 (greenhouse): short delays 20-50ms range, warm damping, dense early reflections
- Canopy=0.5 (forest): medium delays 40-80ms, moderate damping
- Canopy=1 (cave): long delays 60-120ms, bright tail, less HF damping

PARAMETERS
- canopy: 0.0-1.0, default 0.4 (space morphing — greenhouse→forest→cave)
- preddelay: 0.0-1.0, default 0.2 (pre-delay, maps to 0-80ms)
- decay: 0.0-1.0, default 0.5 (reverb tail length, maps to 0.3s-12s)
- damping: 0.0-1.0, default 0.5 (HF rolloff in feedback path)
- width: 0.0-1.0, default 0.7 (stereo width of the reverb tail)
- mix: 0.0-1.0, default 0.35 (wet/dry)
- macro1: 0.0-1.0, default 0.0 (Canopy — space character morph)
- macro2: 0.0-1.0, default 0.0 (Root Depth — pre-delay + decay + sub resonance)

MACRO BEHAVIOR
- macro1 (Canopy): morphs all comb filter times and damping together
- macro2 (Root Depth): increases pre-delay + decay time + adds low-frequency resonance

PRESETS
- "Morning Greenhouse" — canopy 0.05, decay 0.4, damping 0.7, mix 0.3
- "Ichiko Aoba" — canopy 0.3, decay 0.7, damping 0.6, mix 0.25 (intimate, natural)
- "Hiroshi Yoshimura" — canopy 0.5, decay 1.5, damping 0.45, mix 0.35 (Music for Nine Post Cards)
- "Julianna Barwick" — canopy 0.45, decay 2.0, damping 0.5, mix 0.45 (devotional wash)
- "Sigur Rós" — canopy 0.7, decay 4.0, damping 0.35, mix 0.5 (vast, cold)
- "Deep Cave" — canopy 1.0, decay 8.0, damping 0.2, mix 0.4 (stone, resonant, ancient)

UI LAYOUT (600x400)
Top: Title "Mycelium" + preset dropdown
Center: Large circular display ("Mycelium Network") — visualizes reverb density as a web
  of branching bezier paths that grow denser with decay, pulsing with the signal
Bottom: Knobs for Pre-delay, Decay, Damping, Width, Mix + 2 macro knobs (Canopy, Root Depth)

Apply the rising spore particle personality.
```

---

## AUREOLE — Chorus & Modulation

```
Build the Aureole plugin — a lush chorus/modulation effect.
Working directory: ~/anthesis/Aureole/

PLUGIN IDENTITY
Name: Aureole
Type: Chorus / Ensemble Modulation
Seasonal particle: firefly drift (particles move in sine curves, blink at 1-3Hz)
Plugin code: Aur1

DSP ALGORITHM
Implement a multi-voice chorus:
1. 4 delay lines (per channel = 8 total for stereo)
2. Each delay line has an independent LFO (slightly detuned phases: 0°, 90°, 180°, 270°)
3. LFO type: sine with subtle random walk added (organic irregularity)
4. Delay time range: 5-35ms base, modulated ±0-15ms by LFO
5. Slight saturation on the wet sum (softens and warms)
6. Stereo spread: alternate voices panned L/R

Drift macro controls rate + depth together on a musical curve:
- Drift=0: rate 0.2Hz, depth 1ms — barely perceptible shimmer
- Drift=0.5: rate 0.5Hz, depth 5ms — classic ensemble
- Drift=1: rate 0.1Hz, depth 12ms — slow, wide, lush

PARAMETERS
- rate: 0.0-1.0, default 0.3 (LFO rate, maps to 0.05-3Hz)
- depth: 0.0-1.0, default 0.4 (modulation depth, maps to 0-15ms)
- voices: 1-4 integer, default 4 (active chorus voices)
- spread: 0.0-1.0, default 0.8 (stereo width of voices)
- warmth: 0.0-1.0, default 0.4 (wet signal tone + subtle saturation)
- mix: 0.0-1.0, default 0.5 (wet/dry)
- macro1: 0.0-1.0, default 0.0 (Drift — rate + depth musical curve)
- macro2: 0.0-1.0, default 0.0 (Warmth — wet tone character)

MACRO BEHAVIOR
- macro1 (Drift): non-linear mapping — rate and depth follow a curve that keeps the
  ensemble musical at all positions
- macro2 (Warmth): controls wet LPF cutoff + wet saturation amount together

PRESETS
- "Morning Dew" — drift 0.15, warmth 0.3, mix 0.35 (barely there, shimmer)
- "Cocteau Twins" — drift 0.4, warmth 0.7, spread 0.9, mix 0.6 (lush, warm glass)
- "Slowdive" — drift 0.35, warmth 0.5, mix 0.55 (wall of shimmer)
- "My Bloody Valentine" — drift 0.55, warmth 0.8, mix 0.7 (thick, dense, warm)
- "Laraaji" — drift 0.2, warmth 0.4, mix 0.4 (zither-like shimmer)
- "Deep Ensemble" — drift 0.7, warmth 0.6, voices 4, mix 0.65 (orchestral width)

UI LAYOUT (600x400)
Top: Title "Aureole" + preset dropdown
Center: Circular halo display — draws the 4 LFO positions as orbiting light traces around
  a center point. The halo pulses with the modulation rate. Call it "Halo".
Bottom: Rate, Depth, Voices, Spread, Warmth, Mix knobs + 2 macro knobs (Drift, Warmth)

Apply the firefly particle personality.
```

---

## POLLEN — Soft Granular Processor

```
Build the Pollen plugin — a soft granular cloud processor.
Working directory: ~/anthesis/Pollen/

PLUGIN IDENTITY
Name: Pollen
Type: Granular Processor
Seasonal particle: horizontal pollen drift (tiny 2-3px, left-to-right, yellow-green)
Plugin code: Pln1

DSP ALGORITHM
Implement a basic granular processor:
1. Circular buffer (4 seconds at 48kHz)
2. Variable number of simultaneous grains (4-32)
3. Each grain: position in buffer, playback speed, pitch shift, envelope (raised cosine), pan
4. Grain parameters driven by scatter + float macros:
   - position: read position in buffer with random offset controlled by scatter
   - size: grain duration 20-400ms
   - density: grains per second 2-40
   - pitch: random pitch deviation ±0 to ±24 semitones
   - pan: random stereo spread
5. Raised cosine grain envelope (no clicks)
6. Output: sum of all grains, normalized

Scatter macro: controls position randomness + density together
Float macro: controls pitch deviation + time stretch ratio together

PARAMETERS
- scatter: 0.0-1.0, default 0.3 (position randomness + density)
- float: 0.0-1.0, default 0.2 (pitch + time looseness)
- grain_size: 0.0-1.0, default 0.5 (grain duration, 20-400ms)
- density: 0.0-1.0, default 0.6 (grains per second, 2-40)
- spread: 0.0-1.0, default 0.5 (stereo randomization)
- mix: 0.0-1.0, default 0.6 (wet/dry)
- macro1: 0.0-1.0, default 0.0 (Scatter)
- macro2: 0.0-1.0, default 0.0 (Float)

MACRO BEHAVIOR
- macro1 (Scatter): increases position jitter AND density simultaneously. Low = smooth cloud.
  High = sparse, individual grains audible and separated.
- macro2 (Float): increases pitch deviation AND loosens grain timing. High = fully adrift.

PRESETS
- "Morning Cloud" — scatter 0.15, float 0.1, mix 0.5 (smooth, ambient texture)
- "Grouper" — scatter 0.25, float 0.3, mix 0.6 (blurred, devotional)
- "Julianna Barwick" — scatter 0.2, float 0.15, density 0.8, mix 0.65 (dense haze)
- "Huerco S." — scatter 0.5, float 0.4, mix 0.55 (dusted, lo-fi cloud)
- "Kaitlyn Aurelia Smith" — scatter 0.35, float 0.6, mix 0.6 (organic, pitch-drifting)
- "Dispersal" — scatter 0.8, float 0.7, mix 0.5 (fully scattered, individual grains)

UI LAYOUT (600x400)
Top: Title "Pollen" + preset dropdown
Center: Particle field display ("Pollen Cloud") — renders active grains as soft dots
  positioned by their buffer position (x) and pitch deviation (y). Dots fade in/out
  with grain envelopes. Should feel like watching pollen through a window.
Bottom: Scatter, Float, Grain Size, Density, Spread, Mix + 2 macro knobs

Apply the horizontal pollen drift particle personality.
```

---

## COROLLA — Spectral Bloom

```
Build the Corolla plugin — a spectral harmonic bloom effect.
Working directory: ~/anthesis/Corolla/

PLUGIN IDENTITY
Name: Corolla
Type: Spectral / Harmonic Exciter
Seasonal particle: petal fall (ellipses 8x4px, random rotation, slow spiral fall)
Plugin code: Cor1

DSP ALGORITHM
Implement a harmonic exciter with slow bloom envelope:
1. FFT analysis (2048 point, overlap-add)
2. Harmonic generation: for each detected spectral peak, add harmonics (2nd, 3rd, 4th)
   with decreasing amplitude. Bloom macro controls how many harmonics and their level.
3. Spectral smear: blur adjacent FFT bins for that "spread" quality
4. Bloom envelope: a slow attack (0.1s-10s) that brings the harmonic content in gradually.
   Unfurl macro controls this attack time.
5. High-frequency tilt: the added harmonics are filtered to sit above the original material
6. IFFT reconstruction + mix with dry signal

Alternative implementation if FFT is complex: use a parallel bank of 6 bandpass resonators
tuned to natural harmonic intervals, each with their own slow envelope, summed to wet signal.
Either approach is acceptable.

PARAMETERS
- bloom: 0.0-1.0, default 0.3 (harmonic content amount)
- unfurl: 0.0-1.0, default 0.5 (bloom attack time, 0.1s-10s)
- shimmer: 0.0-1.0, default 0.2 (adds a subtle pitch-up shimmer to the harmonics)
- tilt: 0.0-1.0, default 0.5 (spectral tilt of added content, bright to dark)
- mix: 0.0-1.0, default 0.4 (wet/dry)
- macro1: 0.0-1.0, default 0.0 (Bloom — harmonic amount)
- macro2: 0.0-1.0, default 0.0 (Unfurl — speed of bloom)

MACRO BEHAVIOR
- macro1 (Bloom): controls harmonic generation amount + spectral spread together
- macro2 (Unfurl): sets bloom attack time — turn up for slow, glacial reveals

PRESETS
- "First Light" — bloom 0.2, unfurl 0.3, shimmer 0.1, mix 0.3 (subtle, lifting)
- "Brian Eno" — bloom 0.4, unfurl 0.7, shimmer 0.2, mix 0.35 (slow harmonic wash)
- "Harold Budd" — bloom 0.35, unfurl 0.5, shimmer 0.1, mix 0.3 (romantic, impressionist)
- "Sigur Rós" — bloom 0.6, unfurl 0.8, shimmer 0.4, mix 0.45 (full shimmer bloom)
- "Nils Frahm" — bloom 0.3, unfurl 0.2, shimmer 0.05, mix 0.25 (piano overtone lift)
- "Full Corolla" — bloom 0.9, unfurl 0.9, shimmer 0.5, mix 0.5 (maximum bloom)

UI LAYOUT (600x400)
Top: Title "Corolla" + preset dropdown
Center: Petal display ("Bloom") — draws a flower with petals that scale outward based on
  the current bloom amount. More bloom = more petals open. At 0 = tightly closed bud.
  At max = full flower. The petals are bezier curves, soft luminous green with amber tips.
Bottom: Bloom, Unfurl, Shimmer, Tilt, Mix knobs + 2 macro knobs

Apply the petal fall particle personality.
```

---

## ALLUVIUM — Organic Tape Delay

```
Build the Alluvium plugin — an organic tape-style delay with degradation.
Working directory: ~/anthesis/Alluvium/

PLUGIN IDENTITY
Name: Alluvium
Type: Tape Delay with Wow/Flutter
Seasonal particle: edge rain (thin lines fall from top, fade before bottom)
Plugin code: Alv1

DSP ALGORITHM
Implement a tape delay with organic degradation:
1. Delay line (up to 2 seconds)
2. Tempo-syncable delay time (1/32 to 2 bars, or free 10ms-2000ms)
3. Feedback path includes:
   a. Wow: slow pitch drift on playback (sine LFO 0.3-1Hz, depth 0-30 cents)
   b. Flutter: faster irregular pitch modulation (6-12Hz, depth 0-10 cents)
   c. HF rolloff: low-pass filter in feedback (simulates tape HF loss over generations)
   d. Subtle saturation on feedback signal
4. Ping-pong mode: alternates L/R on each repeat
5. Wet/dry mix

Sediment macro: controls wow depth + flutter + HF rolloff + feedback saturation together
Current macro: controls how rhythmically anchored the delay feels (tightens/loosens timing)

PARAMETERS
- time: 0.0-1.0, default 0.5 (delay time, free or tempo-synced)
- feedback: 0.0-0.95, default 0.4 (feedback amount — never allow >=1.0)
- sediment: 0.0-1.0, default 0.3 (degradation: wow + flutter + HF loss + saturation)
- current: 0.0-1.0, default 0.3 (rhythmic anchor — low=tight, high=floating)
- pingpong: bool, default false
- mix: 0.0-1.0, default 0.4 (wet/dry)
- macro1: 0.0-1.0, default 0.0 (Sediment)
- macro2: 0.0-1.0, default 0.0 (Current)

MACRO BEHAVIOR
- macro1 (Sediment): increases all degradation parameters together in a musical curve
- macro2 (Current): at low values, delay is tight to tempo. At high values, gradually detunes
  the delay time by a small random drift (±5ms variance), making repeats "float"

PRESETS
- "Clear Stream" — sediment 0.1, current 0.1, feedback 0.3, mix 0.35 (clean, rhythmic)
- "Ryuichi Sakamoto" — sediment 0.3, current 0.4, feedback 0.4, mix 0.4 (warm, drifting)
- "Boards of Canada" — sediment 0.55, current 0.5, feedback 0.5, mix 0.45 (tape ghost)
- "Huerco S." — sediment 0.7, current 0.6, feedback 0.45, mix 0.4 (heavy degradation)
- "Grouper" — sediment 0.5, current 0.7, feedback 0.55, mix 0.5 (dissolving repeats)
- "Deep Silt" — sediment 0.9, current 0.8, feedback 0.6, mix 0.45 (heavily damaged)

UI LAYOUT (600x400)
Top: Title "Alluvium" + preset dropdown
Center: Waveform display ("Stream") — shows the delay buffer as a flowing ribbon that
  degrades in visual clarity toward the right (more sediment = blurrier trail).
  At high Sediment values, the ribbon visually crumbles at its edges.
Bottom: Time, Feedback, Sediment, Current, Mix knobs + Ping-Pong toggle + 2 macro knobs

Apply the edge rain particle personality. Rain lines appear at top edge, fall slowly,
fade to nothing before reaching the bottom third of the plugin.
```

---

## WORKFLOW REMINDER

```
For each plugin:
1. cd ~/anthesis/<PluginName>
2. claude (open Claude Code)
3. /anthesis          ← loads shared rules
4. paste plugin prompt above
5. let it build, test in Ableton
6. cd ~/anthesis && git add <PluginName>/ && git commit -m "feat: Resin complete"
7. git push
```
