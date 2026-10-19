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
  - **Effects**: Gate, Compress, Doubler, Formant, Output Limit (with its meter strip), De-Esser, Saturation, Delay. Column 1 is Gate over the Compressor; **column 2 is Doubler over Formant**, and that pair IS the folded set — opening adds column 3, the Output Limit + meter over the De-Esser, then column 4 for Saturation and column 5 for the Delay. Limit and Formant exchanged places by design: the dial you shape a voice with stays on screen when the frame is folded, the ceiling control moves to the open zone. Folding hides the CONTROL, never the processing — `limiter_enable` keeps guarding every sample and the meter refresh keeps running while its widget is hidden. Width is pure cell count (2 folded / 5 open), so a swap changes no width; the fold is deliberately not persisted.
    - **Activity dots, one per COMPONENT, carried by its knob**: a dot reports the path its processor works on, so the unit is the component, never the column — column 3 proves it, with a master-side ceiling over a lead-side De-Esser. Every dot lives **inside the knob that IS the component's amount** (`ebs::ActivityKnob`, eiffelbs-ui v0.17.2): the dial paints exactly as a plain knob does and the disc goes on top of its centre, which the shared painter leaves free because the pointer line starts 8 px out and grows outward. Thirteen knobs carry one each — Gate, Compressor, Doubler, Formant, De-Esser and the Delay Mix read the **lead** tap, the Output Limit and the Saturation Drive read the **master** tap, and the **four reverb amounts** read their own voice: both Lead amounts (Room and Hall) carry the lead tap and both Harmony amounts carry the harmony tap. A voice sends the same signal to two rooms, so putting the dot on one amount only would read as "this other amount carries nothing" — every amount of the same voice shows the same level, on purpose. There is therefore **no activity strip anywhere in the row**: the reserve that used to pay for it went back to the window (208 -> 196 px, 12 px of curve editor), and every knob still reaches its full 54 px — the folded Reverb rooms even gained 6 px (48 -> 54) because their strip is gone too. The Ducking column carries none: it is a return ramp, not a voice. The dots are fed by the editor timer from the processor's atomic snapshot at ~30 Hz, so nothing is ever painted from the audio thread, and no dot is a parameter (no APVTS entry, no state attribute, no MIDI-learn name).
  - **Reverb**: two shared rooms (ROOM, short; HALL, long) and two voice columns. A voice switch reads **Lead** or **Harmony**; the two amounts under it read **Room** and **Hall**, the rooms they feed. The knob IS the send — there is no separate send control, the dry voice is never attenuated, and a send at 0 is silent — and the switch keeps the knobs' values, so switching a voice off and back on needs no re-dial. Both switches start **off**, while the knobs already carry the "ready to sing" pair (`lead_room_send` 15 % + `lead_hall_send` 45 %, harmony 10 % + 30 % from `ReverbTuning.h`), so the first switch-on applies a balanced space instead of silence. `reverb_duck_depth` is one sidechain ducker on both returns, keyed by the corrected lead, so it belongs to this block. Both amounts of a voice carry its dot: Room and Hall send the same voice, so the two dots report the same level and neither knob looks like it carries no signal. The **Harmony** dots are where the harmony path becomes visible (those two sends are the only processing that path owns) and the **Lead** dots report the lead's sends. The Ducking knob carries none — it is a return ramp, not a voice. Folded the frame is ONE column: the Lead switch, Room and Hall. Open it is three columns: Lead, Harmony, Ducking alone. Every column floor is the measured width of its own switch name in the loaded language (`reverbHeaderNeed()` = 18 px power strip + label width at the 14 pt toggle font + 2 px edge), so no locale clips a header. Measured across all six locales (eiffelbs-ui smoke v0.15.15): lead needs at most 43 px, harmony at most 67 px (Spanish "Armonías"), ducking at most 59 px — every one rides the fixed 74 px cell floor, so a folded Reverb frame stays ONE fixed column in every language; ideographs ask for LESS than Latin (Japanese 54, Chinese 34), because two and three glyphs carry what four to eight latin letters say. The two switches are what make the legacy one-knob experience usable again: ON applies the balanced pair immediately, so a singer never has to dial a reverb to start singing.
  - **Root/Scale**: key and scale combos, mini keyboard, Key/Scale Detection power.
  - **Harmony**: type, voices and toggles; Volume / Blend / Formant / Attack knobs live in the folded zone. Both zones ride one shared cell grammar, the same `placeLabelledKnob()` the Reverb sends and Saturation use: a 14 px label strip, a 2 px gap, one knob centred inside `effKnobD`, rows split by the band's own 8 px gap — so an open Harmony dial sits on the same row lines, with the same diameter, as an Effects dial in the same window. The main (always-visible) zone keeps fixed row heights for what each row carries, and the height those rows leave over becomes the spacing *between* them (`mainGap`, between 3 px and 12 px), so the six controls read as five breathing groups instead of a glued stack; before this rule the leftover height sat unused at the bottom edge and every row touched its neighbour.
    - **The two zone widths are measured policy, not percentages** (`Source/ui/OVTHarmonyLayout.h`, read by the layout *and* by `test/ui/HarmonyLayoutTest.cpp`). The **main zone** is exactly as wide as its widest row needs — the harmony type combo's longest entry plus the arrow, or the use-voice row (caption + switch + tone swatch) — with a floor that only guards the fixed rows, so the folded frame is never wider than its content and no translated entry ever truncates: measured at the combo font the longest entry is 129 px (EN), 123 (FR), **149 (DE)**, 132 (ES), 103 (JA), 101 (ZH), which puts the zone at 153 / 147 / 173 / 156 / 140 / 140 px where it used to be a fixed 196. The **expanded zone** takes everything left over, and it is sized so two dials read as two controls: each column is a full-size 54 px dial plus 12 px of air on both sides, with 16 px between the columns — 40 px of clearance between the knobs, against 16 px before. When the window is too narrow for that, the frame shrinks toward a floor that still holds two 40 px dials, and the dials themselves are bounded by their column's width, so a squeezed zone shrinks a knob instead of letting two overlap.

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

## Toolbar tempo field (standalone)

The tempo register (`OpenVoxTunerAudioProcessor::setBpm` / `getBpm`, clamped 20..400) is what the
standalone **timeline** advances at, what the Curve Editor's bar grid is drawn on, and what the
tempo-synced **Delay** reads — a DAW replaces it with the host tempo and an ARA source with its
tempo map, so the field is standalone-only, like the Options menu's Tempo submenu. That submenu
carries the usual values (60..180 in tens) and ticks the current one; the toolbar field next to
**Measures** accepts ANY value (click, type `137`, Enter), and both write the same register, so the
field follows a menu pick and the menu tick follows a typed value. The register is persisted as the
state's `bpm` root attribute — it used to be forgotten on every reload — and deliberately does NOT
ride a plugin preset (a preset changes a sound, not the session's tempo).

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
