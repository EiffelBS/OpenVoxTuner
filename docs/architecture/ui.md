# User Interface

The GUI is a single JUCE `AudioProcessorEditor`
(`OpenVoxTunerAudioProcessorEditor`) composed of several dedicated components
in the `ui::` namespace. The layout is separated into a top banner, a tabbed
editor area (Live visualizer vs. Curve Editor), and bottom parameter blocks.

## Layout

- **Top banner**: plugin logo/version, the Live/Curve Editor `TabSwitch`
  (iPhone-style pill), global undo/redo, the A/B comparison buttons, a preset
  selector (`[<] [Combo] [>] [Save]`), the Options (gear) menu and update
  checker.
- **Tabbed area** (`juce::TabbedComponent`):
  - **Live / Auto** tab hosts the `PitchVisualizer`.
  - **Advanced / Curve Editor** tab hosts the `PitchCurveEditor` and the
    `ScaleKeyboardComponent`.
- **Bottom blocks**:
  - Correction block (Speed, Amount, Formant, correction-mode console switch,
    plus an "Advanced" expand/collapse banner).
  - Harmony block (type, gain, blend, attack, tone, shifted voices, formant).
  - Effects block (Reverb + Noise Gate + Upward Compressor).
  - Key/Scale controls (`keyBox`, `scaleBox`, custom-scale keyboard).

Layout is managed manually in `resized()`; block rectangles are cached in
`block1Bounds` … `block4Bounds`.

## PitchVisualizer (`ui::PitchVisualizer`)

Live display component (a `juce::Component` + `juce::Timer`) that draws:

- **The sung note name** (e.g. `F3`) in its header, and the **target note**
  (e.g. `-> F3`) when it differs.
- **The cents offset** (e.g. `-50 c`) with color coding:
  - green  (`|c| < 5`): in tune
  - yellow (`|c| < 15`): close
  - orange (`|c| < 35`): off
  - red    (`|c| >= 35`): clearly off
- **A vertical tuning meter** (needle on the cents offset, graduations at
  ±50 and ±100, Antares / Studio One style).
- **Current scale note lines** in the background (semi-transparent yellow)
  over 4 octaves (C2 → C6).
- **Input / output / harmony pitch traces** as time series (input = pink,
  output = green, harmony voices = blue), plus optional **waveform overlay**
  (Line / Mirror / Spectral FFT modes).

The displayed values come from `ovtdsp::describePitch()` in `NoteUtils.h`
(Hz → MIDI → note name + cents offset between the input and quantized pitch).
Tuning statistics (average cents, std-dev, % in tune within ±15¢) are
computed over a rolling window of 300 samples (~10 s at 30 fps).

Interactions: mouse-wheel and trackpad pinch-to-zoom, toolbar zoom/scroll/reset
buttons, an integrated vertical `PianoKeyboard`, and a right-click wrench menu.

## PitchCurveEditor (`ui::PitchCurveEditor`)

Interactive editor for `ovtdsp::PitchCurve`, connected to the processor via a
`Listener` (`pitchCurveChanged()`).

Interaction:

- **Drag** a point to move it vertically (change pitch).
- **Double-click** to add a point at the cursor position.
- **Right-click on a point** (or Alt+click) to delete it.
- **Right-click in empty space**: factory preset menu (default, spoken, lyric,
  rap, robot).
- **Snap to scale**: rounds points to the nearest note of the current scale.
- **Snap to grid / step mode** editing options; marquee multi-select, copy /
  paste, and undo/redo (its own `juce::UndoManager`).
- **Piano Roll mode**: a second editing metaphor that renders the same
  `PitchCurve` as discrete note rows (forces step mode on).
- **Playhead** display with auto-scroll; `setTimeSignature()` and a measures
  ruler. Also supports MIDI-file import and image export.

## PianoKeyboard (`ui::PianoKeyboard`)

A vertical piano keyboard (piano-roll style) shown to the left of the editor:

- White keys full width, black keys overlaid ~60% shorter.
- **Scale notes highlighted in yellow** (which notes are "allowed").
- Octave labels on the left of the C keys; Y axis is vertical (low notes at
  the bottom, high at the top).
- Default range C2 (MIDI 36) → C7 (MIDI 96).

## ScaleKeyboardComponent (`ui::ScaleKeyboardComponent`)

The **Custom scale editor**: a horizontal row of 12 `PianoKeyButton` toggles
(C → B, semitones 0..11), visible when Scale = "Custom". Each button binds to
an `AudioParameterBool` (`custom0`…`custom11`) via a `ButtonAttachment`, so the
12 notes can also be automated by the host. `setActiveScaleIntervals()` dims
notes outside the current scale in non-Custom modes.

## Theme (`ovt::OVTTheme`, `Theme::Dark` / `Theme::Light`)

Centralized color accessors in `ovt::currentTheme()` (default Dark), selected
by the `ui_theme` parameter. Provides the palette: `bgDark`, `bgPanel`,
`accent`, `accentSoft`, `text`, `textDim`, curve colors (`inputColour`,
`outputColour`, `harmonyColour`), piano colors, header colors, and a CPU-meter
theme. The visualizer / curve editor are **always rendered in dark mode**
(colors like `vizBg`, `rulerBg`, `curveGrid` are opaque and independent of the
active theme). Also exports the shared `drawWaveformOverlay()` used by both
the visualizer and the editor.

## Fonts (`ovt::OVTFonts`)

Centralized font system built on `createFont(size, bold)`:

- On **Windows** the typeface is hard-coded to **Segoe UI** (JUCE would
  otherwise resolve to the wider Verdana).
- On **macOS** the system default sans-serif is used with a `0.85` platform
  scale to compensate for the larger x-height.
- Named constants for consistent sizes: `fontTitle`, `fontNoteLarge`,
  `fontCents`, `fontComboBox`, `fontPopupMenu`, `fontPianoKey`, `fontRuler`,
  etc.

## i18n (`ovt::OVTLanguages`)

All translatable strings live in a keyed `std::unordered_map` per language.
`ovt::Language` supports **6 languages**:

| Language | Code | Display name |
|----------|------|--------------|
| English  | `en` | English      |
| French   | `fr` | Français     |
| German   | `de` | Deutsch      |
| Spanish  | `es` | Español      |
| Japanese | `ja` | 日本語        |
| Chinese  | `zh` | 中文          |

`t()` translates a key to `currentLanguage()`, falling back to English.
`languageDisplayName()` shows each language under its own name. The selected
language is stored in the `ui_language` parameter.

## LookAndFeel (`ui::OVTLookAndFeel`)

A `juce::LookAndFeel_V4` subclass customizing the plugin's widgets: rotary
sliders, combo boxes, toggle buttons, tooltips, tabs, popup menus, and linear
sliders. `refreshThemeColours()` is called whenever `ovt::currentTheme()`
changes so all components re-read the palette. The editor declares
`customLookAndFeel` as the **first** private member so it is destroyed last
(avoids dangling LookAndFeel pointers on teardown).
