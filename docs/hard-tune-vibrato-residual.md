# Hard Tune vibrato residual

Question: how much of a singer's vibrato does the Hard Tune path actually
flatten? The preset contract says Speed 0, Vibrato 0, i.e. a flat, robotic pitch.
The benchmark below shows that the current chain **does not flatten a typical
vibrato - at 5 Hz and above it makes it deeper**, by a predictable amount.

Tool: `tools/bench_vibrato_residual.cpp` (target `bench_vibrato_residual`).
Reproduce:

```powershell
cmake --build build --config Release --target bench_vibrato_residual
build\bench_vibrato_residual_artefacts\Release\bench_vibrato_residual.exe
```

## Method

- Synthetic harmonic tone at 220 Hz (A3), 2 s, with a sinusoidal vibrato:
  `f(t) = f0 * 2^(depth * sin(2*pi*rate*t) / 1200)`.
- The chain mirrors the plugin's Hard Tune path: YIN detector with the short
  analysis window (2048/512, median 3), chromatic quantisation, the ratio
  applied per block with no retarget smoothing and no speed floor, then the
  shipped OLA shifter (latency 10 ms, 30 ms attack, ratio-jump duck off).
- Input and output vibrato depth are measured with the same fine YIN tracker
  (2048-sample window, 128-sample hop, median 1) as the 2.5th-97.5th percentile
  half-range in cents, so input and output numbers are comparable.
- Residual = output depth / input depth. 100 % means no flattening at all.

## Results

Hard Tune chain before the 2026-10-23 change (2048/512, median 3):

| rate | depth 15 cents | depth 30 cents | depth 50 cents |
|---|---|---|---|
| 3 Hz | 82 % | 77 % | 78 % |
| 4 Hz | 101 % | 101 % | 99 % |
| 5 Hz | 125 % | 122 % | 119 % |
| 6 Hz | 143 % | 143 % | 138 % |
| 7 Hz | 157 % | 156 % | 153 % |

The residual is **independent of the vibrato depth** (a percentage), **grows
with the rate**, and crosses 100 % at ~4 Hz: from 5 Hz on, the output vibrato is
deeper than the input. This matches the classic phase-shifted residual

```
residual_cents ~= 2 * A * sin(pi * f_vib * tau)
```

with `A` the input vibrato depth and `tau` the pitch-estimate lag. Fitting the
three rates gives **tau ~= 41-42 ms** for the shipped detector (analysis window
46 ms -> half window 23 ms, plus hop 11.6 ms and median 3, plus block hold).
At 5 Hz and A = 30 cents: 2 * 30 * sin(pi * 5 * 0.041) = 36 cents, i.e. 122 % -
exactly the measured value.

Variants:

| config | 5 Hz / 30 c residual | note |
|---|---|---|
| previous 2048/512, median 3 | 122 % | baseline |
| median 1 (less lag) | 92 % | cheaper, still not flat |
| 4096 window, hop 1024 | 187 % | more lag = worse |
| shipped detector + shifter f0 smoothing OFF | 124 % | proves the shifter's 290 ms f0 one-pole is NOT the cause |
| IDEAL detector (true instantaneous pitch) | 29 % | floor set by the rest of the shifter |

The ideal-detector row is important: with a zero-lag pitch estimate the shifter
leaves only **3-12 cents** of residual (13-33 % of the input). So the dominant
contributor is the **detector lag**, not the OLA chain: the ratio applied at time
`t` was computed from the pitch at `t - tau`, the input audio is not delayed by
`tau`, and the output pitch ends up as `f(t) * target / f(t - tau)`.

## Why this matters

- The Hard Tune preset promises a flat pitch (Vibrato 0). On real singing
  vibrato (4-7 Hz, 20-60 cents) the output keeps 100-160 % of it: the note is
  snapped to the scale, but the vibrato is phase-shifted and slightly amplified.
- The "Cher/T-Pain" character is therefore weaker than advertised on vibrato
  notes, and the effect can sound like the dry take with a moved centre - which
  is consistent with the first listening pass on the A/B renderer.
- This is a detector-latency finding, not a shifter-family finding: it survives
  every shifter change (the two-tap low-latency path was closed for other
  reasons), and the ideal-detector floor shows what the rest of the chain can do.

## Option 1 measurement (shorter Hard Tune analysis)

The Hard Tune detector is already mode-scoped (2048/512, median 3), so shortening
it further only touches that path. Candidates, residual at the same 220 Hz tone:

| config | 3 Hz | 4 Hz | 5 Hz | 6 Hz | 7 Hz |
|---|---|---|---|---|---|
| shipped 2048/512/median 3 | 77 % | 101 % | 122 % | 143 % | 156 % |
| **1536/256/median 1** | **37-43 %** | **49-53 %** | **58-62 %** | **69-74 %** | **81-84 %** |
| 1536/256/median 3 | 46-52 % | 61-67 % | 73-77 % | 88-91 % | 103-104 % |
| 1024/256/median 1 | 21-36 % | 29-31 % | 37-38 % | 43-46 % | 47-52 % |
| 1024/512/median 1 | 29-55 % | 36-39 % | 45-51 % | 55-58 % | 59-67 % |

(the range is across the 15/30/50 cent depths).

The 1024 window wins but has a hard floor: **it cannot detect the 82.41 Hz bass
note at all**. Steady-tone detection accuracy (mean |cents|, max |cents|, gross
errors > 100 cents):

| config | 82 Hz | 110 Hz | 131 Hz | 175 Hz | 262 Hz |
|---|---|---|---|---|---|
| 2048/512 med3 (previous) | 0.1 / 2 / 0 | 0.1 / 2 / 0 | 0.1 / 1 / 0 | 0.2 / 1 / 0 | 0.5 / 1 / 0 |
| 1536/256 med1 | 0.1 / 5 / 0 | 0.2 / 5 / 0 | 0.2 / 5 / 0 | 0.3 / 2 / 0 | 0.5 / 3 / 0 |
| 1024/256 med1 | **no detections** | 0.2 / 5 / 0 | 0.2 / 5 / 0 | 0.3 / 4 / 0 | 0.6 / 4 / 0 |

A 1024-sample window is 23 ms at 44.1 kHz: the decimated half-window (11.6 ms) is
shorter than the 82.41 Hz period (12.14 ms), so YIN has no valid minimum. The
shortest **register-safe** candidate is therefore **1536/256/median 1**.

Cross-check in the A/B renderer (`render_hard_tune_ab --fast-detector`) on the
5 Hz / 30 cent vibrato tone: the OLA takes measure 33.2 cents of vibrato with the
shipped detector and **16.0 cents** with 1536/256/1 (dry = 27.7 cents), i.e. the
same ~58 % residual the benchmark predicts. Listening sets:

- `build/ab_hard_tune/` (shipped) vs `build/ab_hard_tune_short/` (option 1a) on
  the detuned phrase;
- `build/ab_vibrato/` vs `build/ab_vibrato_short/` on the vibrato tone.

**Verdict:** 1536/256/median 1 is safe for every register and roughly **halves**
the residual (5 Hz: 122 % to ~60 %; 7 Hz: 156 % to ~82 %). A 1024 window would
reach ~37 % at 5 Hz but needs a per-voice-type fallback (Bass/Universal keep
1536) and is a bigger, riskier change. Neither reaches the ideal-detector floor
of 13-33 %: true flatness still needs option 2 (ratio derived from the local
grain period).

**Wired (2026-10-23).** `Source/PluginProcessor.h` now ships the
1536/256/median-1 Hard Tune detector (the normal path is untouched). Measured
side effects with the new config (DetectorResponseTest): note-change lock time
**55.7 -> 32.5 ms** (the live path stays at 218 ms), YIN cost 0.89 -> 1.29 ms per
second of audio (0.13 % of one core), and the bass G2 step still locks in
26.7 ms with 0.0 cents of error. The A/B renderer keeps the previous config as
its default and exposes the new one as `--fast-detector`.

## Options (measured or estimated, not implemented)

1. **Shorten the Hard Tune analysis further** (measured, see the section above):
   1536/256/median 1 is register-safe and roughly halves the residual; a 1024
   window is better still but cannot detect the 82.41 Hz bass note, so it would
   need a per-voice-type fallback. Cheap and mode-scoped (the Hard Tune path
   already uses a shorter window than the normal path).
2. **Align the ratio with the audio**: derive the correction ratio from the
   local period at the shifter's grain read position (the OLA already searches
   pitch marks there) instead of from the detector's window half. Aligned by
   construction; estimated residual ~0, but a real DSP change with its own
   tests.
3. **Increase latency by `tau`** (delay the audio 40 ms before the ratio):
   mathematically clean, unacceptable for a live path.
4. **Accept and document**: the Hard Tune path preserves vibrato. Then the
   preset documentation must stop claiming a flat vibrato.

Recommendation: wire option 1a (1536/256/median 1) for Hard Tune now - it is
register-safe and halves the residual - and listen to the two A/B sets. A 1024
window with a per-voice-type fallback is the next step if the listening asks for
more; full flatness still requires option 2 (the ratio source is a single point
in the chain, and the ideal-detector row bounds the achievable result).

## Reproducing the numbers

```
previous Hard Tune detector (2048/512, median 3)   -> the first table
median 1 (less detector lag)
4096 window, hop 1024, median 3 (more lag)
shipped detector + f0 smoothing OFF (fix candidate)
IDEAL detector (true pitch; shifter smoothing only)
```

The `f0 smoothing OFF` row uses `PitchShifter::setF0SmoothingEnabled(false)`, an
experimental API added for this benchmark (default ON, so the shipped behaviour
is unchanged). The `IDEAL detector` row feeds the true instantaneous pitch and
the ratio against it, bypassing the detector entirely.


## The "ghost octave" on the Formant +2 take: checked, not a defect

The first listening pass flagged one note of the Formant +2 take as odd, and a
pitch tracker locked an octave below on it. Isolated reproduction: a steady
329.63 Hz harmonic tone through `render_hard_tune_ab --fixed 0` (ratio 1.0, no
detector, so only the shifter colours the tone). Spectrum, level of the half-
frequency component relative to the fundamental:

| take | 330 Hz | 165 Hz | relative |
|---|---|---|---|
| dry | 68.7 dB | -56.1 dB | -125 dB |
| OLA + Formant +2 | 69.6 dB | -0.4 dB | **-70 dB** |
| OLA, Formant 0 | 69.2 dB | 5.3 dB | **-64 dB** |
| Doppler 4.0 / 2.5 ms | 68.7 dB | -57.6 dB | -126 dB |

Conclusion: the half-frequency component is a property of the **OLA itself**
(its grain pattern is coherent over two input periods), it is **not** formant
specific (the formant take has it 6 dB *lower*), and at -64 to -70 dB it is
inaudible. The octave-down reading was a pitch-tracker lock on that weak
two-period coherence, not audible evidence. The Formant +2 "strange" note is
therefore the intended creative formant coloration (plus the residual vibrato),
not a bug to fix.
