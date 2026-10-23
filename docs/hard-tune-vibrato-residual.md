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

Shipped Hard Tune chain (2048/512, median 3):

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
| shipped 2048/512, median 3 | 122 % | baseline |
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

## Options (measured or estimated, not implemented)

1. **Shorten the Hard Tune analysis further**: window 1536/1024, hop 256,
   median 1. Estimated `tau` ~20-26 ms -> residual at 5 Hz ~60-77 %, at 3 Hz
   ~35-50 %. Cheap and mode-scoped (the Hard Tune path already uses a shorter
   window than the normal path), but the detector needs ~2-3 periods: at 82 Hz
   (12.2 ms period) a 1024-sample window is only 1.9 periods, so bass stability
   must be measured before shipping.
2. **Align the ratio with the audio**: derive the correction ratio from the
   local period at the shifter's grain read position (the OLA already searches
   pitch marks there) instead of from the detector's window half. Aligned by
   construction; estimated residual ~0, but a real DSP change with its own
   tests.
3. **Increase latency by `tau`** (delay the audio 40 ms before the ratio):
   mathematically clean, unacceptable for a live path.
4. **Accept and document**: the Hard Tune path preserves vibrato. Then the
   preset documentation must stop claiming a flat vibrato.

Recommendation: measure option 1 with this benchmark and a listening pass; if it
does not reach a convincing flattening, take option 2 as its own small project
(the ratio source is a single point in the chain, and the ideal-detector row
bounds the achievable result).

## Reproducing the numbers

```
shipped Hard Tune detector (2048/512, median 3)   -> the first table
median 1 (less detector lag)
4096 window, hop 1024, median 3 (more lag)
shipped detector + f0 smoothing OFF (fix candidate)
IDEAL detector (true pitch; shifter smoothing only)
```

The `f0 smoothing OFF` row uses `PitchShifter::setF0SmoothingEnabled(false)`, an
experimental API added for this benchmark (default ON, so the shipped behaviour
is unchanged). The `IDEAL detector` row feeds the true instantaneous pitch and
the ratio against it, bypassing the detector entirely.
