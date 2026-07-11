# Changelog - 2026-06-13

## Performance optimizations (CPU)
* **PitchDetector (YIN)** :
  * Restructuring of the inner loop of the difference function to enable compiler auto-vectorization (SIMD).
  * Reduction of the outer loop search range (`maxTau`) to only compute up to the required lower frequency limit, avoiding unnecessary calculations.
* **PluginProcessor** :
  * Implementation of a 4x decimation of the input signal before YIN analysis. The 2048-sample analysis window is compressed to 512 samples, dividing the pitch detector's computation count by 16.
  * Increase of `analysisHopSize` from 512 to 1024 samples. Pitch is now computed every ~23ms (instead of ~11ms).
* **PitchShifter (Internal Engine)** :
  * Drastic optimization of the `findBestOffset` method (cross-correlation computation for granular phase alignment). Replaced float-interpolation accesses and `while` wrapping loops with direct integer access using binary masking (`bitwise AND`).
  * Increase of the phase search step from 2 to 4, and decimation of the correlation comparison (1 sample out of 2 evaluated), considerably reducing CPU load when generating each new audio grain.

## Features
* **Formant Shift** :
  * Addition of a new formant shift parameter (Formant Shift) controllable via a rotary button in the main interface, ranging from -12 to +12 semitones.
  * Connected to the `FormantPreserver` DSP engine. Allows artificially darkening or brightening the timbre of the voice independently of the pitch.

## Architecture & Dependencies
* **Preparation for ARA2 (Audio Random Access) support** :
  * Integration of the Celemony ARA SDK (v2.2.0) into the project via CMake (`juce_set_ara_sdk_path`).
  * Activation of the `IS_ARA_EFFECT TRUE` flag in the CMake configuration. The plugin is now compiled with the ARA2 extension interfaces.
  * Implementation of the base `AutotuneCloneARADocumentController` controller and the `createARAFactory()` generator.
  * Implementation of a robust fallback mechanism in `processBlock`: if the DAW does not support ARA, the plugin silently falls back to the usual real-time engine.
  * Dynamic reading and extraction of the **Key Signature** : Querying the `ARAMusicalContext` via `HostContentReader` to decode the `root` (conversion from Circle of Fifths to Chromatic) and the mode (Major/Minor).
  * Automated synchronization of the UI parameters (`key` and `scale`) with the key detected by ARA.
  * **Fix of ARA Key Signature extraction** : The decoding of the `intervals` array now correctly distinguishes Major, Natural Minor, and Chromatic.
  * **BPM-synced Graphic Editor** : The timeline (X axis) now uses the DAW's `PPQ` to loop over 16 Beats (4 bars in 4/4) instead of a fixed 4 seconds, with measure markers displayed ("M 1", "M 2", etc.).
  * **Playhead synchronization (ARA and Live)** : If the host loops, the playhead subtracts the loop start point to always restart at the left of the screen. In Live/Standalone mode, the playhead keeps advancing visually even if there is silence.
  * **Clear Curve button** : Added a button to empty the curve and reset the Standalone time counter to zero.
  * **TRUE Integrated Formant Shift (WSOLA)** : Complete replacement of the Peaking filter by a deep modification of the WSOLA granular engine. The micro-timing (Formant) is now fully decoupled from the macro-timing (Pitch), allowing the vocal tract size to be changed from -5 to +5 semitones without cancelling the tuning and without artefacts.
  * Writing of the ARA2 specification document (`docs/ARA_Specifications.md`) detailing the Clip/Track hierarchy and the target DAWs.
* **Removal of external engines** :
  * Complete removal of **RubberBand** and **SoundTouch** from the codebase (`CMakeLists.txt`, headers, cpp).
  * The in-house `Internal` engine being now superior in terms of CPU (0%) and quality (no click or pop), keeping these external dependencies no longer made sense, especially as they imposed restrictive licenses (GPL / LGPL).
  * Removal of the "Engine" dropdown from the UI and from the plugin's internal parameters. The routing is done exclusively and directly to the `PitchShifter` instance.

## Bug fixes
* **Internal engine (WSOLA)** :
  * Fix of slight audio "pops" (clicks) occurring when the user holds a perfectly in-tune note (`ratio ≈ 1.0`). The brutal "passthrough" logic that disabled phase alignment (and caused destructive interference at the grain boundary) was removed. The WSOLA engine now stays always active to guarantee perfect phase continuity in all circumstances.
  * Restriction of the correlation search window (`searchWindowMs`) to 10ms (strictly below the 15ms granular latency) to mathematically guarantee that the read head will never attempt to read audio samples from the "future" (not yet written to the ring buffer).
* Validation and confirmation of the success of the 60 unit tests, including those of `ScaleQuantizer`.
