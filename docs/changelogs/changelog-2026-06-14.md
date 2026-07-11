# Changelog - 2026-06-14

## Changes
- **DSP / Formant Shift** :
  - Complete separation of the Formant Shift from the Autotune chain: the Formant Shift is now handled by a dedicated instance (`formantShifter`) that runs in post-processing after the Autotune. This solves the bug where the Formant Shift cancelled the pitch correction.
  - Isolation of the Formant Shift from the autotune's formant preservation mechanism (`formantPreserver`).
  - Reduction of the Formant parameter range from `[-12, +12]` to `[-5, +5]` to avoid buffer overruns and audio clicks.
  - Complete rewrite of the formant handling in `PitchShifter::process`: use of the WSOLA technique with manipulation of the intra-grain playback speed (`currentFormantRatio`) and phase jump (`virtualInputTime`) to obtain a true "ogre" or "chipmunk" effect without cancelling the pitch correction.

- **DSP / Graphic Mode** :
  - Resolution of audio artefacts (clicks, scratch) in Graphic mode: fix of a major bug where the phase alignment search window (WSOLA) was restricted to `0.5` period, preventing it from finding phase continuity during the brutal ratio changes dictated by the curve. The window was extended to a minimum of `1.2` period.
  - Restoration of the intra-grain playback speed to `1.0f` for the Autotune to guarantee perfect smoothing of the natural formants and prevent out-of-range temporal jumps.

- **DSP / Formant Shift & Autotune** :
  - Major rework of the `PitchShifter` algorithm to solve the loss of autotune and the "echoes/scratch" at the end of notes.
  - Replacement of the rotating write pointer (`writePos`) by an absolute write pointer (`absoluteWritePos`) and switching `virtualInputTime` to `double` instead of `float`. This definitively eliminates the buffer wraparound bugs that caused the random repetition of old audio fragments (the "echoes" heard after singing stopped).
  - Removal of the `FormantPreserver` equalizer and the second `FormantShifter` instance. The granular engine now performs Formant Shifting and Pitch Shifting in a single pass, in a perfectly mathematical way and with superior quality, which drastically reduces CPU consumption.

- **UI / Graphic Mode & Ergonomics** :
  - **Pro-Q3-inspired spatial reorganization** : The tools specific to the graphic editing mode (`Snap to scale`, `Clear Curve`, `Reset Playhead`) were completely removed from the bottom block of the interface. They are now integrated horizontally in the top banner (Top Bar), aligned to the right next to the Bypass button.
  - This reorganization made it possible to remove an entire interface block at the bottom, offering much more breathing room (padding) to the processing rotary buttons (`Speed`, `Amount`, `Formant`) and to the scale selection keyboard (`Scale`).
  - **Strengthened conditional display** : These three graphic tools placed in the top bar appear *exclusively* when the "Graphic (Advanced)" tab is active. In "Auto (Live)" mode, the top bar becomes perfectly clean again so as not to distract the user.
  - **Display fix on resize** : Addition of an absolute minimum size limit (800x550) for the plugin. This prevents the buttons (knobs) and the graphic view from disappearing or being crushed when the window is shrunk too much from the DAW.
  - **Graphic rework (Studio One theme)** : The UI was completely modernized to match your references (Fat Channel, Compressor, Vocal Tune). The backgrounds are now a very professional matte gray/black (`#1A1A1A`), the active elements (progress bars, combobox) light up in cyan blue (`#1A9AF0`), and the inactive buttons are grayed out. The knobs (potentiometers) were redrawn in a "flat" style with a lighted tip.
  - **Formant Power button** : The Formant button was turned into a real "Power" icon (open circle with vertical line) that lights up with a yellow/gold halo (like on the Fat Channel) when active, and goes dark when cut.
  - The Formant slider automatically grays out when the effect is disabled, providing intuitive visual feedback.
  - Definitive fix of the visibility and functionality of the `Reset Playhead` button: it is now grayed out (disabled) when the plugin is loaded in ARA mode (where the timeline is managed exclusively by the sequencer). In classic VST3 plugin mode, it resets the local playback by applying an internal time offset without losing the base synchronization with the host.

- **DSP / Pitch Detection (Spikes)** :
  - **Major latency improvement** : The engine's internal latency was brought from 60ms to **20ms**. The plugin now dynamically reports this value (`setLatencySamples`) to the DAW so the compensation (PDC) is perfect. This is the optimal delay to keep excellent Autotune and Formant quality while allowing live playing.
  - Addition of a median filter (Median Filter of size 5) directly in the core of the `PitchDetector` (YIN algorithm). This completely eliminates the "spikes" (random octave jumps of one or two frames) that caused tracking errors and metallic artefacts in the Pitch Shifter. The green curve in the editor will now be perfectly smooth and stable, even on difficult voices.
  - Adjustment of button visibility: `Clear Curve` and `Reset Playhead` are now exclusively visible in the **Graphic** tab.

- **Graphic editor presets (vocal tessituras)** :
  - **Analysis of old presets** : The old presets (`default`, `robot`, `spoken`, `lyric`) were based on generic frequencies (440 Hz / A4 or 200 Hz). These values were often 1 to 2 octaves too high for spoken or sung male voices (Baritone/Bass), forcing the DSP engine to "pull" excessively on the signal and creating an artificial perception.
  - **Rework and addition of realistic presets** : Creation of a complete preset menu categorized by human vocal tessituras (Soprano, Mezzo, Alto, Tenor, Baritone, Bass).
  - New flat presets (Robot): targeted on **C3 (130 Hz)** for low voice, and **C4 (261 Hz)** for high voice.
  - New spoken presets (Spoken): natural oscillation for **Man (~120 Hz)** and **Woman (~220 Hz)**.
  - New melody presets: expressive curves generated for each vocal tessitura with their respective frequency limits (e.g. Bass from E2 to C3, Soprano from C4 to G4).
  - Complete validation by unit tests: all curves load correctly and do not overwrite the editor's behavior (Snap-to-scale functional on the new curves).

- **Documentation** :
  - Update of `roadmap.md` to reflect the addition of `Reset Playhead` and the adjustment of `Clear Curve`.
