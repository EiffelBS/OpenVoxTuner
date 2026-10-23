# Low-latency pitch shifter — Phase 3 analysis

**Status:** analysis + benchmark, no production code yet.
**Benchmark:** `tools/bench_pitchshift_latency.cpp` (target `bench_pitchshift_latency`).
**Context:** the Hard Tune mode removed the engine's own smoothing (80 ms speed floor,
ratio-jump duck), which made the pitch detector the dominant delay; the detector was
shortened in Phase 2 (note-change response 218 ms → 56 ms). The pitch shifter is now the
last large block: its declared latency is 10–30 ms and the model of a low-latency path
had never been measured.

## 1. What the current shifter actually costs

### 1.1 Architecture audit

`Source/dsp/PitchShifter` is a pitch-synchronous OLA shifter:

- a grain is `outLength = 2.5 × min(Tin, Tout)` long, windowed by a KBD window at 50 %
  overlap, and a new grain is created every output period (`outPhase` accumulates at
  `targetF0`); the read position starts at `center − outLength·F/2` and advances by `F`
  per output sample;
- the read **center** is placed at `absoluteWritePos − latencySamples`, i.e. the declared
  latency is the delay of the grain's center;
- the grain is rendered **forward** from the sample at which it is created, and its window
  peaks half a grain later: the input sample at the read center reaches the output at
  `creation + outLength/2`, so the signal's true delay is
  **`declaredLatency + outLength·F/2 − bestOffset`**, where `bestOffset` is the pitch-mark
  alignment chosen by `findBestOffset` (bounded by `maxSafeOffset = declared − outLength·F/2 − 10`).

Two consequences the benchmark confirms:

1. the declared latency is a **lower bound**, not the delay of the output;
2. the extra delay is about **1.25 pitch periods** (half a 2.5-period grain), so it grows
   as the voice goes down and with the creative formant ratio `F`.

### 1.2 Measured latency (AM-envelope cross-correlation)

Signal: 200 Hz sine with an 8 Hz amplitude modulation; the envelope carries the timing so
the measurement is not ambiguous modulo the pitch period.

| Declared | Declared samples | **Effective delay** |
|---|---|---|
| 8 ms (internal clamp) | 352 | **16.71 ms** |
| 10 ms (Direct Monitoring) | 441 | **16.87 ms** |
| 12 ms (Low Latency) | 529 | **17.57 ms** |
| 20 ms (Quality) | 882 | **21.36 ms** |
| 30 ms (Safe) | 1323 | **31.41 ms** |

And the same declared 10 ms across registers:

| Fundamental | **Effective delay** | Extra over declared |
|---|---|---|
| 82 Hz (E2) | **29.93 ms** | +19.9 ms |
| 120 Hz | **24.17 ms** | +14.2 ms |
| 200 Hz | **16.87 ms** | +6.9 ms |
| 400 Hz | **10.98 ms** | +1.0 ms |

So the **minimum effective latency of the shipped architecture is ~16.7 ms for a 200 Hz
voice, ~26.5 ms for a bass voice at the 8 ms clamp (~30 ms in the 10 ms mode)**, and the
declared value only becomes a decent approximation at high pitch. For comparison, a 10 ms
declared plugin reports 10 ms to the host: the PDC declaration understates the real delay by
6–20 ms depending on the sung register. A fixed PDC cannot track a pitch-dependent delay,
but the current gap is larger than a declaration choice should be.

### 1.3 What to declare instead (PDC replacement values)

A declaration is what the host compensates, so it must be the **largest** effective delay the
mode can produce for the notes it will see — not the nominal mode value. Because the delay
depends on the sung pitch, a single set cannot be right for every register. Measured
effective latency (ms), declared mode × fundamental, AM-envelope method, +1 semitone:

| declared | 65.4 | 82.4 | 110 | 130.8 | 174.6 | 261.6 | 400 |
|---|---|---|---|---|---|---|---|
| 8 | 31.47 | 26.49 | 24.35 | 19.61 | 16.62 | 12.70 | 10.95 |
| 10 | 36.30 | 29.93 | 24.31 | 21.16 | 18.96 | 12.93 | 10.98 |
| 12 | 34.97 | 33.29 | 25.90 | 22.72 | 18.44 | 12.81 | 11.22 |
| 20 | 41.61 | 36.92 | 26.51 | 22.56 | 25.31 | 19.27 | 19.66 |
| 30 | 46.69 | 34.44 | 37.69 | 34.92 | 33.47 | 28.14 | 28.16 |

The 8 ms row is the internal clamp, not a UI mode; it is kept for the model. The AM-sine
measurement carries a phase-dependent uncertainty of ±1–2 ms, and the pitch-mark alignment
(not a bug) makes some rows non-monotone.

**Minimal fix — stop mis-labelling a mid voice:** 10 / 12 / 20 / 30 ms become
**17 / 18 / 21 / 31 ms** (measured at 200 Hz: 16.71 / 16.87 / 17.57 / 21.36 / 31.41, rounded
up; the 8 ms clamp is 17 ms too). Exact around 200 Hz, still late by up to ~13 ms at 82 Hz.

**PDC-correct fix — declare per Voice Type at its lowest documented note** (measured value
+1 ms, held monotone in the mode):

| Voice Type (floor) | 10 ms | 12 ms | 20 ms | 30 ms |
|---|---|---|---|---|
| Bass (82.41 Hz) | 31 | 35 | 38 | 38 |
| Baritone (110 Hz) | 26 | 27 | 28 | 39 |
| Tenor (130.81 Hz) | 23 | 24 | 24 | 36 |
| Alto (174.61 Hz) | 20 | 20 | 27 | 35 |
| Soprano (261.63 Hz) | 14 | 14 | 21 | 30 |
| Universal (vocal floor 82.41) | 31 | 35 | 38 | 38 |

What this implies:

- **The mode ladder loses its meaning at the extremes.** At 262 Hz the 10, 12 and 8 ms modes
  all measure 12.7–12.9 ms; in the bass, Quality and Safe both land at ~37–38 ms. Three modes
  (minimal / standard / high quality) would carry the same information with clearer labels.
- **Universal is the hard case.** Its detector range starts at 30 Hz, where no practical
  declaration exists (65 Hz already needs 32–47 ms; 30 Hz would need well over 55 ms). Either
  declare Universal for the 82.41 Hz vocal floor and document that lower inputs are
  under-declared, or narrow Universal's range to the vocal floor.
- **Declaring the truth does not make monitoring later** — the delay is already there. It
  makes the host align the other tracks correctly and the UI number honest, but the mode
  names must stop promising "10 ms": show the computed value, or rename to quality modes.
- The real fix is still Phase 3: a 2–4 ms shifter would let the ladder advertise those numbers.

### 1.4 Other measured properties (declared 10 ms unless stated)

| Property | Value |
|---|---|
| CPU | 0.4–1.9 % of one core (4–19 ms of process() per second of audio) |
| Level at +1 semitone, 200 Hz | −2.1 dB |
| Level, 82 Hz | −5.8 dB (the grain needs future samples: `outLength/2 > latency`) |
| Amplitude warble (4–30 Hz envelope modulation) | 1.8 % at 200 Hz, 7.2 % at 120 Hz, **14.7 % at 82 Hz** |
| Pitch accuracy | −4.7 cents at 200 Hz (the pitch tracker is biased by the harmonic mix) |
| Spectral distortion vs ideal shift (LSD) | 16.4 dB at 200 Hz (steady vowel; relative measure) |

The low-register numbers are the structural limit, not a tuning issue: the grain must look
**half a grain ahead** of its center, so `declared ≥ 1.25 × min period` is mandatory. At
82 Hz that is 15.2 ms of lookahead before any margin, which is exactly why the 10 ms mode
already loses level, adds warble and takes 30 ms of real delay there.

**Conclusion:** the shipped OLA architecture cannot reach 5 ms for a mid voice, and cannot
reach it for a bass voice at any declared setting. A low-latency path needs another
algorithm. The PDC declaration should be revisited separately (Section 1.3).

## 2. Candidate algorithm families

| Family | Realistic algorithmic latency | Quality at ±1–2 st | Formants | CPU | Artifacts |
|---|---|---|---|---|---|
| **Delay-line / Doppler, 2-tap crossfade** | **2–5 ms**, independent of F0 | Flangey/phasey, "doubled" tone | Follow the shift (varispeed) | Very low | Flanging, AM at the crossfade rate, comb dips, splice clicks if unaligned |
| TD-PSOLA | ≥ 1–2 periods: 12.5–25 ms at 80–100 Hz, ~9 ms at 220 Hz | Excellent for small shifts | Preserved by construction | Low | Buzz, pitch-mark errors |
| Granular OLA / WSOLA (shipped) | 10–25 ms | Very good | Approximate via grain read offsets | Low | Time smearing, wooble at low F0 |
| STFT phase vocoder (+ phase locking) | ≈ one window; low-latency research figures ~12 ms ([source](https://www.researchgate.net/publication/261078164_Low_latency_audio_pitch_shifting_in_the_frequency_domain)), classic tutorial ~1024 samples ([source](https://cycling74.com/tutorials/the-phase-vocoder-%E2%80%93-part-i)) | Good at large windows, collapses at ≤5 ms for low voices | Needs explicit spectral-envelope correction | Moderate | Phasiness, pre-echo; phase locking reduces phasiness but not latency ([Laroche & Dolson](https://www.ee.columbia.edu/~dpwe/papers/LaroD99-pvoc.pdf)) |
| Neural / hybrid | ≥ 16–32 ms + model cost | Potentially best timbre | Yes | 10–100× budget | Not viable at 2–5 ms |

Reference points from the field (claims are labelled; none re-measured here):

- **Antares AutoTune Advanced** (2026) claims **under 1 ms** in Live mode ([product page](https://www.antarestech.com/products/pitch-correction/advanced), [FAQ](https://help.antarestech.com/hc/en-us/articles/53453475818004-AutoTune-Advanced-FAQ)); user reports say ~0.8 ms for the classic low-latency path and 2–3 ms for the new algorithm ([Gearspace](https://gearspace.com/threads/antares-introduces-autotune-r-2026.1456533/page-2)).
- **Waves Tune Real-Time** reports zero latency to the host ([user guide](https://www.waves.com/1lib/pdf/plugins/tune-real-time.pdf)) and does not publish its internal delay.
- **zplane ELASTIQUE PITCH V2** is 150 ms at 48 kHz ([manual](https://d1aeri3ty3izns.cloudfront.net/media/94/940160/download_940160.pdf)); **Rubber Band**'s real-time modes are tens of ms ([JUCE forum](https://forum.juce.com/t/how-to-handle-latency-in-pitch-shifter-rubberband/45799)); **SoundTouch** ~100 ms ([README](https://www.surina.net/soundtouch/README.html)).
- **JUCE `dsp::PitchShifter`** is STFT-based, so its latency is a function of the FFT size ([docs](https://docs.juce.com/master/classdsp_1_1PitchShifter.html)).

The structural fact: **only the delay-line family has a latency independent of F0**. Every
pitch-synchronous method needs at least one period of lookahead, and a phase vocoder needs
a window worth of samples. For a bass voice (10 ms period) a 5 ms budget is impossible for
PSOLA and marginal for a vocoder; the delay-line shifter reaches 2–5 ms for every register.

## 3. Benchmark: a 2.5–4 ms existence proof

A deliberately simple two-tap crossfaded delay-line shifter was added to the benchmark
(not production code): the read delay sweeps linearly, wraps at the span end, and a
triangular crossfade hides the wrap. Figures measured by the benchmark at 200 Hz, +1
semitone:

| Config | Effective latency | CPU (µs/s) | RMS | Click* | Pitch | LSD | Warble |
|---|---|---|---|---|---|---|---|
| PSOLA declared 10 ms | 16.87 ms | ~6 500–8 500 | −2.1 dB | 0.39 | −4.7 c | 16.4 dB | 1.8 % |
| **Doppler avg 2.5 ms** | **2.47 ms** | **~400** | −1.0 dB | 0.02 | +0.7 c | 15.0 dB | 2.7 % |
| **Doppler avg 4.0 ms** | **3.97 ms** | **~390** | −1.6 dB | 0.20 | +0.3 c | 12.2 dB | 5.4 % |

\* click = max sample jump on a 200 Hz sine with ±1 semitone ratio steps every 75 ms. It is
a stress figure, not the unit-test gate: the shipped shifter's attack duck does not settle
at that rate, and the number varies with the latency/alignment combination (0.058 at
declared 12 ms, 0.29–0.39 elsewhere). Read it comparatively.

The prototype proves that **2.5–4 ms is reachable at ~0.04 % of a core** and that the
objective metrics do not collapse. They also do not capture the cost the ear hears: the
delay-line shifter's flanging/phasiness lives in phase/comb behaviour, not in the averaged
magnitude spectrum or the envelope AM. **A listening test is mandatory** before choosing it.
The published character of the family is "classic hardware-style, lowest latency and CPU,
may produce artifacts" ([BrainModular manual](https://www.brainmodular.com/manuals/hh7/fr/modules/audio/fx/audio-pitch-shifter)),
with crossfade rates from 2 ms to 200 ms in the Eventide lineage ([H9 guide](https://downloads.eventide.com/audio/Product+Manuals/H9AlgorithmGuide+V12.pdf)).

## 4. Recommendation

**Build a separate `LowLatencyPitchShifter` class for Hard Tune; do not bend the OLA
shifter.** The existing class is structurally a 10–30 ms algorithm (grain size, pitch-mark
search, ring alignment) and every parameter that makes it good at 10–30 ms works against
2–4 ms.

Proposed shape:

- **Algorithm**: 2 taps (4-tap fallback), fractional-delay ring, cubic/Lagrange
  interpolation, delay span 2–3 ms, crossfade 1–2 ms, correlation-aligned splice search
  (±0.5 ms) to avoid comb dips, fixed declared latency **4 ms** decided in `prepare()`.
- **Routing**: Hard Tune selects it; Normal/Creative keeps the OLA. Behind a small
  interface (`prepare`, `process`, `getLatencySamples`, `reset`); both latencies fixed per
  mode at `prepareToPlay` (VST3 does not like a reported latency that changes mid-stream).
- **Formant control**: formant preservation is impossible at 2–5 ms without an FFT window.
  In Hard Tune either bypass the Formant knob (documented) or map it to a gentle spectral
  tilt; do not fake independent formants.
- **Large ratios**: above ~3 semitones the crossfade rate makes the warble obvious; route
  them to the quality path or add the 2 extra taps.
- **The PDC finding**: fix the delay declaration/compensation separately (Section 1.2).
  A conservative declared latency plus an alignment offset is a decision, not a DSP change.

### Acceptance criteria

- Measured effective latency ≤ 5 ms (target 3–4 ms) at 44100 and 48000 Hz.
- Ratio 1.0 null test: output = input delayed by exactly the declared latency (no comb).
- Pitch accuracy ≤ 5 cents on steady tones, ±2 semitones.
- Click detector at or below the unit-test threshold on note steps and ratio steps.
- Warble and LSD no worse than the prototype in Section 3; listening tests on sung
  phrases (male bass, female alto), including vibrato and plosives.
- CPU ≤ 5 ms per audio second (≈0.5 % of one core).
- Mode switch click-free; latency changes only in `prepareToPlay`.

### Effort and risks

~200–400 lines + 1–2 days of tuning, 1–2 days of listening/click tests. Low algorithmic
risk (the family is 40 years old), medium product risk: the flanging character is either
"exactly the hard-tune attitude" or "cheap", and only ears decide. Plan B is a one-period
PSOLA gated to F0 ≥ ~180 Hz (~8–10 ms), which does not solve bass.

## 5. Listening A/B — the decision step

The objective metrics cannot judge flanging, so the decision is made by ear on one take.
`tools/render_hard_tune_ab.cpp` (target `render_hard_tune_ab`) renders the SAME Hard Tune
correction trajectory (short YIN window 2048/512, median 3, chromatic snap, no retarget
smoothing, no speed floor, no ratio-jump duck; the OLA keeps its note-onset mask) through:

- OLA 10 ms with the shipped preset colour (creative Formant +2 st);
- OLA 10 ms with Formant 0 (separates the formant colour from the shifter);
- Doppler probe 2.5 ms and 4.0 ms (formants follow pitch; no creative formant).

It is deliberately not the full plugin chain: no detector-side anti-octave filter, no
de-esser, saturation or limiter, no formant preserver. The variants are level-matched to the
dry take and written to `build/ab_hard_tune/` (or the directory you pass) as `dry.wav`,
`take_01..04.wav` (anonymised, shuffled) and `key.txt`. Listen before reading the key.

```powershell
cmake --build build --config Release --target render_hard_tune_ab
build\render_hard_tune_ab_artefacts\Release\render_hard_tune_ab.exe               # synthetic phrase
build\render_hard_tune_ab_artefacts\Release\render_hard_tune_ab.exe mytake.wav    # a real take
```

What to listen for: sustained notes (flanging/combing, warble at the wrap rate), note attacks
and splices (clicks), low register, vibrato. Decision rule: if the Doppler is at least as
convincing as the OLA on a mid/high voice, prototype the separate class (Section 4); if it
sounds cheap on held notes, stop and spend the effort on the PDC finding instead.

## 6. Reproducing the benchmark

```powershell
cmake --build build --config Release --target bench_pitchshift_latency
build\bench_pitchshift_latency_artefacts\Release\bench_pitchshift_latency.exe
```

The tool prints the three tables above (declared-latency sweep, fundamental sweep, Doppler
prototype) and its sanity gates. It is an analysis tool, not a CI gate; the unit tests
remain the shipped-quality gates.
