# Changelog - 2026-07-27

## Added
- **Harmony Attack Parameter** (`harmony_attack`): New configurable knob (1-300 ms, default 35 ms) placed under the Blend knob in the Harmony section. Controls the per-voice progressive fade-in duration for each harmony voice individually.
- **Smoothstep (Raised-Cosine) Per-Voice Fade-In**: Replaced the previous linear fade-in with a smoothstep ramp (zero slope at both ends) so that multiple voices starting simultaneously no longer constructively reinforce at the transient, eliminating the "thud" at note onset.
- **Gate-Open Retrigger Logic**: The per-voice attack envelope now re-arms on every gate-open transition (and on true silence restart), preventing clicks when the Noise Gate quickly re-opens after a short close.
- **Gate-Follow Clamp**: When the Noise Gate module is enabled, the effective harmony attack is clamped to a short 12 ms gate-follow time so harmony voices swell *together with* the gated dry signal instead of arriving late and creating a perceived volume surplus.

## Fixed
- **Gate + Harmony Volume Surplus**: Eliminated the systematic volume surplus at harmony voice attacks when both Gate and Harmony modules are active. The harmony envelope now tracks the gate envelope (no late swell) for *all* harmony profiles (ThirdBelowAbove, VocalStack4, PowerChord, UnisonOctaves4, etc.).

## Technical Details
- Modified `HarmonyEngine::renderHarmonies()` signature to accept `gateActive` boolean.
- Added per-voice smoothstep state: `attackTotalSamples`, `attackSamplesRemaining`, `attackStartAmp`, `voicePrevGate`.
- Added `gateFollowMs = 12.0f` constant for gate-coupled attack clamping.
- New `harmony_attack` AudioParameterFloat (range 1-300 ms) persisted in APVTS and PresetMorpher state.
- UI: Harmony Attack knob added under Blend knob in PluginEditor, fully wired to APVTS with tooltip.
- Regression test `HarmonyAttackTest` covering: (1) gate+hormony tracking for all profiles, (2) harmony-only smooth fade-in, (3) gate re-open retrigger, (4) long user attack (200 ms) still clamped in gate mode.

## Tests
- All 139 existing unit tests pass.
- New `HarmonyAttack` test added and passing.