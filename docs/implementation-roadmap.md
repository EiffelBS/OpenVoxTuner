# OpenVoxTuner - Implementation Roadmap

> Last updated: 2026-07-15 CEST

## Legend

- [x] Implemented
- [ ] Not yet implemented
- [~] In progress

---

## 1. Core DSP Engine

- [x] Pitch detection (YIN algorithm)
- [x] Pitch detection (YIN algorithm) — robustness fix 2026-07-12: replaced the over-strict `numSamples < maxLag * 2` guard (which returned 0 for 2048-sample buffers at 44.1 kHz, failing the 440/220 Hz unit tests) with an adaptive `searchMax = min(maxLag, numSamples/2)` range, applied to both `PitchDetector.cpp` (test) and `YinPitchDetector.cpp` (production); unit tests now 52 OK / 0 KO.
- [x] Standalone transport clock ran slow (lost per-block increments through the 10 ms host-time cache) 2026-07-12: in Standalone `currentTime` is now re-based on `transportTime` every block (not the stale `cachedTransportTime`), so the standalone tempo matches the DAW (8 s to ruler label "5" at 120 BPM instead of ~32 s); host/ARA path unchanged.
- [x] Pitch detection (YIN algorithm — SWIPE'/PYIN evaluated and removed)
- [x] Pitch shifting (PSOLA)
- [x] Formant preservation
- [x] Scale quantizer (14 scale types including Custom)
- [x] Harmony engine (21 harmony types + None; 22 entries)
- [x] Reverb effect (post-processing)
- [x] Noise Gate (input, before pitch detection)
- [x] FlexTune / Humanize parameters
- [x] Correction mode (Modern / Transparent)
- [x] Retarget envelope (attack/release smoothing)
- [x] Pitch curve (programmatic control over time)
- [x] Vibrato preservation (2026-07-14): correct against a smoothed center pitch instead of the instantaneous pitch, so the vibrato modulation survives while the note still snaps to the scale. New `vibrato_preserve` parameter (0-100%), `ovtdsp::VibratoPreserver` helper, Correction-section knob, A/B-morph + preset persistence, and unit test.

## 2. Audio I/O and Plugin Format

- [x] VST3 plugin format
- [x] Audio Unit (AU) plugin format (macOS)
- [x] Standalone application mode
- [x] ARA integration (DAW timeline sync)
- [x] MIDI output (quantized note events)
- [x] Bypass mode (audio pass-through)

## 3. UI / GUI - Main Editor

- [x] Dark theme with blue accent (Autotune-style)
- [x] Light theme support (toggle in hamburger menu)
- [x] Title bar with logo and version
- [x] Hamburger menu (gear icon) for options
- [x] Tabbed view: "Live" (visualizer) and "Curve Editor"
- [x] Preset system (Factory + Custom save/load/delete)
- [x] Key and Scale selection (ComboBox)
- [x] Custom scale editor (12-button keyboard)
- [x] Speed, Amount, Formant, Reverb knobs
- [x] FlexTune and Humanize knobs
- [x] Correction mode toggle (Modern / Transparent)
- [x] Bypass toggle (standalone only)
- [x] MIDI Out toggle
- [x] Harmony controls (enable, type, gain, blend, tone)
- [x] Use Voice / shifted voices selector
- [x] Harmony tone color knob
- [x] Harmony "Follow Lead" toggle (2026-07-14): when on (default), the harmony voices follow the lead correction character (vibrato preservation, humanize, flex, attack-aware), so the blue harmony lines move with the green lead line instead of staying locked to the scale grid. Implemented as the `harmony_follow_lead` AudioParameterBool (default true) that re-applies the lead's `charRatio = f0_out / scaleNote` to each harmony voice in `processBlock`; `MorphState` / preset persistence; power-style toggle button in the Harmony block; i18n EN/FR/DE/ES/JA.
- [x] Latency mode selection (Direct Monitoring / Low Latency / Quality / Safe)
- [x] Update checker (GitHub releases)
- [x] Internationalization (i18n) - English, French, German, Spanish, Japanese, Simplified Chinese (zh) (menu items, labels, all tooltips)
  - UTF-8 Debug-crash fix 2026-07-13: map value type `juce::String` -> `const char*`, `tr()` converts via `CharPointer_UTF8`, and MSVC `/utf-8` flag added in CMakeLists.txt (static-init `String(const char*)` assertion on non-ASCII literals eliminated).
  - 2026-07-15: added Simplified Chinese (`Language::Chinese`, code `"zh"`) as a 6th language — full translation map + menu item + `ui_language` range extended to `0..5`.
  - 2026-07-15: "Gate" label forced to English in every language (`tr()` short-circuits `kLabelNoiseGate` to the English map) because "Porte" (FR) / "Puerta" (ES) / "Door" etc. are misleading for the audio noise-gate term.
- [x] Language selector in hamburger menu with persistence
- [x] Centralized font system (OVTFonts.h) - consistent typeface across all components
- [x] Centralized theme system (OVTTheme.h) - dark/light theme accessors
- [x] MIDI Learn for sliders (hamburger menu submenu with CC assignment)
- [x] CPU Usage Meter (header strip display with color-coded bar, positioned left of A/B button)
- [x] A/B Comparison (two separate buttons with morph slider between them, green border for valid data, right-click save, MorphState-based persistence — no exponential XML growth)
- [x] PresetMorpher interpolation engine (header-only, captures/applies/morphs states + PitchCurves)
- [x] DAW automation coexistence: morph no longer overwrites parameters driven by concurrent host automation (e.g. speed/amount lanes running alongside a morph automation lane)
- [x] Morph slider A<->B toggle bug fixed 2026-07-11: the external-automation exclusion map (`lastMorphIntendedValues`) was not cleared on slot switch, so toggling A<->B several times accumulated exclusions until the slider had no effect. Now cleared on slot switch and on the A->B context-menu action.
- [x] Stray green curve on A/B slot switch fixed 2026-07-13: `interpolateCurves` (PresetMorpher.h) resampled over a fixed `timeRange = 10.0` SECONDS while the whole PitchCurve system works in BEATS (PPQ) — editor axis, DSP `getPitchAt(transportTime)`, user-drawn points. Switching slots triggers a morph via the automatable `morph_amount` param (timerCallback -> onMorphSliderChanged), so every curve was stretched/truncated to ~0..10 "beats" (≈ ruler "3.3") producing a garbled, over-dense curve persisted into the settings `PITCH_CURVE` (which is why deleting the settings file fixed it before). Fix: resample over the union time span of the two input curves, in beats; at morph endpoints (t<=0.001 or t>=0.999) return the exact source/target curve (no 128-point resample) so switching slots keeps the original point structure instead of densifying the curve. Unit test added/extended in PitchCurveTest.cpp (54 OK / 0 KO).
- [x] Preset commits to active A/B slot 2026-07-13: `loadFactory` now `saveSlot`s the loaded preset into the active slot (so it survives slot switching) and aligns `morph_amount` to the active slot's endpoint; previously the preset only touched the editor curve and `resetMorph()` left `morph_amount` at 0, so switching slots reloaded the slot's stale stored curve. Auto-scroll forced off + menu item disabled in Standalone (playhead loops on Measures).
- [x] Morph blends parameters ONLY 2026-07-13: Jérôme chose "Parametres seuls" — when morphing between two different curves the displayed curve snaps to the nearest slot's curve (morphSource/morphTarget) and is never crossfaded, so the Morph slider can no longer densify the curve with spurious intermediate points (previously caused by the curve-blend resample). Consequence: `interpolateCurves` and the unused `loadStateFromXml` were removed from `PresetMorpher.h` (no-dead-code rule); removing `loadStateFromXml` also clears a latent `reverb_enable`->`harmonyEnable` typo. The `interpolateCurves` test was removed from PitchCurveTest.cpp; suite 54 OK / 0 KO.
- [x] A/B slot curve persisted across reload/restart 2026-07-13: `getStateInformation`/`setStateInformation` now serialize the slot `MorphState.curve` as a nested `PITCH_CURVE` child of `AB_A`/`AB_B` (previously only the scalar params were saved, so a restored slot had an empty curve and clicking it cleared the editor line).
- [x] Middle-button horizontal scroll disabled in Loop Playhead (measures) mode 2026-07-13: `PitchCurveEditor` stores `loopingPlayhead` (from `setPlayheadTime`'s `isLooping`) and the middle-drag handler now also requires `!loopingPlayhead` (auto-scroll-off still required).
- [x] Curve Editor "Options" button is now icon-only hamburger 2026-07-13: replaced the label-only "Options" `TextButton` (and the now-unused `PresetsButton` class) with an icon-only `DrawableButton` using a hamburger (3 bars) SVG, with a distinct accent-tinted background so it stands out from the neutral zoom/scroll/snap buttons; an 8px gap separates it from the reset (X) button (previously touching). The plugin's own options keep the gear.
- [x] Flex/Humanize labels no longer truncated on macOS 2026-07-13: label widths are now measured from glyph metrics instead of hard-coded 28/52px boxes (which overflowed under the macOS SF Pro fallback for the Windows-only "Segoe UI" font).
- [x] Curve Editor trackpad support 2026-07-13: added `mouseMagnify` for macOS pinch-to-zoom (shared `applyZoom` helper, clamped 1..8 octaves) and two-finger horizontal scroll via `wheel.deltaX` panning `scrollOffset` (same constraints as middle-drag: disabled while auto-scroll/loop locks the view).
- [x] "Export as Image" exports the active tab 2026-07-13: the menu no longer hard-codes the Live visualizer (`getTabContentComponent(0)`); it now dispatches on `getCurrentTabIndex()` to `curveEditor->exportAsImage` (new method, mirrors the visualizer's 2x PNG) or the visualizer. Dialog/not-found strings generalized from "Visualizer" to "current view" across en/fr/de/es/ja.
- [x] Value-less knobs show live value while dragging 2026-07-13: replaced the (non-working) tooltip-on-drag approach with JUCE's popup display (`setPopupDisplayEnabled(true, false, this)` + `textFromValueFunction` for units) on Flex/Humanize/Gate/Reverb/Formant; removed the redundant drag-tooltip lambdas. The TooltipWindow can't show during a rotate-drag because it needs a stationary mouse.
- [x] VST3 category Fx/Pitch 2026-07-13: `juce_add_plugin` now sets `VST3_CATEGORIES "Fx" "Pitch"` so DAWs (Cubase, Nuendo, Studio One, ...) file the plug-in under "Pitch & Time".
- [x] Keyboard shortcuts help overlay (? key or hamburger menu)

## 4. UI / GUI - Pitch Visualizer (Live Tab)

- [x] Real-time pitch curve display (input + output)
- [x] Harmony voice lines display
- [x] Semi-logarithmic frequency scale (Hz)
- [x] LED-grid VU meter (tuning cents indicator)
- [x] Current note display with cents offset
- [x] Target note indicator
- [x] Vertical piano keyboard (notes highlighted by scale and active pitch)
- [x] Mouse wheel scroll (vertical pan)
- [x] Ctrl/Cmd + mouse wheel zoom
- [x] Dynamic octave reference lines (C notes) with labels - visible across ALL octaves
- [x] Dynamic scale note lines within visible range
- [x] Complete legend (Input, Output, Harmony) - compact, no truncation
- [x] Keyboard shortcut hints (non-truncated, compact format)
- [x] Scroll/zoom SVG icon buttons (magnifying glass, chevrons, cross)
- [x] Reset view button (restores default frequency range)
- [x] Hover cursor showing Hz/note value at mouse position
- [x] Y-axis frequency labels (Hz values along the right edge)
- [x] Animated smooth transitions for zoom/scroll (lerp interpolation)
- [x] Image export (PNG/JPEG at 2x resolution)
- [x] Piano key note labels (D, E, F, G, A, B - height-gated)
- [x] ARA2 waveform overlay (input audio captured in processBlock, displayed as background in Live visualizer with menu toggle)
- [x] Waveform overlay reflects the input noise gate (2026-07-14): the visualizer waveform is now captured after the noise-gate stage in `processBlock`, so the displayed waveform is attenuated together with the gated audio (previously it showed the raw pre-gate input). The French "Gate" label/tips use the untranslated audio term "Gate" instead of "Porte".
- [x] Unified waveform display types (Bars, Filled, Line, Mirror) with user-selectable modes via hamburger menu, shared rendering function between Live and Curve Editor views, persisted across sessions
- [ ] Bookmark positions (save/restore frequently used frequency ranges)
- [ ] Responsive layout for small screens

## 5. UI / GUI - Curve Editor (Graphic Tab)

- [x] Graphical pitch curve editing (click to add/move points)
- [x] Time ruler with measures/beats
- [x] Snap to scale (quantize points to scale notes) — bug fixed 2026-07-11: the snap now uses the authoritative scale interval set (same as the on-screen display). The real root cause was the Scale/Key ComboBox not updating the parameter (see entry below), so the snap used a stale/default scale; that binding is now fixed. Further fixed 2026-07-12: in-scale notes now snap to their exact pitch (not the raw clicked frequency), so all scale notes — including D4/G4/A#4 in C Natural Minor — lock onto the note instead of staying where clicked.
- [x] Snap to grid (quantize points to beat grid)
- [x] Step mode (staircase interpolation)
- [x] Clear all points
- [x] Transport playhead (follows DAW time in ARA mode)
- [x] Auto-scroll toggle (Options menu; available in ARA and Standalone — no longer an embedded checkbox) — behavior fixed 2026-07-12: OFF now keeps the view fixed during playback (playhead can run off-screen); only reveals on an explicit seek. Previously OFF still scrolled, making the option appear to do nothing.
- [x] Time signature display
- [x] Measures count selector (toolbar row, no longer covering the ruler)
- [x] Preset curves for common use cases
- [x] Harmony trace visualization
- [x] Right-click preset menu
- [x] Reset playhead button (standalone/VST3) — also exposed as a "Return to start" (rewind) toolbar button in Standalone (vertical bar + left-pointing triangle glyph) 2026-07-12.
- [x] Undo/Redo buttons (visual UI complement to keyboard shortcuts)
- [x] Scale note lines (horizontal reference lines for current scale notes)
- [x] Curve Editor toolbar mirrors Visualizer view controls (Zoom In/Out, Scroll Up/Down, Reset View) + "Options" menu (Clean Curves, Reset Playhead, Curve Presets) 2026-07-11: snap/grid/step kept as direct toggle icons; clear/reset moved into the Options menu; zoom/scroll reuse the Visualizer's SVGs and pitch-zoom/pitch-pan semantics (matching the Visualizer and the Curve Editor's own wheel behavior).
- [x] "Measures" combo + label moved onto the toolbar row (same line as Options menu and view icons) so it no longer covers the ruler 2026-07-12.
- [x] "Curve Presets" promoted to a direct submenu of the Options menu (no extra click-to-open step) 2026-07-12.
- [x] Options menu "Auto-Scroll" item is now a ticked toggle (replaces the old embedded checkbox+label) 2026-07-12.
- [x] Standalone transport: a single Play/Pause toggle (Play glyph when stopped, Stop glyph when playing) plus a "Return to start" (rewind) button on the Curve Editor toolbar (standalone only) 2026-07-12. The toggle freezes/runs the timeline so the curve can be edited while stopped; the rewind button resets the playhead and clears the input trace.
- [x] Standalone window maximise button (JUCE's StandaloneFilterWindow only requested minimise + close by default; re-added via parentHierarchyChanged) 2026-07-12.
- [x] Standalone tempo: "Tempo" submenu in the Options menu (standalone only) lets the user fix the BPM instead of being locked at 120 BPM; the fallback transport advances at the chosen tempo 2026-07-12.
- [x] Curve Editor "Show Input Trace" toggle (Options menu) shows/hides the live input pitch trace (red line); ON by default 2026-07-12.
- [x] Reset Playhead reliability fix: new `returnToStart()` resets the scroll offset AND the playhead on the first click (icon + Options menu) in every context; the earlier 3–4 click defect came from the view only snapping back via `setPlayheadTime`'s >0.5-beat seek detector, leaving the playhead off-screen when auto-scroll was OFF 2026-07-12.
- [x] Ruler click moves the playhead to the clicked position, quantized to the project grid (0.5 beat); works in Curve and Live modes; `onSeek` callback bridges editor → transport (DAW/standalone) 2026-07-12.
- [x] Middle-button drag horizontal scroll (beats) in the curve editor + ruler, active only when auto-scroll is OFF; hand cursor + yellow feedback overlay while dragging; `clampScrollOffset` bounds the scroll to >= 0 2026-07-12.
- [x] Curve Editor playhead loop mode (per transport context) 2026-07-12:
  - ARA: playhead follows the DAW (unchanged).
  - Standalone: playhead loops within the Measures window `[0, measuresVisible * ppqPerBar]` (end of beat 4 for "4 Measures" in 4/4 = 16 beats).
  - Plugin (VST3, non-ARA): user choice between "Follow host" (default) and "Loop (Measures)" via a new Options-menu ticked item "Loop Playhead (Measures)" (disabled/greyed in ARA and Standalone, reflecting the forced mode).
  - The loop length is shared by the playhead display AND the graphic pitch-curve sampling (replaces the hardcoded `fmod(currentTime, 16.0)` at PluginProcessor.cpp:1080), so playhead and curve loop on the same window. `transportTime` stays monotonic; a derived wrapped time (`getLoopTransportTime()`) is used for display/trace/sampling.
  - New parameter `editor_playhead_loop` (default = false / Follow). Effective mode derived in `isPlayheadLooping()` (ARA→follow, standalone→loop, plugin→param).
  - Edge case handled: at the loop wrap boundary with auto-scroll ON, the `L -> 0` jump is treated as normal advance (not a seek) to avoid a recadrage flicker each loop.

## 6. Scale Keyboard Component

- [x] 12-button chromatic keyboard display
- [x] Toggle individual scale notes
- [x] Blue highlight for active scale notes
- [x] Bidirectional sync with AudioParameterInt (custom0..custom11)
- [x] Auto-switch to Custom mode on user interaction
- [x] Scale/Key ComboBox -> parameter binding fixed 2026-07-11 (hardened 2026-07-11): the combo -> parameter direction is owned by JUCE's `ComboBoxAttachment` Listener (`comboBoxChanged`), which writes `scale`/`key` on genuine user selection. The `onChange` handlers were changed to ONLY mirror the per-note custom flags / piano keys and never write `scale`/`key` back. This removes a transient morph-regression where `onChange` (which JUCE fires during morph/automation via `sendNotificationSync`, unguarded) could momentarily overwrite the morph's new scale while the display lagged. Key getters corrected to read the normalized value (`round(load*11)`) so non-C roots work.

## 7. Build and Release

- [x] CMake build system
- [x] GitHub Actions CI (Windows, macOS)
- [x] Release workflow (tagged builds)
- [x] Version bump workflow
- [x] Installer for macOS (.pkg)
- [x] `scripts/build_helper.cmd` reconciled with the remote (CI/release) version 2026-07-12: the machine-specific Windows build helper was restored to `origin/main` (commit `aeeb438`) via `git checkout -- scripts/build_helper.cmd` so the installer/CI build matches the committed configuration.

## 9. UI Polish (2026-07-15)

- [x] Expansion handle (Correction "Advanced" banner) is now half the block height with rounded corners (`fillRoundedRectangle`, trimmed 25% top/bottom).
- [x] Top toolbar reflows on language change — the language handler now calls `resized()` after `refreshLabels()`, so the transport / "Measures" controls no longer overlap the tabs and the "Measures" label is no longer truncated (e.g. Spanish "Compases" → "Co..."). Tab-end offset made robust with `tabBar.getX()`.
- [x] Harmony "Follow Lead" toggle is now visible — the Harmony block's first row spans the full block width; the Harmony on/off button is sized to its text and "Follow Lead" takes the rest (previously a fixed 130px on/off button squeezed it to ~0 width on the rightmost block).
- [x] "Clean Curves" / "Reset Playhead" menu items translated (added FR/DE/ES/JA; they already existed only in English).
- [x] Key/Scale Detection power button height 22 → 18 (matches Gate/Reverb); clicking the "Key/Scale Detection" label toggles the power button (same behaviour as the other Power buttons).
- [x] Harmony power button height → 18 (matches Gate/Reverb) and left-aligned on the first row (no longer centred above the combo).
- [x] Curve Editor CTRL+wheel zoom inverted vs Live visualizer — flipped the wheel factor sign so wheel-up zooms in, matching the Live tab.
- [x] Piano Roll horizontal grid lines now identical to Curves mode (C notes = `curveGrid`, in-scale = `scaleLine`, full alpha; off-scale notes draw no line), so no faint/off-scale rows appear when switching to Piano Roll.

### 9b. UI Polish — 2e passe (2026-07-15)
- [x] Key/Scale Detection combo moved directly under the power/label row (removed the 4px gap) and kept at the same 24px height as the Key/Scale combos; `detRow` tightened 24 → 22px.
- [x] "Follow Lead" toggle moved below the "number of voices" combo in the Harmony block (first row now holds only the Harmony on/off toggle).
- [x] Curve Editor scroll no longer "stacks" out-of-range notes/curves/points at the top/bottom edges — removed the `pitchToY` clamp (Curves mode) and made `PianoKeyboard::midiToNorm` extrapolate (no clamp) so off-screen items are clipped, not pinned to the edges.
- [x] MIDI OUT "Follow Lead" behaviour documented: ON → harmony MIDI notes vary with the lead (`charRatio = f0_out / leadScaleFreq`); OFF → harmony notes locked to the scale grid (fixed). No code change (answering the question).

### 9c. UI / DSP Polish — 3e passe (2026-07-15)
- [x] "Follow Lead" toggle left-aligned in the Harmony block (sized to its content, placed on the left of its row instead of spanning the full column width).
- [x] MIDI OUT now ignores "Follow Lead": pushed harmony notes are always clean scale-locked notes (`harmonyFrequenciesClean` / `lastHarmonyNotesClean`), independent of the toggle; the on-screen visualizer still follows the lead when the toggle is on.
- [x] Tooltips (and the "Attack" button text) refresh on language change: added `advancedButton` + `harmonyFollowLeadButton` tooltips to `refreshLabels()`, added the `pianoRollButton` tooltip (and text) to `PitchCurveEditor::refreshTranslations()`, and `attackAwareButton` button text now refreshed.
- [x] "Measures" label adapts to content (no truncation in DE/JA): `refreshLabels()` now re-runs `resized()`, and the width padding was bumped (+6 → +10px).
- [x] Key Detection — OpenVoxKey now packaged in the Windows installer (new `companion` component in `installer/OpenVoxTuner.iss` + `OpenVoxKey_VST3` built by `scripts/build_installer.ps1`); the duplicate mono `Input`/`Output` buses were removed so the `Sidechain` bus is now input index 1 (matching all sidechain code) — fixes SideChain routing in Studio One.
- [x] KeyBridge shared memory now active (2026-07-15): the Windows named memory-mapped file `Local\OpenVoxTunerKeyBridge` was previously disabled by a leftover debug guard (`if (false && ...)`) so each VST3 used its own private in-process region and the companion's `publish()` never reached `read()` in OpenVoxTuner. Guard removed → both separate binaries (OpenVoxKey.vst3 + OpenVoxTuner.vst3) now share one session-local region; the in-process fallback is kept only when shared memory is unavailable. Unit-test `KeyBridgeTest.cpp` (and `SidechainBusLayoutTest.cpp`) were re-added to `CMakeLists.txt` and now compile/pass — 86 OK / 0 KO.
- [x] Harmony blue lines no longer "drop" when "Follow Lead" is active: `charRatio` now uses the scale note nearest to the continuous `f0_out` (`scaleQuantizer->quantize(f0_out)`) instead of the jumping quantizer target, so the ratio stays ≈1 across note transitions.
- [x] OpenVoxKey companion — "Send" button (2026-07-15): added `OpenVoxKeyProcessor::forcePublish()` which re-publishes the last detected key/scale to `ovtdsp::KeyBridge` immediately, bypassing the change-guard in `processBlock` (which only publishes on detection change). The companion editor gained a "Send" `TextButton` to the right of the Group combo (`buttonClicked` → `processor.forcePublish()`) so the user can manually re-sync OpenVoxTuner (Key/Scale Detection = OpenVoxKey, matching group) even after changing the scale by hand there. No-op until at least one key has been detected. Button is created disabled and `timerCallback()` enables it (`sendButton.setEnabled(key >= 0)`) as soon as a key is detected, and disables it again if detection is lost (the enable call was moved above the early `return` in the detected branch, which previously left the button stuck disabled after first detection).
- [x] OpenVoxTuner Sidechain — red debug LED (2026-07-15, **removed 2026-07-15**): a `DebugLed` + "Sidechain audio" label was added to confirm the sidechain bus received audio; once Sidechain detection worked it was removed per user request (no behavioural change to detection). See below for the group-B/C fix that followed.
- [x] Key/Scale Detection — detected scale now re-asserts after a manual change (2026-07-15): `applyDetectedKey()` guard changed from a cached `lastAutoKey/lastAutoScale` to the live `key`/`scale` parameter values, so while Key/Scale Detection is on the detected scale is authoritative and a hand edit is re-applied on the next estimate (identical values still skipped → no automation churn). To keep a manual scale, turn detection off.
- [x] OpenVoxKey companion — animated "searching for key" (2026-07-15): `OpenVoxKeyProcessor` logs `lastAudioTime` (system-clock seconds, `std::atomic<double>`, accessor `getLastAudioTime()`) when the input bus carries signal above a noise floor; the editor's `timerCallback()` now shows the detected key (white) when known, an animated `SearchingDots` (cyan, three dots pulsing in sequence) when audio is present but no key yet, and a dim "No signal" when the bus has been silent > 2 s — replacing the old static "-".

## 8. Documentation

- [x] Architecture overview
- [x] Default parameters documentation
- [x] Multi-engine architecture docs
- [x] Pitch detection rollback guide
- [x] Pitch shifting feasibility study
- [x] Deployment and packaging guide
- [x] macOS AU and installer guide
- [x] macOS VST3 build guide
- [x] ARA specifications
- [x] GitHub setup and release guide
- [x] Changelog (per-day format)
- [x] Implementation roadmap (this file)
- [x] Pitch Visualizer improvements documentation

## 9. Future Improvements (Backlog)

### Prioritized Feature Backlog (2026-07-14)
User-ranked wishlist (audio + UI). **Implementation order (decided 2026-07-14):**
**3 Attack-aware correction -> 2 Automatic key detection -> 6 Modern EQ view -> 4 Piano-roll -> 5 Preset gallery -> 1 MIDI target -> 7 LV2** (LV2 under reflection, may not be implemented).

**Audio**
- [x] Attack-aware correction (3):
- [x] Automatic key detection (2): **2026-07-15 fix:** OpenVoxKey group B/C cross-talk — `companionGroupParam->load()` returns the 0..3 choice index (not a normalised 0..1 value), so the old `* 3.0f` mapped B->D and C->D. Now uses the index directly. Verified with a new KeyBridge A/B/C/D regression test.
- [x] MIDI target / follow (1): optionally drive the pitch target from an external MIDI note (play "through" the plug-in) instead of the detected pitch. **2026-07-15:** DSP parameter `midi_target_enable` implemented in `processBlock` (held MIDI note overrides scale-quantized target). UI entry is the hamburger menu "MIDI Target" item (the legacy top-bar toggle/icon buttons were removed as dead code — they were never positioned/visible). Works on both Live and Curve Editor tabs (global DSP param).
- [x] Vibrato preservation

**UI**
- [x] Modern EQ / spectral waveform display (6):
- [x] Piano-roll editing mode (4):
- [x] Correction block UI redesign (2026-07-14):
- [x] Preset gallery (5): browsable grid of factory/custom presets with thumbnails/metadata. **2026-07-15:** FactoryPresets registry + PresetGallery component (opens from hamburger "Preset Gallery" and toolbar button). Fixed: clicking a card now loads the preset (sub-components no longer intercept mouse events); delete confirmation dialog is parented to the gallery window so it appears in front.

**Platform**
- [ ] LV2 plugin format (7, under reflection): open-source Linux standard format (not natively supported by JUCE — requires an external wrapper such as JUCE-LV2 or carla/lv2host). Revisit once the items above are implemented.

### Medium Priority
- [ ] Bookmark positions (save/restore frequency range presets)
- [x] Dark/Light theme toggle
- [ ] Responsive layout adaptation for small screens
- [ ] Note name labels on additional piano keys (D, E, F, G, A, B)

### Low Priority
- [ ] Touch gesture support (pinch-to-zoom, drag-to-scroll)
- [x] Export visualizer as image/screenshot
- [x] Multi-language UI support
- [ ] Accessibility improvements (keyboard navigation, screen reader)
