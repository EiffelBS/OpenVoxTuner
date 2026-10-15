# Changelog — 2026-10-15

## Feature — De-Esser: sibilance control on the corrected lead (`deess_*`)

### Context
A de-esser tames the burst energy of the "s", "t", "f" consonants — the 5–8 kHz
region where a vocal gets harsh, and where upward compression does the most
damage: it lifts every quiet "s" to the same level as the vowels. OpenVoxTuner had
no control for it.

The design rule from the card is a band-limited detector: **only the high band is
heard**. A full-band detector would let plosives, breaths and loud low notes duck
the whole voice, which is the classic de-esser misuse. So the module is a band-pass
front-end (2-pole high-pass in series with 2-pole low-pass around the chosen
centre), a smoothed RMS detector on that band only, fast attack / slow release,
reduction capped by a Max reduction knob, hysteresis + per-sample smoothing so
switching produces no click — the same shape as `NoiseGate`, on a narrower band.

Placement was decided by measurement, not by taste: the de-esser sits **before**
`ReverbEffect`, so it de-esses the corrected lead voice and never the room tail.
A reverb tail is decorrelated high-frequency energy; feeding it to the detector
keeps the effect engaged after the singer has stopped hissing, ducking the whole
mix, and it ducks the body of the voice through the wet signal (pumping). The
bench numbers below are the evidence.

### Changes
- **New DSP module `ovtdsp::DeEsser`** (`Source/dsp/DeEsser.h`, header-only,
  `IEffect` interface — no `juce_dsp` dependency, so the test target compiles it
  without linking `juce::BiquadFilter`).
  - **Band**: 2-pole RBJ high-pass at `centre × 0.7` in series with a 2-pole
    low-pass at `centre × 1.3`, Q = 0.7071, coefficients stored pre-normalised by
    `a0` (house convention, as `FormantPreserver.cpp`). Edges clamped to
    300 Hz … nyquist − 200 Hz with a minimum 200 Hz span; the default 7 kHz centre
    gives 4900 – 9100 Hz. Measured band gain: ≈ 0.35 at 3 kHz, ≈ 0.778 at 7 kHz.
  - **Detector**: smoothed **per-sample RMS** of the band over the first two
    channels (τ = 10 ms) — the exact `NoiseGate.h` convention. The first version
    used a rectified mean, which reads 0.637 × amplitude for a sine against RMS
    0.707 × amplitude; switching to RMS made the detector behave like every other
    level-sensitive stage in the plugin.
  - **Hysteresis**: open at `2 × thresholdLinear` (+6 dB), close at
    `thresholdLinear`.
  - **Gain**: attack 5 ms toward `10^(−maxReduction/20)`, release 80 ms toward 1,
    per-sample ramp.
  - **Bypass**: 25 ms ramp with the `bypassGain < 1e-4` early-out, so toggling the
    power never clicks; bypassed output is bit-identical to the input.
- **Four APVTS parameters** (`Source/PluginProcessor.cpp`): `deess_enable` (bool,
  default **false**), `deess_threshold` (−60 – 0 dB, default **−25**),
  `deess_max_reduction` (0 – 24 dB, default **12**), `deess_frequency`
  (3000 – 12000 Hz, 100 Hz steps, default **7000**). Raw pointers cached in
  `PluginProcessor.h`; the effects loop reads them with `wetMix = 1.0f`. The
  branch is wired in **both** effects loops — the silent pass-through path and the
  main path; wiring one only makes the two paths diverge.
- **Chain order**: `effects.push_back<DeEsser>()` **before** `ReverbEffect`
  (`PluginProcessor.cpp` L791-798). Gate → upward comp → pitch/formant →
  **de-esser** → reverb.
- **UI in the Effects block** (`Source/PluginEditor.cpp/.h`): the block grows to
  **4 columns** (width 230 → 292 px) — row 1 Gate / Compress / Reverb / Formant,
  row 2 Doubler / **De-Esser** power toggle + Threshold knob / Depth knob / Freq
  knob. The block grows by columns, never by rows, so every knob keeps its full
  size. The two knobs with no switch of their own use the Harmony caption-label
  pattern. The enable switch greys the three knobs in the 30 fps timer, exactly
  like the sibling effects; values (`dB` / `Hz`) appear only in the popup display.
- **i18n**: `label.deesser` + `tooltip.deesser_enable`, `tooltip.deesser_threshold`,
  `tooltip.deesser_max_reduction`, `tooltip.deesser_frequency` in all 6 locales
  (en **De-Esser**, fr **Désesseur**, de **De-Esser**, es **Dessersor**, ja
  **デエッサー**, zh **齿音消除器**). Bytes verified as strict UTF-8, no BOM, no
  replacement characters.
- **Persistence**: `MorphState` fields (`deessEnable`, `deessThreshold` = 0.5833333,
  `deessMaxReduction` = 0.5, `deessFrequency` = 0.4444444 — normalized, as every
  APVTS value in the morph), capture, `applyInterpolatedState` (lerp + `lerpOrStep`
  for the boolean), `getMorphParameterIds()`, and the explicit A/B slot restore in
  `loadSlot` (`setSlider` × 3 + `setChoice`).
- **Factory plugin presets**: the presets that add top-end energy switch the
  de-esser on — `studioStack` −30 dB / 9 dB / 7 kHz, `spokenPodcast` −28 dB /
  10 dB / 7.5 kHz, `broadcastVoice` −20 dB / 16 dB / 6.5 kHz, `leadVocal` −25 dB /
  12 dB / 7 kHz; `correctionPure` and `transparentDub` set `deess_enable = 0`
  explicitly (their names promise no dynamics on the lead). `liveScene` and
  `breathyVocals` keep the default off.

### Placement bench
`tools/bench_deesser_placement.cpp` (new console target in `CMakeLists.txt`) plays a
synthetic voice — a 180 Hz harmonic stack with 7 band-limited sibilant bursts —
through three chains and measures the output: sibilance RMS (5.5 / 6.5 / 7.5 kHz
band-pass bank), total RMS, the same two over the last 400 ms of silent input
(the tail), a body band (400 / 800 / 1600 Hz) in the tail, and the peak-to-trough
swing of the de-esser gain in the tail window. Settings: threshold −35 dB, max
reduction 12 dB, centre 7 kHz, reverb room 0.4 / damping 0.6 / wet 0.3.

```text
chain     sibilance   total    tail sibilance  tail body    tail gain swing
bypass      +0.00 dB    +0.00 dB     +0.00 dB       +0.00 dB    0.0000
before     -8.78 dB    -5.01 dB    -10.52 dB       -4.28 dB    0.0451
after      -8.76 dB    -6.48 dB     -4.81 dB       -4.60 dB    0.1043
detector peaks (band RMS, dBFS): voice -36.95  burst -21.24  tail -82.85   close -35.00 dB  open -28.98 dB
PLACEMENT GATES PASS
```

Reading: the shipped order ("before") cuts the burst sibilance by 8.78 dB, cleans
the **tail** harshness 5.71 dB deeper than the alternative (−10.52 vs −4.81), keeps
1.47 dB more loudness, and pumps the tail 2.3× less (gain swing 0.0451 vs 0.1043).
The voice body alone peaks at −36.95 dBFS, below the −35.00 close point, so a loud
low note never triggers the detector; the bursts peak at −21.24, above the −28.98
open point. The tail detector peak is −82.85 dBFS: with the de-esser before the
reverb, the room never feeds the detector.

### Tests
New `test/dsp/DeEsserTest.cpp`, registered in `test/Main.cpp`: 8 groups /
1039 assertions, on the `UpwardCompressorTest` motif (deterministic tones at
44.1 kHz, 512-sample blocks):
1. **Bypass bit-identical** — 1024 assertions: de-esser off leaves the buffer
   exactly as it came in.
2. **Band isolation** — a 3 kHz tone is not de-essed (band gain ≈ 0.35, detector
   stays under the close point).
3. **Reduction depth** — a hot band tone ducks to the Max reduction floor
   (expected RMS = amplitude × 0.7071 × 0.25119).
4. **Hysteresis** — between the close and open levels the gate stays shut.
5. **Stereo link** — the loudest channel ducks both channels equally.
6. **Release** — silence lets the gate open again.
7. **Click-free power toggle** — applied gain continuity `< 0.01` per sample, plus
   a sample-continuity bound of 0.80.
8. **Disabled restores full level** — the tone returns to its input level.

Two bugs the tests caught:
- The first cascade wrote the high-pass and the low-pass coefficients onto the
  **same** 2-pole section, so the band was a low-pass at 9100 Hz only: a 3 kHz
  tone passed at ~1.0 and engaged the detector (measured outRms 0.0533 = 0.2121 ×
  floor). Fixed by splitting `BandSection` into `Section high, low` in series;
  post-fix probe: 3 kHz detector 0.0638717 / gain 1 / engaged 0, 7 kHz detector
  0.413414 / gain 0.251192 / engaged 1.
- The click test initially measured **output sample delta**, which is meaningless on
  a 7 kHz sine: its natural slope is `2·A·sin(πf/sr)` ≈ 0.765 (probe printed
  maxDelta 0.764861 against sineMaxDelta 0.797865). The test measures the
  continuity of the **applied gain** instead.

Build and suite results: full suite in one process — **133 groups OK / 0 FAILED**,
exit 0, including `DeEsser` (8 groups). The plugin SharedCode target builds clean in
Release; `bench_deesser_placement` passes all gates with exit 0.

### Notes
- Windows `min` / `max` macros break `juce::max(...)` inside test code (`C2589` +
  `C2059`); plain `if (d > maxDelta) maxDelta = d;` is the safe form.
- A failed build leaves the **stale** test executable in place: always read the
  build tail before running it, otherwise a green run proves nothing.
- Deferred by design: a de-esser sidechain input (external detector reference),
  which belongs to the 3-bus architecture work; and a split-band mode that ducks
  only the band instead of the whole voice.

## Refactor — De-Esser collapsed to one Intensity knob (`deess_amount`)

### Context
The surface shipped hours earlier asked for three decisions: a dB threshold, a depth
cap, and a centre frequency. A speaker does not make those decisions; they set a
de-esser as strong as they want and expect the tool to place it. The chosen answer is
one **Intensity** knob, with the band centre driven by **Voice Type** — the register
the user already picks for pitch detection — so no extra control appears at all.

### Changes
- **Mapping**: `thresholdDb = −38 × amount`, `maxReductionDb = 19 × amount`
  (`setAmount`), the hardware de-esser convention: more intensity means both "hear
  the sibilance sooner" and "duck deeper". The default **0.65** reproduces the
  measured −24.7 dB / 12.35 dB pair, so the placement bench and the depth tests keep
  their meaning. At 0% the open level is +6 dBFS, unreachable by any signal, and the
  depth floor is unity: mathematically inert, no special case in the code.
- **Band centre from Voice Type** (`setVoiceType`): ladder index 0 – 5 →
  **7000 / 5200 / 6000 / 6800 / 7600 / 8400 Hz**, edges at ×0.7 and ×1.3 (the Soprano
  top edge, 10920 Hz, stays under nyquist − 200 at 44.1 kHz). Universal keeps the
  measured 7 kHz, so default behaviour is unchanged. The raw setters
  (`setThresholdDb`, `setMaxReductionDb`, `setFrequencyHz`) stay in the header: the
  unit tests and the placement bench drive them directly.
- **Parameters**: `deess_threshold`, `deess_max_reduction` and `deess_frequency` are
  removed; `deess_amount` (0 – 1, default 0.65) replaces them. No migration code is
  needed: the plugin has none, and no XML preset on disk carried the removed keys —
  the UI shipped the same day. `MorphState.deessAmount` stores the normalized value,
  which for this range equals the raw value; capture, lerp, `getMorphParameterIds()`
  and the A/B slot restore all follow.
- **UI**: Effects row 2 is Doubler + De-Esser with one knob under the power toggle.
  The caption-label cells for Depth/Freq and the `placeCaptionKnob` lambda are
  deleted (no remaining users); the block keeps its 4 columns, leaving two free cells
  for the next module. The knob reads `0 – 100 %` in the popup display only, greys
  with the enable switch, and carries `tooltip.deesser_amount`.
- **i18n**: `tooltip.deesser_amount` in all 6 locales, and the Voice Type tooltip now
  states that it places the de-esser band. Bytes verified as strict UTF-8, no BOM, no
  replacement characters.
- **Factory presets** set intensity only: `studioStack` 0.55, `spokenPodcast` 0.65,
  `broadcastVoice` 0.8, `leadVocal` 0.65; `correctionPure` and `transparentDub` keep
  `deess_enable = 0`. No preset sets `voice_type`: it constrains pitch detection, so
  changing it is a semantic change beyond de-essing.

### Tests
The suite grows to **11 groups / 1056 assertions** for `DeEsser`: the depth group is
re-anchored on the amount mapping, and three groups are new — intensity monotonicity
(0% inert, 100% reaches `10^(−19/20)` = 0.141, mid intensity in between), the Voice
Type ladder values with clamping at both ends, and Bass band isolation (the 5200 Hz
centre passes its own tone and engages; 11 kHz sits outside and does not, detector
ratio > 1.5). The hysteresis group moved to `ampHold = 0.17`, because the new open
level (0.117) sits above the band level of the old `ampMid` tone: a test that passes
for the wrong reason is not a test.

Full suite in one process: **136 groups OK / 0 FAILED**, exit 0. The plugin SharedCode
target builds clean in Release, and `bench_deesser_placement` still passes all gates —
it drives the raw setters, so the mapping change does not touch it.

### Notes
- The ladder is a **user choice, not a measurement**: a user who leaves Voice Type on
  Universal gets 7 kHz whatever their voice. Stated as the honest limitation of this
  design.
- Deferred: auto multi-band placement (5 / 6.5 / 8 kHz detectors with band-selection
  hysteresis), ~1.5 day extra.


## Refactor â€” Shared UI widgets: five project-local widgets retired to eiffelbs-ui (v0.12.28)

### Context
The shared design library already owned most of this plugin's surface (theme, knobs,
morph slider, power toggles), but five widgets were still drawn locally: the tab pill,
the toolbar icon buttons, the Advanced chevron, the Use Voice switch and the Debug
window log. The blocker was a gap in the library â€” `ebs::IconButton` had no glyph for
zoom, chevrons, grid, step, power, trash or save, so the toolbar could not move. The
gap was closed **upstream first** (the no-fork rule), then the consumer swaps followed.

### Changes
- **Upstream, library commit `7c374e9` / tag `v0.12.28`**: 11 shapes added to
  `ebs::IconButton` â€” `zoomIn`, `zoomOut`, `chevronUp`, `chevronDown`, `chevronLeft`,
  `chevronRight`, `grid`, `step`, `power`, `trash`, `save`. The Feather SVG strings are
  verbatim from this project's drawable table, so the glyphs the plugin already drew are
  now shared. `IconButton.h` 540 â†’ 558 lines; `tests/Smoke.cpp` renders **the whole
  39-shape enum** and asserts every shape paints visible ink.
- **`ebs::PillTabSwitch`** replaces the local tab pill (Live / Curve Editor). The pill
  self-measures its width and scrolls on wheel; its label font is fixed at 17 px, so the
  text is slightly larger than the old widget â€” verified on screen that it does not push
  the toolbar icons out of `toolsArea`.
   Slots carry glyphs through the library provider `onTabIcon` (`mic` for Live,
   `activity` for Curve Editor): the strip measures icon + gap + label itself, and the
   glyph colour follows the label (white on the active slot, dim elsewhere).
- **14 buttons â†’ `ebs::IconButton`**: zoom in / out, scroll up / down, reset view, global
  undo / redo, preset prev / next / save, Options, Preset Gallery, hamburger menu,
  rewind, and the Curve Editor undo / redo chips. Two local setup lambdas
  (`setupEbsIcon`, `setupFramedIcon`) carry the tooltip and framed-body language; the
  Curve Editor chips pin white ink over a soft-accent frame because they float on the
  theme-invariant dark canvas. The dead hook `refreshDrawableButtonIcons()` is deleted,
  and the SVG table keeps only the six strings still drawn (scale, grid, step, power,
  play, stop).
- **Six buttons kept as `juce::DrawableButton` on purpose**: bypass, midiOut, snap,
  snapGrid, stepMode, play. They paint a solid accent pill when ON and
  `ebs::IconButton` has no accent-fill ON background, so swapping them would delete the
  active-state signal.
- **`ebs::DisclosureChevron`** replaces the `VerticalTextButtonLF` triangle that painted
  the Advanced chevron from `getToggleState()`; the host keeps owning `advancedExpanded`.
- **`ebs::ToggleSwitch`** on `useVoiceButton` â€” a drop-in typing replacement for
  `juce::ToggleButton`, knob glides ~120 ms, disabled state dims by multiplying alpha.
- **`ebs::LogView`** for the Debug window's MIDI log half: tail-follow + drop counter,
  fed by the same 10 Hz refresh from `proc.getLastSentMidiNoteForChannel(1..9)`.
- **`ebs::sanitizeForShaping` on displayed preset names**: the preset combo list and its
  four `setText` sites, plus the gallery card name and meta labels. User file names and
  XML fields are an untrusted edge for the text shaper; the raw name stays the lookup
  key, so preset matching is untouched.

### Tests
- `OpenVoxTuner` SharedCode target: **0 errors** in Release.
- `OpenVoxTunerTests`: rebuilt and run â€” **136 OK / 0 FAILED**, exit 0.
- Library smoke on the new tag: **PASS, 0 failures**, including the new gate "every
  IconButton shape paints visible ink" over all 39 shapes (receipt
  `%TEMP%\eiffelbs-ui-smoke.png`, 18738 bytes).
- Standalone captured at `.dbg/ui_check_swaps.png` (1102x628, `PrintWindow` flag 2,
  PID 88220): pill, five toolbar glyphs, gallery grid icon, global undo/redo, settings
  gear, collapsed Advanced chevron and the Use Voice switch all render on the real widget.

### Notes
- Curve Editor chips, the expanded Advanced panel and the Debug window need a mouse click
  to be seen; synthetic input is blocked on this machine, so those three surfaces are
  verified by compile + code review only and are left to the author's session.
- `attackAwareButton` stays untouched: deprecated and hidden.
## UI polish — taller pill band, flush viewport, system title bars

### Context
Three layout complaints from the author's session on the running standalone: the Live /
Curve Editor pill looked cramped, the tab viewport left a wider gap above the control
frames than the frames leave below them to the window edge, and the plugin still drew its
own caption (the JUCE title bar) in the standalone and in the Preset Gallery window. The
system title bar is now the house norm across the projects under development.

### Changes
- **Pill band 44 px, pill 40 px** (`resized()`): the toolbar band over the tab viewport
  grows from 30 to 44 px and `ebs::PillTabSwitch` from 30 to 40 px, so the badge (library
  inset: `height - 6`) reads 34 px instead of 24. The pill geometry is height-driven in
  the library (radius `h/2`, badge `h-6`), so no library change was needed.
- **Fixed 22 px icon slots**: `iconSize` used to be `toolsArea.getHeight()`, which would
  have stretched every toolbar button to 40 px with the taller band. A `placeIcon` lambda
  now takes square slots and centres each button in its slot; `measuresComboBox` is
  centred at 24 px for the same reason.
- **Viewport flush**: `tabArea.setBottom (centerArea.getBottom())` puts the tab viewport's
  bottom edge on the centre area's, so the gap above the frame row equals the pad the
  frames keep to the window edge (10 px). Measured on screen: canvas bottom → frame top
  10 px, frame bottom → window edge 9–10 px.
- **System title bars**: `ClosableGalleryWindow` gets `setUsingNativeTitleBar (true)`;
  `parentHierarchyChanged()` sets it on the standalone wrapper, guarded by
  `! w->isUsingNativeTitleBar()` so the window peer is recreated once, not on every
  callback. The Harmony Debug window was already native and stays as it is. In a DAW host
  the `DocumentWindow` cast yields nullptr, so both calls are no-ops.

### Tests
- Full tree rebuilt in Release: **0 errors**, exit 0.
- `OpenVoxTunerTests`: **136 OK / 0 FAILED**, exit 0.
- Standalone captured at `.dbg/ui_check_swaps.png` (1116x639, `PrintWindow` flag 2):
  Windows caption with the standard minimise/maximise/close buttons replaces the JUCE
  strip; pill badge measures 32 px on screen (was 26); toolbar icons stay 22 px; canvas
  bottom → frame top 10 px and frame bottom → window edge 9–10 px, read pixel by pixel.

### Notes
- The pill width grows with the glyph slots (icon 14 + gap 6 per slot), so at narrow
  plugin widths the middle toolbar group loses slack — watch it on the smallest host
  window.
- The gallery window caption is native now; seeing it needs a click on the gallery icon,
  and synthetic input is blocked on this machine, so that surface is verified by compile
  + review only.
## UI polish — pill lifted above the viewport, Audio/MIDI setup in the Advanced menu

### Context
The band-above-viewport layout of the previous pass did not land: the pill still overlapped
the tab canvas. The reason is a JUCE detail — hiding the tab bar with `setVisible(false)`
does not shrink the strip it reserves, so the canvas kept starting 30 px below the top of
`tabbedComponent`, inside the band. The system title bar also removed the standalone's
built-in "Options" button, which was the only way to reach the audio device and MIDI setup
dialog, so that entry needed a home in the plugin's own menu.

### Changes
- **`tabbedComponent.setTabBarDepth (0)`** (constructor, right after the bar is hidden):
  `setTabBarDepth` is public and calls `resized()`, so zeroing it makes the content fill the
  whole component bounds. That is what lets the band be a strip taken off `centerArea` above
  the tab area, with `tabArea.setY (bandArea.getBottom())` — canvas top on band bottom.
- **Band geometry**: `bandH = 44`, `bandGap = 6`; the pill (40 px) is centred in the band, so
  it sits directly under the logo banner and the viewport starts below it with a minimal gap.
- **Advanced menu, standalone only**: new item "Audio/MIDI setup" opens
  `juce::StandalonePluginHolder::getInstance()->showAudioSettingsDialog()`, the same dialog
  JUCE's own caption button used to reach. Guarded by `#if JUCE_STANDALONE_APPLICATION` so the
  VST3 binary never compiles the wrapper class; the include needs `juce_audio_utils` first,
  because the wrapper header declares `AudioDeviceManager` and `AudioIODeviceCallback`.
- **i18n**: key `menu.audio_midi_setup` added to `ovt::Keys` with all six locales (en, fr, de,
  es, ja, zh).

### Tests
- Full tree rebuilt in Release: **0 errors**, exit 0. The first attempt failed with 130
  cascading errors in `juce_StandaloneFilterWindow.h` (base class not defined,
  `AudioDeviceManager` not a namespace) until the module include was added.
- `OpenVoxTunerTests`: rebuilt and run — **136 OK / 0 FAILED**, exit 0.
- Standalone captured at `.dbg/ui_check_swaps.png` (1116x639, `PrintWindow` flag 2, PID 131168)
  and read pixel by pixel: pill badge spans y 86..125 (40 px pill, 32 px accent badge), canvas
  fill starts at y 131 → **6 px gap** between pill bottom and viewport; the bottom margins are
  unchanged (canvas bottom → frame top 10 px, frame bottom → window edge 9–10 px).

### Notes
- The Audio/MIDI dialog opens on a click, so it is verified by compile + code review here;
  the author confirms it in his session.

## Feature â€” Preset Gallery icon gated to Curve Editor, Effects block as an ExpansibleFrame

### Context
The author checked the pill band and found one leak: the "Preset gallery" toolbar icon was
visible on the Live tab, where the gallery has nothing to act on â€” it opens the curve notes,
so it belongs to the Curve Editor tab only. He also asked to adopt `ebs::ExpansibleFrame` for
the Effects frame (the second block of the bottom row): two columns of two cells visible
directly, Gate over Reverb in column 1, Compress over Doubler in column 2, and Formant over
De-Esser in a third column that only shows when the panel is open.

### Changes
- **Visibility** (`PluginEditor.cpp`, tab-driven block): `presetGalleryButton.setVisible (isCurveEditorMode)`
  added next to the curve-only icons (options, snap, snap grid, step mode, zoom, scroll, reset
  view, measures, play/rewind), so the icon follows the tab exactly like its siblings.
- **Frame adoption**: new member `ebs::ExpansibleFrame effectsFrame` in `PluginEditor.h`. The six
  effect cells (6 power toggles + 6 knobs) are reparented into the frame in the constructor with
  `effectsFrame.addAndMakeVisible (c)`; as children of the body they paint above it and are clipped
  by it, so a folded column vanishes with the frame. The frame paints its own body through
  `drawFramePlain`, so the two block4 fill/outline calls in `paint()` were deleted, and the
  `block4Bounds` member is gone.
- **Host layout**: `effectsFrame.onExpandedChanged = [this] { resized(); }` â€” the frame owns the
  open flag, the editor re-lays out when it flips. The width is computed from the flag:
  `effectsChevron (28) + 2 * effectsPad (10) + (open ? 3 : 2) * effectsColW (74) + (open ? 2 : 1) * effectsColGap (8)`
  â†’ **204 px closed, 286 px open** (the block was a fixed 292 px), so closing frees ~88 px for the
  Harmony block. Cells are laid out in `effectsFrame.interior().reduced (10)`, two rows per column;
  `effKnobD = jlimit (30, 54, rowH - 18 - 2)` keeps knobs at their full 54 px because adding a
  COLUMN is how this block grows, never a third row.
- **Folded cells**: Formant and De-Esser widgets are `setVisible (effectsOpen)`, so an invisible
  cell takes no clicks and leaves its slot empty.
- **Chevron tooltip**: `ebs::DisclosureChevron` sets no default tooltip, so the host supplies one â€”
  new key `tooltip.effects_more` in all six locales (en, fr, de, es, ja, zh).

### Tests
- Full tree rebuilt in Release: **0 errors**, exit 0. `OpenVoxTunerTests`: **136 OK / 0 FAILED**, exit 0.
- Standalone captured twice with `PrintWindow` flag 2 (1116x639): `.dbg/ui_check_frame_closed.png`
  and `.dbg/ui_check_frame_open.png`. Pixel scan of the closed row: Effects frame outer edges
  x 249..449 (200 px measured, chevron glyph at x 438..441), Root/Scale block 466..696, Harmony
  712..1096 â€” the Harmony block took the freed width. The open capture shows three columns with
  Formant over De-Esser and the chevron pointing left.
- **Preset Gallery icon is absent from the Live capture** (the toolbar row shows zoom, scroll up,
  confirm, reset only).
- The open state was captured by temporarily starting `effectsFrame.setExpanded (true)` in the
  constructor; that line was removed and the tree rebuilt, so the shipped binary starts collapsed.

### Notes
- The chevron click cannot be injected here, so the toggle is proven by the temporary expanded
  launch plus code review; the author confirms the click in his session.
- Opening the frame pushes the Root and Harmony blocks right; at narrow plugin widths Harmony
  loses slack first (same trade-off as the widened pill band).

## Fix — Effects frame repaint ghost after expand/collapse

### Context
The ExpansibleFrame adoption works, but clicking the chevron left the old Root/Scale block ink on screen: the display only cleaned up when the window was resized.

### Changes
- `PluginEditor::resized()` ends with `repaint()`. The bottom-row blocks are paint-only regions (the editor draws their fill and outline), so moving one repaints only the rectangles its widgets vacated. The effects frame is a component and repaints itself, which is why only its neighbours ghosted.
- The glyph half of the bug was fixed upstream, see the repin section below.

### Tests
The click cannot be injected here, so the state was driven from code: a temporary timer opened the frame 1.5 s after launch and nudged the editor bounds 3.5 s later (the same full repaint a manual resize forces). DWM captures of the window before and after the nudge differed on 2 319 pixels, x 465..795 / y 450..621 - the ghosted Root/Scale block. After the fix the two captures are pixel-identical (diff 0). Release build 0 errors; tests 136 OK / 0 FAILED.

### Notes
- The rule covers every re-layout that moves a paint-only region, including the Correction block expand.
- The temporary timer was removed and the tree rebuilt, so the shipped binary carries no repro code.

## Repin — eiffelbs-ui v0.12.30, chevron repaints on fold change

### Context
`ebs::DisclosureChevron` exposes its fold state as a public field, and `ExpansibleFrame::setExpanded` wrote that field directly: the glyph changed with no repaint. A host that moves the strip gets its repaint from `setBounds`, a host that keeps it in place gets none.

### Changes
- Upstream patch, eiffelbs-ui `v0.12.30`: `DisclosureChevron::setExpanded (bool)` sets the flag and repaints; `ExpansibleFrame::setExpanded` calls it, and the header now states the host-side repaint duty for paint-only regions.
- `CMakeLists.txt` FetchContent pin `v0.12.28` — `v0.12.30`.

### Tests
Library smoke PASS (0 failures), including the new DisclosureChevron + ExpansibleFrame block: both fold states rendered, glyph direction read from ink columns (closed heavy at x 8, open heavy at x 13), host notified once, `interior()` slot. OVT rebuild 0 errors; tests 136 OK / 0 FAILED.

### Notes
- The editor-side `repaint()` is what cleared the ghosted block; this patch closes the same bug class for the glyph, for any host that toggles without moving the frame.
- The library commit is `fbfa5f0`, tag `v0.12.30` pushed to `EiffelBS/eiffelbs-ui`.
