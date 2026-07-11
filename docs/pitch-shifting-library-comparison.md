# Comparison of open source libraries for real-time pitch shifting

> **📁 ARCHIVED (2026-07-11):** Historical comparison. OpenVoxTuner now uses only the in-house PSOLA engine. RubberBand and SoundTouch were not retained.

> Working document - 2026-06-11
> Purpose: choose the pitch shifting library that will replace our in-house PSOLA
> as part of the DSP pipeline rework.

## Context

The in-house PSOLA of the Autotune Clone produces audible artefacts
(phasiness, pops, pitch-dependent glitches) that cannot be fixed
through incremental adjustments (rounds 4 to 8 observed). The pipeline is
too simplistic (OLA with Hann, no phase-locking, discontinuous COLA)
and must be replaced by a third-party, production-quality library.

This document compares the real options and makes a recommendation.

---

## Evaluated libraries

### 1. RubberBand Library

- **Official site**: https://breakfastquay.com/rubberband/index.html
- **Author**: Chris Cannam (active maintainer)
- **Current version**: 4.0.0 (2024)
- **Licence**: **GPL-2.0-or-later** (viral)
- **Language**: C++, JNI wrapper, Python, LV2
- **Algorithm**: R3 phase vocoder (Griffin & Lim canon)
- **Typical latency**: 50-100 ms (configurable via `Option::WindowSize`)

#### Audio quality

Excellent, industry standard. Used by:
- **MuseScore** (audio transposition in scores)
- **Mixxx** (DJ software)
- **Ardour** (open source DAW)
- **Sonic Visualiser**
- Various commercial LV2 and Audio Units plugins

Particularly good handling of transients (`Option::PhaseLocus = Transient`),
formant preservation, and non-stationary signals. Much better than PSOLA
on sustained voices and vowels.

#### Integration

- Classic C++ library (header + .lib/.so/.dll)
- Simple API:
  ```cpp
  RubberBand::RubberBandStretcher stretcher(sampleRate, channels, options);
  stretcher.setPitchScale(pow(2.0, semitones/12.0));
  stretcher.process(input, inputSize, false);
  stretcher.retrieve(output, outputSize);
  ```
- Compatible with Windows, macOS, Linux, iOS, Android
- Standard CMake build
- JUCE wrapper: a custom wrapper is required (50-100 lines)

#### Licence implications

**GPL = viral**: if we statically link RubberBand into our
VST3 plugin, the plugin itself must be distributed under GPL.
- Users can view and modify the source code
- They can freely redistribute it
- Obligation to ship the source alongside the binary
- No "close-source" sale possible without buying a **commercial
  licence** (Standard Licence: £420 / Non-Attribution: £1120)

#### Integration cost

- CMake build: ~30 min
- JUCE wrapper (`RubberBandPitchShifter` deriving from our `PitchShifter`): ~2-3 h
- Audio tests (buffer sizes 144/512/2048): ~1 h
- Latency to declare in `getLatencySamples`: 5 min
- **Total: ~1 working day**

---

### 2. SoundTouch

- **Official site**: https://soundtouch.surina.su/
- **Author**: Olli Parviainen (active maintainer)
- **Current version**: 2.3.3 (2023)
- **Licence**: **LGPL-2.1** (less restrictive)
- **Language**: C++ (classic library)
- **Algorithm**: SOLA/WSOLA (Synchronous OverLap-Add)
- **Typical latency**: ~100-130 ms

#### Audio quality

Fair to good on most signals. Slightly behind
RubberBand on:
- Very sustained voices (some "phasiness" on held vowels)
- Transients (weaker attack preservation)
- Large pitch shifts (> 5 semitones)

But clearly superior to our in-house PSOLA. Used by:
- **MuseScore** (also, for comparison)
- **Auralé** (Android audio player)
- **BPM Analyzer**
- **Mixxx** (optional)
- Several LV2 plugins

#### Integration

- Classic, simple C++ API:
  ```cpp
  soundtouch::SoundTouch st;
  st.setSampleRate(sampleRate);
  st.setChannels(channels);
  st.setPitchSemiTones(semitones);
  st.putSamples(input, numSamples);
  st.receiveSamples(output, numSamples);
  ```
- Header-only for the high-level wrapper, otherwise dynamic lib
- Official CMake build available
- Compatible with Windows, macOS, Linux, Android, iOS
- JUCE wrapper: ~1-2 h

#### Licence implications

**LGPL = permissive**: we can link SoundTouch **dynamically**
(.dll / .so shipped separately) without contaminating the licence of our
plugin.
- Our plugin can remain **close-source**
- We can **sell** it without restriction
- Obligation: allow the user to replace the SoundTouch .dll
  (standard on Windows)
- Alternative: commercial SoundTouch licence available (contact
  author, price not public but reasonable)

#### Integration cost

- CMake build: ~20 min
- JUCE wrapper: ~1-2 h
- Audio tests: ~1 h
- User documentation (how to replace the .dll): ~30 min
- **Total: ~0.5-1 working day**

---

### 3. Other libraries reviewed quickly

| Lib | Licence | Algo | Voice quality | Verdict |
|---|---|---|---|---|
| **Aubio** | GPL-3.0 | PSOLA + variants | Variable | Analysis-oriented, no production quality |
| **libsamplerate** | BSD-2 | Best-fit SRC | Poor | SRC, not really pitch shifting |
| **PaulStretch** | GPL | Extreme TS | Excellent for extreme | Out of scope (extreme TS) |
| **JUCE `dsp::PitchShift`** | (closed JUCE) | Basic phase vocoder | Bad on voice | No production quality |
| **TAL-Vocoder** (style) | GPL-2.0 | Various | Variable | Reference only |

**Verdict**: the only serious options for quality autotune
are **RubberBand** and **SoundTouch**.

---

## Summary comparison

| Criterion | RubberBand 4.0.0 | SoundTouch 2.3.3 |
|---|---|---|
| Licence | GPL-2.0-or-later (viral) | LGPL-2.1 (permissive) |
| Voice quality | Excellent | Good (one notch below) |
| Transients | Very well preserved | Fair |
| Latency | 50-100 ms (configurable) | 100-130 ms |
| CPU | Medium | Light |
| Build | CMake | CMake |
| Maintainer | Active (breakfastquay) | Active (Parviainen) |
| Commercial cost | £420-1120 (optional) | Unknown (contact author) |
| Integration cost | ~1 day | ~0.5-1 day |
| **Plugin must be GPL?** | **YES** (mandatory) | **NO** (dynamic LGPL) |

---

## Recommendation

### If you accept the GPL on the plugin

**RubberBand**, without hesitation. Superior audio quality, very careful
transient and formant handling, configurable latency.
This is what serious open source audio projects use
(Ardour, MuseScore, Mixxx).

### If you want to keep licence freedom

**SoundTouch**. Quality is slightly behind but far
superior to our current PSOLA. The net gain will be huge (from
"unusable on out-of-tune voice" to "usable in light production").
The licence cost is zero and you keep the ability
to sell your closed plugin.

### Hybrid alternative

Use **SoundTouch under LGPL** as default, and **leave a
compile-time slot** for a RubberBand wrapper. You start
with SoundTouch (quickly functional, no constraints),
and you can switch later by accepting the GPL.

---

## Next steps

1. **User decision** (DONE 2026-06-11): Jerome chose
   **RubberBand (GPL)**. See Round 10 of
   `docs/changelogs/changelog-2026-06-11.md` for the integration details.
2. ~~If the user chooses RubberBand~~: done, see
   `Source/dsp/RubberBandPitchShifter.h/.cpp` and Round 10 of
   the changelog.
3. ~~If the user chooses SoundTouch~~: not applicable.

## External references

- RubberBand API: https://breakfastquay.com/rubberband/code.html
- SoundTouch API: http://www.surina.net/soundtouch/README.html
- GPL-2.0 implications: https://www.gnu.org/licenses/old-licenses/gpl-2.0.html
- LGPL-2.1 implications: https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
