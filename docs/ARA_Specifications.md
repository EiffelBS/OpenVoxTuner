# Technical Specifications: ARA2 Support (Audio Random Access)

## 1. Fallback mechanism for non-ARA DAWs
The plugin uses a "Hybrid" architecture. When inserted into a DAW that does not support ARA2 (such as Ableton Live or FL Studio), or when used in Standalone mode, the plugin operates as a standard real-time VST3 effect.

**Technical implementation:**
In the `processBlock` method, the plugin calls `processBlockForARA()`.
- If the host provides a valid ARA context and audio regions are assigned, ARA processing is performed and the method returns `true`. Real-time processing (YIN, etc.) is then bypassed.
- If the method returns `false`, the plugin continues its normal sequential execution, capturing the current block's audio, performing real-time YIN analysis, and applying correction via `PitchShifter`.
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
- `ARAKeySignature` (Key) and `ARAChord` (Chords) data are extracted.
- If the project contains a defined key (e.g. C Major), the plugin's `ScaleQuantizer` module automatically locks onto that scale, eliminating the need for the user to select it manually in the interface.

## 4. Compatibility matrix and validation
- **Studio One (PreSonus)**: Native ARA2 support (Clip & Track). Chord and key extraction 100% supported.
- **Cubase / Nuendo (Steinberg)**: Native ARA2 VST3 support. Chord track extraction supported.
- **Logic Pro (Apple)**: ARA2 AudioUnit support. Track instantiation recommended.
- **Reaper (Cockos)**: ARA2 VST3 support. Excellent for offline audio; chord extraction limited depending on configuration.
- **Ableton Live / FL Studio**: Automatic real-time fallback (standard processing with no loss of functionality).
