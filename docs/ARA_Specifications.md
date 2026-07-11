# Technical Specifications: ARA2 Support (Audio Random Access)

## 1. Processing model — ARA does NOT bypass the real-time DSP pipeline
The plugin uses a "Hybrid" delivery model. When inserted into a DAW that does
not support ARA2 (such as Ableton Live or FL Studio), or when used in
Standalone mode, the plugin operates as a standard real-time VST3 effect.
When the host provides ARA2, the plugin gains access to the host's musical
metadata and waveform cache.

**Important correction — there is no `processBlockForARA()` and `processBlock`
does NOT branch or early-return for ARA.**

- The single entry point is `processBlock()`. It always runs the **full DSP
  pipeline** regardless of ARA: `NoiseGate` -> `YIN` pitch detection ->
  `ScaleQuantizer` -> `RetargetEnvelope` (Speed) -> `PitchShifter`/PSOLA (with
  native Formant Shift) -> `HarmonyEngine` (shifted voices) -> post-processing
  effects (`ReverbEffect`, etc.).
- ARA only **augments** `processBlock` with two additions that run *before*
  the DSP chain:
  1. **Metadata reads**: when `isBoundToARA()` is true, the plugin reads the
     host's key signature and bar (time) signature via the
     `ARAMusicalContext` and pushes them into the `key`/`scale` parameters and
     the `araBarSignatures` cache (used by the Curve Editor ruler).
  2. **`araWaveformBuffer` cache**: captured before DSP, a mono downmix of the
     input block is stored so the visualizer can display the waveform.
- Because the DSP path is identical, the audio result (correction quality,
  harmony, reverb, noise gate) is the same in ARA and non-ARA modes. ARA's
  added value is metadata-driven (key/scale/bar sync) and the waveform cache,
  not a separate processing path or "offline" analysis.

The user experience is thus preserved 100% across all environments.

## 2. Clip vs Track behavior (ARA hierarchy)
The ARA extension allows the plugin to be inserted in two distinct ways depending on the DAW:

### A. Instantiation on a Clip / Event (e.g. Studio One)
- The user drags the plugin directly onto an audio event in the timeline.
- **ARA behavior**: The host creates an `ARARegionSequence` containing exclusively this clip. The plugin will only have access to and will only analyze the audio portion delimited by this clip. The graphical editor will display this specific region.

### B. Instantiation on a full Track (e.g. Logic Pro, Cubase)
- The user inserts the plugin into the effect rack of the entire vocal track.
- **ARA behavior**: The host creates an `ARARegionSequence` encompassing all clips present on this track. The plugin will have access to the entire track's audio content. The graphical editor will display the complete track timeline with all successive vocal events.

**Design note:**
The plugin's UI editor is designed to iterate over the root `ARADocument` object and aggregate all `ARARegionSequence` objects associated with the current instance. Thus, the plugin will transparently and accurately display the content that the DAW has decided to assign to it, ensuring perfect visual consistency without any user intervention.

## 3. Key signature extraction (Chord Track / Key Signature)
When the plugin is in ARA2 mode, it queries the `ARAMusicalContext` object provided by the host.

- Only **`ARAKeySignature`** (key) and **`ARABarSignature`** (time signature)
  data are extracted via `kARAContentTypeKeySignatures` and
  `kARAContentTypeBarSignatures`.
- **Chord extraction is NOT implemented.** There is no `ARAChord` / 
  `kARAContentTypeChords` reader anywhere in the codebase. The plugin never
  reads per-chord harmonic information from the host.
- If the project contains a defined key (e.g. C Major), the plugin derives a
  `scale` value and the `ScaleQuantizer` module locks onto that scale.
- **Scale derivation is limited** to three cases resolved from the key
  signature intervals:
  - 12 active pitch classes -> `Chromatic` (index 0)
  - Major third interval present (`intervals[4]`) -> `Major` (index 1)
  - Minor third interval present (`intervals[3]`) -> `Natural Minor` (index 4)
  
  Any other scale (Melodic/Harmonic Minor, modes, pentatonics, Blues, Custom,
  etc.) is **not** inferred from ARA metadata; the user must still select it
  manually in the interface.

## 4. Compatibility matrix and validation
- **Studio One (PreSonus)**: Native ARA2 support (Clip & Track). Key signature
  and bar (time) signature extraction supported. **Chord extraction: NOT
  implemented.**
- **Cubase / Nuendo (Steinberg)**: Native ARA2 VST3 support. Key and bar
  signature extraction supported. **Chord track extraction: NOT implemented.**
- **Logic Pro (Apple)**: ARA2 AudioUnit support. Track instantiation recommended.
  Key and bar signature extraction supported. **Chord extraction: NOT
  implemented.**
- **Reaper (Cockos)**: ARA2 VST3 support. Excellent for offline audio; key and
  bar signature extraction supported. **Chord extraction is not implemented in
  any configuration.**
- **Ableton Live / FL Studio**: Automatic real-time fallback (standard
  processing with no loss of functionality). No ARA metadata is used.
