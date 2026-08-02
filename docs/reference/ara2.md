# ARA2 Support

OpenVoxTuner implements **ARA2** (Audio Random Access) for DAW timeline
integration. This page summarizes the design from `docs/ARA_Specifications.md`
and the ARA code in `Source/PluginProcessor.cpp`.

!!! warning "Hybrid model — ARA does NOT bypass the real-time pipeline"
    There is **no `processBlockForARA()`** and `processBlock()` does **not**
    branch or early-return for ARA. The single entry point always runs the
    full DSP pipeline (`NoiseGate → YIN → ScaleQuantizer → RetargetEnvelope →
    PitchShifter/PSOLA → HarmonyEngine → ReverbEffect`). ARA only **augments**
    the input stage with metadata and a waveform cache.

## How ARA augments the DSP

When `isBoundToARA()` is true, `processBlock()` gains two additions that run
*before* the DSP chain:

1. **Metadata reads** — the host's key and bar (time) signatures are read from
   the `ARAMusicalContext` and pushed into the `key` / `scale` parameters and
   the `araBarSignatures` cache (used by the Curve Editor ruler).
2. **`araWaveformBuffer` cache** — a mono downmix of the input block is stored
   so the visualizer can display the waveform.

Because the DSP path is identical, the audio result is the same in ARA and
non-ARA modes. ARA's value is **metadata-driven** (key/scale/bar sync) plus
the waveform cache, not a separate offline analysis path.

## Clip vs. Track behavior (ARA hierarchy)

| Instantiation | DAW behavior | Plugin access |
|---------------|--------------|---------------|
| **Clip / Event** (e.g. Studio One) | Host creates an `ARARegionSequence` containing only that clip. | The plugin sees/analyzes only the audio within the clip; the editor shows this region. |
| **Full Track** (e.g. Logic Pro, Cubase) | Host creates an `ARARegionSequence` covering all clips on the track. | The plugin sees the whole track timeline and all vocal events. |

The UI editor iterates over the root `ARADocument` and aggregates all
`ARARegionSequence` objects assigned to the current instance, so the display
always matches what the DAW has decided to provide — no user intervention.

## Key signature extraction

Only **`ARAKeySignature`** (key) and **`ARABarSignature`** (time signature) are
read, via `kARAContentTypeKeySignatures` and `kARAContentTypeBarSignatures`.

- The root pitch class is derived as `chromatic = ((root * 7) % 12 + 12) % 12`.
- **Scale derivation is limited** to three cases, resolved from the key
  signature intervals:
  - 12 active pitch classes → `Chromatic` (index 0)
  - Major third present (`intervals[4]`) → `Major` (index 1)
  - Minor third present (`intervals[3]`) → `Natural Minor` (index 4)

  Any other scale (melodic/harmonic minor, modes, pentatonics, Blues, Custom…)
  is **not** inferred from ARA; the user must select it manually.

!!! note "Chord extraction is NOT implemented"
    There is no `ARAChord` / `kARAContentTypeChords` reader anywhere in the
    codebase. The plugin never reads per-chord harmonic information from the
    host.

## Measures ruler & time-signature awareness

- Bar signatures are read from the `ARAMusicalContext` on the **UI thread only**
  (`updateAraMetadata()`), because `HostContentReader` acquires a lock that can
  deadlock the audio thread in some hosts (Cubase LE 15, Live VST3).
- Each bar signature event is stored as
  `{ position (PPQ), numerator, denominator }` in `araBarSignatures`.
- `getTimeSignatureAt(ppq, num, den)` scans the signature events and returns
  the **active** numerator/denominator at any playhead position.

### Multi-signature support

Because bar signatures are stored as a **list of events** indexed by PPQ
position, the plugin supports **multiple time-signature changes** across a
project: the ruler and the playhead follow the signature that is active at the
current position rather than assuming a fixed 4/4.

## Playhead follow

- When bound to ARA, the editor **follows the host timeline** (no standalone
  transport). The Curve Editor receives the playhead via
  `setPlayheadTime(time, isHostPlaying, isLooping)`; auto-scroll is active only
  while the DAW is actually playing.
- Time is derived from the playhead position in **seconds** (from
  `getPlayHead()->getPosition()`), which in Graphic mode drives
  `PitchCurve::getPitchAt(t, f0_in)`.

## Compatibility matrix

| DAW | ARA2 support | Key & bar extraction | Chord extraction |
|-----|--------------|----------------------|------------------|
| Studio One (PreSonus) | Native (Clip & Track) | Yes | **Not implemented** |
| Cubase / Nuendo | Native VST3 | Yes | **Not implemented** |
| Logic Pro | ARA2 AudioUnit (Track recommended) | Yes | **Not implemented** |
| Reaper | ARA2 VST3 | Yes | **Not implemented** |
| Ableton Live / FL Studio | Real-time fallback (no ARA metadata) | — | — |

## Build-time enablement

ARA is compiled conditionally behind the CMake flag `OVT_ARA_ENABLED`. When
enabled, `OpenVoxTunerARADocumentController` (a
`juce::ARADocumentControllerSpecialisation`) is defined and
`createARAFactory()` is exported via the JUCE macro. When disabled,
`updateAraMetadata()` becomes a no-op and the plugin behaves as a plain
real-time effect.
