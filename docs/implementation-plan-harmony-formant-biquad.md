# Implementation plan — Harmony Formant via per-voice biquad (Option B)

Status: **planned — not implemented** (2026-08-03)

## Goal

Replace the harmony voices' **granular formant preservation** (grain read-speed
scaling inside `PitchShifter`) with a **filter-based formant shift** (per-voice
`ovtdsp::FormantPreserver` biquad), so the Harmony Formant knob works fully and
stably on **all** harmony types — including single octave-shifted voices
(Drone, Octave Below, Octave Above), which are the ones that artifact with the
granular method at extreme values (−5).

## Context / why

- Harmony formants are currently shifted inside `PitchShifter::process` by
  scaling the grain read speed: `grains[v].speed = F`, `F = 2^(harmonyFormant/12)`
  (`Source/dsp/PitchShifter.cpp`).
- On a voice that is also strongly pitch-shifted (octave → pitch ratio 2.0 / 0.5),
  the granular read-speed formant breaks the OLA COLA sum → wobble / rapid pops,
  lateralized per ear (HC.13).
- Option A (blend the granular formant ratio toward 1.0 by pitch deviation)
  reduced the artifact but **cannot** deliver a full −5 formant shift on octave
  voices — it reduces exactly where the user wants it. It also made the knob
  effectively dead on mono octave types (regression, mitigated by a 0.5 floor).
- Option B applies the formant shift with a **stable biquad filter** after/before
  the pitch shift, decoupled from the granular read-speed, so it works uniformly
  regardless of the voice's pitch ratio.

## Proposed design — B1: pre-warp biquad per voice (matches the lead voice)

The lead voice already uses the proven pre-warp path
(`Source/PluginProcessor.cpp`):
```
formantPreserver.setFormantShift(shiftSemitones);
formantPreserver.process(buffer, ratio);       // pre-warp input formants
pitchShifter->process(buffer, ratio, userFormantRatio, f0);
```
We mirror this per harmony voice, with the **Harmony Formant** value as the shift
and the granular formant ratio forced to 1.0.

Per shifted voice `v`:
1. Copy the shared input `synthWorkBuffer` into a per-voice staging buffer.
2. Pre-warp: `formantPreserverHarmonyVoices[v].process(staging[v], ratioH)` with
   `setFormantShift(harmonyShiftSemitones)`.
3. Pitch shift with **no granular formant**: 
   `shiftedVoicePitchShifters[v]->process(staging[v], tmp, ratioH, 1.0f, safe_f0)`.

Net effect: the voice's pitch is shifted by `ratioH` and its formants end up at
the `harmonyShiftSemitones` offset — identical intent to the current granular
method, but using the stable biquad, and it works at octave shifts.

### Design decision recorded

- **B1 (pre-warp, above)** is preferred because it *preserves* formants relative
  to the source (an octave-up voice keeps human formants, just offset by the
  knob), matching the lead voice and the current granular behaviour.
- **B2 (post-shift)**: applying the biquad to the already-pitch-shifted `tmp`
  would shift the *chipmunk* formants (shifted by the pitch ratio) rather than
  preserve them → different/incorrect sound for octave voices. Rejected as
  primary, kept as a fallback only if B1 sounds wrong.

## File changes

### `Source/PluginProcessor.h`
- Replace the single `formantPreserverHarmony` member with:
  `std::array<ovtdsp::FormantPreserver, maxShiftedVoices> formantPreserverHarmonyVoices;`
  (each voice needs its own filter state — a shared instance would bleed state
  across voices).
- Add per-voice staging buffers for the pre-warped input:
  `std::array<juce::AudioBuffer<float>, maxShiftedVoices> shiftedVoiceFormantInput;`
  (resized in the voice loop like `shiftedVoiceBuffers`).

### `Source/PluginProcessor.cpp`
- `prepareToPlay()`: loop over `formantPreserverHarmonyVoices`, call
  `prepare(sampleRate, samplesPerBlock)` and apply the same mode/strategy/
  voiceType/Q/smoothing configuration currently applied to `formantPreserverHarmony`
  (lines ~2324–2387).
- `reset()`: call `reset()` on each voice instance.
- `syncParameters()` / the formant config block: configure all voice instances
  instead of the single one.
- Voice loop (`Source/PluginProcessor.cpp` ~line 2608):
  - resize `shiftedVoiceFormantInput[v]`, `copyFrom(synthWorkBuffer)`.
  - `formantPreserverHarmonyVoices[v].setEnabled(true)`,
    `setFormantShift(harmonyShiftSemitones)`,
    `process(shiftedVoiceFormantInput[v], ratioH)`.
  - call `shiftedVoicePitchShifters[v]->process(shiftedVoiceFormantInput[v], tmp, ratioH, 1.0f, safe_f0)`.
- **Revert Option A**: remove the `voiceFormantRatio` blend (HC.13); always pass
  granular formant ratio `1.0f`. Option A is superseded by Option B.

## Configuration reuse

The existing formant config block (mode / strategy / voice type / Q multiplier /
smoothing alpha) already targets the harmony path. It is re-targeted from the
single `formantPreserverHarmony` to the per-voice array. The `harmony_formant`
APVTS parameter value (`harmonyShiftSemitones`) is unchanged and feeds
`setFormantShift`.

## CPU / performance

- Adds up to 4 biquad pre-warp passes + 4 buffer copies per block (one per active
  shifted voice). The lead already does 1 pass; 4× that is modest for modern CPUs.
  Mitigation if needed: reuse the per-voice `tmp` buffer for staging in-place is
  **not** possible (input must stay pristine for other voices), so staging buffers
  are required.

## Risks & validation

- **Formant character may change** vs the granular method on close harmonies.
  Validate by ear across all types at multiple Harmony Formant values (±5).
- **Octave voices** (Drone / Octave Below / Octave Above): confirm the wobble/pop
  is gone and the knob is clearly audible.
- **Formant direction**: verify the knob moves formants in the expected direction
  (down at −5) on both octave-up and octave-down voices.
- **CPU**: check the standalone doesn't hit the real-time budget.
- If B1 produces an unexpected sound, try **B2** (post-shift on `tmp`, passing
  `ratio = 1.0f` to `process` to skip pitch compensation).

## Rollback

Option A's blend is small and localized (one expression + comment). If Option B is
unacceptable, it is easy to restore by keeping the granular formant ratio path and
removing the per-voice biquad. Both paths can coexist behind the `formant_strategy`
parameter if desired, but the default should be a single stable path.

## Comparison option (A/B at runtime)

To validate B without losing the current sound, add an APVTS `AudioParameterChoice`
`harmony_formant_method`:

- `"Granular (current)"` — the existing HC.13 path (per-voice formant-ratio blend,
  floor 0.5) via the grain read-speed in `PitchShifter`.
- `"Biquad (per-voice)"` — the new HC.14 per-voice `FormantPreserver` pre-warp.

The voice loop branches once per block on the selected method. This lets the user
flip between the two while singing (no recompile / reload), which is the cleanest
way to validate that the biquad path both removes the wobble/pop and keeps the
formant knob fully working on all types (including Drone / Octave Below / Octave
Above).

