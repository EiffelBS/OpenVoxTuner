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
- **Bottom row**: one `ebs::FrameBand`, left to right: Correction, Key & Scale, Effects, Reverb, Harmony. Every block is a component that paints itself (`ebs::ExpansibleFrame` for the foldable ones — Correction, Effects, Reverb, Harmony — and `ebs::FramedBody` for Key & Scale); the band lays each frame at its preferred width, gives spare room only to blocks marked growable, and scrolls below a hard floor. All widths come from one model, `updateBottomBandWidths()`.
  - **Title spine**: every bottom-row frame carries an `ebs::FrameTitleBand` on its left edge — a vertical band naming the block, reading bottom-to-top with glyphs facing left (book-spine convention), hugging the rounded corner of the frame through the shared `ebs::frameCornerRadius`. The label is UPPERCASE, bold, letters tracked apart by `ebs::kBandLetterSpacing` (1.2 px between glyphs) and drawn at 50 % opacity (`ebs::kBandTextAlpha`) — a caption, never an instrument; upper casing CJK is a no-op, so one rule serves every locale. Captions carry **no accents**: JUCE `toUpperCase()` only casing-changes ASCII letters (FR `Réverbération` would upper to `RéVERBéRATION`), so the shared helper `ebs::bandCaps()` maps accented codepoints — both cases — to plain capitals, writes ß as SS, and lets ligatures, CJK and Cyrillic pass through untouched. The same helper drives the paint AND the thickness measurement, so every consumer measures exactly what is drawn. Tracking stretches the name ALONG the spine only, never across it, which is why the fixed width stays measured from glyphs: `ebs::FrameTitleBand::bandWidth()`, 17 px = 13 px glyph thickness at 13 pt plus two pixels clear at each edge, for every locale; a longer translated name pays in length along the spine, shrinking glyphs from 13 pt to 9 pt (9 px thick), never in width. Each band is a child of the frame it names, so it travels with that frame when the row re-lays out, and its text rides the i18n refresh list (`translatableBands` + `refreshLabels()`), like every label. The content area of each frame removes this strip before placing widgets (`interior()`, then `removeFromLeft`).
  - **Correction**: Speed, Amount, the Modern/Transparent console switch; "Advanced" opens the vibrato / humanize / voice-type zone.
  - **Effects**: Gate, Compress, Doubler, Formant, Output Limit (with its meter strip), De-Esser, Saturation. Column 1 is Gate over the Compressor; **column 2 is Doubler over Formant**, and that pair IS the folded set — opening adds column 3, the Output Limit + meter over the De-Esser, and column 4 for Saturation. Limit and Formant exchanged places by design: the dial you shape a voice with stays on screen when the frame is folded, the ceiling control moves to the open zone. Folding hides the CONTROL, never the processing — `limiter_enable` keeps guarding every sample and the meter refresh keeps running while its widget is hidden. Width is pure cell count (2 folded / 4 open), so the swap changes no width; the fold is deliberately not persisted.
  - **Reverb**: the two rooms, Lead Reverb and Harmony Reverb. Their widget labels read **Lead** and **Harmony**, and the ducking knob is labelled **Ducking**; `duck_depth` is consumed only by the shared room's return, so it belongs to this block and not to a send. Folded they stack in ONE column — Lead header + mix knob on top, Harmony header + mix knob below. Open they become three columns: column 1 Lead + Lead Send, column 2 Harmony + Harmony Send, column 3 Ducking alone. Every column floor is the measured width of its own label in the loaded language (`reverbHeaderNeed()` = 18 px power strip + label width at the 14 pt toggle font + 2 px edge), so no locale clips a header. Measured across all six locales (eiffelbs-ui smoke v0.15.15): lead needs at most 43 px, harmony at most 67 px (Spanish "Armonías"), ducking at most 59 px — every one rides the fixed 74 px cell floor, so a folded Reverb frame stays ONE fixed column in every language; ideographs ask for LESS than Latin (Japanese 54, Chinese 34), because two and three glyphs carry what four to eight latin letters say.
  - **Root/Scale**: key and scale combos, mini keyboard, Key/Scale Detection power.
  - **Harmony**: type, voices and toggles; Volume / Blend / Formant / Attack knobs live in the folded zone. Both zones ride one shared cell grammar, the same `placeLabelledKnob()` the Reverb sends and Saturation use: a 14 px label strip, a 2 px gap, one knob centred inside `effKnobD`, rows split by the band's own 8 px gap — so an open Harmony dial sits on the same row lines, with the same diameter, as an Effects dial in the same window. The main (always-visible) zone keeps fixed row heights for what each row carries, and the height those rows leave over becomes the spacing *between* them (`mainGap`, between 3 px and 12 px), so the six controls read as five breathing groups instead of a glued stack; before this rule the leftover height sat unused at the bottom edge and every row touched its neighbour.

Layout is host-owned: `resized()` reads each frame's own interior, and a fold re-states every block width (`updateBottomBandWidths()`) before the row is laid out again. The Reverb and Harmony folds are persisted as root attributes `reverbExpanded` / `harmonyExpanded` (the Correction advanced zone as `advancedExpanded`); the Effects fold is deliberately not. Every expansible frame starts FOLDED, including a state file written before the fold existed: reading an absent attribute defaults to folded, so a fresh install and an old session open the same row. Each block's stated width includes its title spine (`ebs::FrameTitleBand::bandWidth()`), which is why the model adds it once for every frame in all six locales.

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
