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

## Formant preservation — deep analysis & benchmark (2026-07-27)

- **Analysis report**: `docs/formant-preservation-analysis-report.md` — studies LPC (cross-synthesis, source-filter decoupling) and temporal smoothing/interpolation for formant preservation; compares them to the deployed solutions (`FormantPreserver` `1/sqrt(r)` pre-warp with fixed male-default centers + `PitchShifter` `formantRatio` grain speed); identifies gaps (fixed centers G1, partial `1/sqrt(r)` G2, no real source/filter decoupling G3, double-application risk G5, no perceptual/metric validation G7); proposes prioritized, actionable improvements (P0 voice-type-aware centers + full `1/r`; P1 LPC module C0 + temporal LPC interpolation C1 + disambiguate chains; P2 pre-emphasis, MUSHRA harness, hybrid fallback) with validation steps and success metrics.
- **Benchmark**: `test/formant_preservation_benchmark.py` (pure numpy, no scipy) — source-filter vowel synthesis (parallel Klatt resonators), 5 methods (naive B0, filter `1/sqrt(r)` B1, filter `1/r` B2, LPC cross-synth C0, LPC+interp C1) vs an ideal reference, across male/female/child voices and ratios {0.75,1.0,1.5,2.0}. Metrics: global LSD, formant-band LSD, warble (5 Hz modulation depth), CPU ms/s, noise robustness (SNR 20/10 dB).
  - **Result**: LPC cross-synthesis preserves formants **~2.5x-5x better** (LSD) than the current `1/sqrt(r)` approach at large transposition ratios, across all voice types. At r=2.0 male: C0=3.10 dB vs B1=8.24 dB vs B0=12.24 dB. Temporal LPC interpolation (C1) is <= C0 at large upshifts. At r=2.0 female, the `1/sqrt(r)` pre-warp (B1=9.53 dB) is *worse* than naive (B0=4.87 dB) in formant bands — evidence the fixed-ratio approach is unreliable across voices.
  - **Fix during development**: the LPC whitening excitation must use `e = x + sum a_k x[n-k]` (PLUS sign), not minus; otherwise the envelope is inverted. Also required: recursive output-feedback resonators (not FIR input-delay) for vowel synthesis, and frame-based (not global) LPC envelopes to avoid spurious high-frequency poles.
- **Roadmap**: `docs/implementation-roadmap.md` section 8l "LPC formant preservation" added with LP.1-LP.8 tracked items mapping the P0/P1/P2 recommendations.

## Formant Strategy selector — Current / P0 / P1 / P2 (2026-07-27)

Added a user-facing **`formant_strategy`** AudioParameterChoice (UI combo box in the Advanced section, after Voice Type) letting the user pick the formant-preservation method per the P0/P1/P2 recommendations of the analysis report. The selector drives BOTH the lead voice and the harmony voices (when Harmony is enabled), so harmonies transposed an octave up/down also benefit.

Mapping (index → method):
- **0 — Current**: pre-warp `1/sqrt(r)` with fixed male-default formant centers (pre-PSOLA, `FormantPreserver`).
- **1 — P0**: full `1/r` compensation + voice-type-aware formant centers (`FormantPreserver::Strategy::P0`, uses `voiceTypeTable`).
- **2 — P1 (LPC C0)**: LPC cross-synthesis after PSOLA. The pitch-shifter runs at ratio 1.0; `LpcFormantPreserver` whitens with the input's LPC and re-colors with the **pre-shift reference** LPC (`leadReferenceBuffer` snapshot). The creative formant shift is then re-applied (`FormantPreserver::process(buffer, 1.0f)`) so users can still move formants.
- **3 — P2 (LPC C1Hybrid)**: P1 + C1 temporal LPC-coefficient interpolation (smoothed across blocks via `aOrigPrev`/`aShiftedPrev`) + pre-emphasis/de-emphasis (0.97) + hybrid passthrough fallback when the frame RMS is below `hybridRmsFloor` (1e-3) or the residual gain `scale` is outside `[1/hybridMaxScale, hybridMaxScale]` (silent/unstable frames).

### Files modified / added
- **Added** `Source/dsp/LpcFormantPreserver.h` / `.cpp`: LPC cross-synthesis core. `Mode { C0, C1Hybrid }`, per-channel history, Levinson-Durbin `computeLpc` with reflection-coefficient clamp (`jlimit(-0.999, 0.999)`), band-width expansion (`bwLambda=0.98`), residual-gain match (`gRef/gIn`), pre-emphasis (P2 only). Processes the whole block as one LPC frame (order 18).
- **Modified** `Source/dsp/FormantPreserver.h` / `.cpp`: `Strategy { Current, P0 }`, `setStrategy`/`getStrategy`/`setVoiceType`, `voiceTypeTable[6][4]`, `getFormantFreqHz(f)`; `compensationRatio = (P0) ? 1/r : 1/sqrt(r)`.
- **Modified** `Source/PluginProcessor.h` / `.cpp`: new `formant_strategy` APVTS choice param; `LpcFormantPreserver` instances (lead + per-harmony array); `leadReferenceBuffer` / `harmonyWarpBuffer`; lead branch (`useLpc` → PSOLA at ratio 1.0 → LPC cross-synth → re-apply creative) vs pre-PSOLA warp branch; harmony branch (C0/C1Hybrid / P0 pre-warp / Current).
- **Modified** `Source/PluginEditor.h` / `.cpp`: `formantStrategyBox` combo (items, colors, tooltip) + `ComboBoxAttachment` on `"formant_strategy"`; placed in Advanced row 3 (after Voice Type); visibility follows the Advanced fold state.
- **Modified** `CMakeLists.txt`: `LpcFormantPreserver.cpp` / `.h` added to both the main plugin (`OpenVoxTuner`) and the `OpenVoxTunerTests` sources.
- **Modified** `test/Main.cpp`: includes `dsp/LpcFormantPreserverTest.cpp`.
- **Added** `test/dsp/LpcFormantPreserverTest.cpp`: passthrough-bounded at ratio 1.0, formant-peak-preservation (dominant spectral peak moves from ~1400 Hz input to ~700 Hz reference after cross-synth), P2-silent no-explode.
- **Modified** `test/dsp/FormantPreserverTest.cpp`: new `P0 strategy (1/r + voice-type) differs from Current (1/sqrt(r))` test (uses MultiFormant mode + smoothing warm-up).

### Tests
- All 144 unit tests pass (previously 139 + new LPC & P0 tests).
- Key regression guards: P0 output differs measurably from Current at ratio 2.0; LPC cross-synthesis does not produce NaN/inf and preserves the formant peak; P2 stays silent on silent input.

### Notes / deviations
- **LPC block framing**: implemented as a single whole-block LPC frame (no hop / Hann overlap-add), whereas the report's LP.3 spec suggested order 10 with ~100-sample hop. Order 18 was chosen for stable formant capture; the overlap-add refinement remains future work (LP.7 MUSHRA/CI harness still pending).
- **Sign convention**: after implementation the whitening uses `e = x - sum a_k x[n-k]` and re-color `y = e + sum a_k y[n-k]` under the standard MINUS LPC convention (`k = (R[m] - sum a_i R[m-i]) / E`), with reflection clamp — this corrected an initial NaN/inf blow-up.