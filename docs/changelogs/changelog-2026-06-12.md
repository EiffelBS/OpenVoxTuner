# Changelog - June 12, 2026

## 1. Resolution of artefacts on non-standard buffers (Round 13)
* **Problem**: The audio processing with `RubberBand` worked at buffers 1024 and 2048, but artefacts (chops/clicks) appeared on other sizes (e.g. 1280, 1920) because the shifter requires exactly 512 samples per call, and our old circular buffer did not correctly handle the desynchronization between the host (Variable Block Size) and the shifter (Fixed Block Size).
* **Solution**:
  - Complete replacement of the manual circular buffers by `juce::AbstractFifo` in `RubberBandPitchShifter`.
  - Total separation between input (host -> inputFifo), DSP processing (inputFifo -> shifter -> outputFifo) in strict 512-sample blocks, and output (outputFifo -> host).
  - Pre-filling the output buffer with a safety silence (bufferingLatency) to avoid any risk of buffer underrun linked to VST3 consumption jitter.

## 2. GUI rework (Auto-Tune Pro style)
* **Theme & Colors**: Creation of an `AutotuneLookAndFeel` inheriting from `juce::LookAndFeel_V4`.
* **Knobs (Sliders)**: Replacement of the default display by dark rotary buttons with a peripheral glowing ring depending on the activation state.
* **ComboBox**: Redrawn to be flat and modern with a custom indicator arrow.
* **PianoKeyboard**: Full rework for a 3D effect.
  - White keys with vertical gradient and thin borders.
  - Black keys with drop shadow, asymmetric gradient and rounded right corners.
  - Fix of a bug where key height was only one pixel.
* **General Layout**: Addition of a global background gradient in `PluginEditor` (from dark to black/purple). The backgrounds of `PitchCurveEditor` and `PianoKeyboard` were made transparent to reveal the nice background gradient.
* **Visualizer**: Added a fill effect under the corrected pitch curve (neon green with transparency) for a "glow" brightness effect.

## 3. Multi-Engine Pitch-Shifting Architecture
* **Design and Interface**: Creation of a common interface `IPitchShifter` (`Source/dsp/IPitchShifter.h`) guaranteeing the interchangeability of several engines by exposing standard methods (`prepare`, `reset`, `process`, `getLatencySamples`).
* **SoundTouch integration**: Download and integration of the SoundTouch library (LGPL) directly via `CMakeLists.txt`. Creation of a `SoundTouchPitchShifter` wrapper.
* **PSOLA restoration**: Restoration of the old in-house analysis-synthesis PSOLA code, converted to use the `IPitchShifter` interface.
* **Dynamic switching**: Addition of an `AudioParameterChoice` option (`engine`) and a "Engine" dropdown (`ComboBox`) in the UI to switch on the fly between **RubberBand**, **SoundTouch** and **PSOLA**.
* **Documentation**: Addition of the `docs/multi-engine-architecture.md` file describing the operation, selection, and later addition of engines.

## 4. Fix of the alternative engines (SoundTouch & PSOLA)
* **SoundTouch**: Resolution of the "clicks" audible at each pitch change.
  - *Cause*: Without FIFO, the host received zeros during internal latencies. Moreover, the inserted FIFO was brutally purged in case of drift, and the ratio parameter was not smoothed.
  - *Fix*: Added a 50ms ratio smoothing (so as not to disturb SoundTouch's internal algorithm), and complete removal of the FIFO purge, letting the algorithm naturally stabilize the output phase.
* **PSOLA / In-house**: The original PSOLA algorithm was entirely removed because fundamentally unstable (it "chopped" as soon as a correction was applied).
  - *Cause*: The read heads "jumped" during ratio modulations without respecting the zero-crossing of the amplitude windows, and without checking phase coherence (which created "chops" and phase cancellations).
  - *Fix*: Complete rewrite as a robust "Delay-Line Crossfade" algorithm. Full implementation of **WSOLA (Waveform Similarity Overlap-Add)**: at the moment of the read-head jump (wrap), the algorithm searches for the best cross-correlation (maximal phase alignment) between the "past" signal and the "present" signal in a given time window. The optimal offset is applied before the crossfade. The result is guaranteed seamless and preserves the waveform integrity, totally eliminating the "helicopter/chops" effect.

## 5. Feasibility study of alternative algorithms
* Writing of a detailed report in `docs/pitch-shifting-feasibility-study.md` evaluating the relevance of:
  - **Time-domain algorithms**: WSOLA (recommended and close to our Delay-Line implementation).
  - **Spectral algorithms**: Rubber Band (already integrated), zplane Elastique Pro (ultimate quality but blocked by license cost).
  - **AI algorithms**: RVC / DDSP (phenomenal quality but unsuitable for a real-time "Autotune").

## 6. UX & Configuration fixes
* **Default configuration**: The "PSOLA (Legacy)" engine was renamed "Internal" and set as the default audio processing engine at plugin startup.
* **Pitch editor (PitchCurveEditor)**:
  - Repair of the drag & drop functionality of the curve points.
  - Addition of a dynamic tooltip displaying the note (e.g. C4) and the time (e.g. 1.25s) on hover and during move.
  - Clarification of the time grid with explicit display of seconds (1.0s, 2.0s, etc.).
  - Implementation of continuous time looping (modulo 4.0s) so the graphic editor is functional in Standalone mode.
  - Addition of smart snapping (Snap): time snap-to-grid (to 0.05s) and snap-to-note (to 15 cents) even when the global scale correction is disabled.
  - **Playhead**: Added a red vertical playhead bar that follows the current position of the sequencer or the continuous time in Standalone mode.
* **Scale selection rework**:
  - Complete removal of the 12 custom scale checkboxes.
  - Creation of a new `ScaleKeyboardComponent` component: a small horizontal and interactive piano keyboard.
  - In *Preset* mode (Major, Minor, etc.), the keyboard readably displays all the active notes of the corresponding scale (read-only).
  - In *Custom* mode, the keyboard becomes interactive: a click on each key (white or black) activates or deactivates the note with instant colored visual feedback, while staying perfectly synchronized with the audio engine.
  - Smart toggle: interacting with a key on any preconfigured scale automatically switches the UI and the DSP engine to "Custom" mode to preserve the user's modification.
  - Major extension of the scale list: Addition of *Melodic Minor, Harmonic Minor, Natural Minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Blues, Major Triad and Minor Triad* for a total of 15 modes + Custom.
* **Mode interface**: Removal of the redundant "Mode (Auto/Graphic)" dropdown. The mode change is now smoothly and exclusively driven by the tabs.
* **Bottom visual rework**:
  - Reorganization of the bottom controls into 3 distinct thematic blocks (Correction, Engine, Scale) for better readability.
  - Fix of the display bug (excessive height) of the dropdowns.

## 8. CPU optimization (Sleep Mode)
* **Smart silence detection**:
  - Addition of a fast RMS/Peak analysis of the audio input.
  - Automatic sleep (internal bypass) of the plugin after 500 ms of total silence (<-80 dB).
  - Complete disabling of the YIN detector, formant preservation and pitch-shifting algorithms (RubberBand, SoundTouch, Internal) during sleep.
  - **Result**: The plugin's CPU consumption drops from 14% (Internal) / 49% (RubberBand) to **~1%** when there is no audio to process, with no artefact (click or dropout) when audio resumes.
* **Dynamic Latency Declaration (PDC)**:
  - The plugin now correctly declares its processing latency to the DAW (e.g. Studio One) via the `setLatencySamples()` call.
  - The latency value adjusts dynamically according to the selected engine and is transmitted instantly to the sequencer. The DAW now correctly displays the latency (~10ms) instead of 0.0ms.

## 7. DSP correction (Quantizer)
* **Chromatic scale**: Fix of the bug that prevented the chromatic scale from allowing all notes. The quantizer was rewritten to correct the pitch to the nearest semitone, acting as a true "T-Pain" effect on all 12 notes without filtering them to the C Major scale.
* **UI/DSP synchronization**: Fix of a major bug where a scale change (Scale) in the GUI was not propagated to the audio engine. The scales (e.g. Chromatic, Major, Minor) now apply instantly.
* Fix of a unit test compilation problem caused by a `private` visibility of the color constants in `PluginEditor.h`.
* Fix of the include paths (`#include`) in the unit tests.
* Fix of a local reference return bug in `PitchCurve.h` detected by the compiler.
* Fix of the default engine initialization: the plugin silently loaded RubberBand at startup while displaying "Internal" in the UI. This caused audio artefacts on some buffers until the user forced a round-trip in the dropdown. The dynamic initialization now guarantees that the real "Internal" engine is loaded from launch.
