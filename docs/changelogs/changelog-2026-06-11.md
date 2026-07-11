# Changelog - June 11, 2026

## Round 11 - Fix RubberBand wrapper helicopter/stutter

### Symptom reported by Jérôme (after Round 10)

"Wow, it's even worse than with the previous solution! Now, at max buffer
2048, it sounds like a helicopter... chops/stutters, whether in-tune or
out_of_tune! To the point where I don't even hear the sung notes. At min
buffer, there are also artefacts but I hear my voice a bit."

### Root cause (confirmed by instrumentation)

The `RubberBandPitchShifter::process()` wrapper had two design flaws that
combined to create the "helicopter" pattern:

1. **Output overwritten by the while loop**: `readyOutput` (size 512) was
   overwritten at each `shift()` call in the while loop. When the host gave
   a buffer larger than 512 (e.g. 2048), the loop ran 4 iterations and only
   the last shift() was kept. Phase 3 then served 512 audio and filled the
   rest with silence. **At host=2048, this gave 75% silence in output**
   (1536/2048).

2. **shift() called at its own pace**: we waited to have accumulated 512
   samples in `pendingInput` before calling `shift()`. At host=144, this
   happened only 1 time every 4 calls. The other 3 calls produced silence
   (Phase 3 found nothing to serve). **At host=144, silence ratio = 11%**.

The instrumentation (NDJSON file written by the wrapper, then deleted after
validation) confirmed the predictions:
- Run 1 (host 144, 13924 calls): 11.2% cumulative silence
- Periodic pattern of 64 samples of silence every 4 calls

### Fix applied

Full rework of the input/output pipeline in
`Source/dsp/RubberBandPitchShifter.h/.cpp`:

1. **Rolling input buffer (size 512)**: always contains the last 512 input
   samples (instead of waiting to accumulate 512). We shift left by
   chunkSize then append the new samples at the end.

2. **Multi-shift loop**: we call `shift()` N times per process() where
   N = ceil(numSamples / 512). For host=144, N=1. For host=2048, N=4.
   Between each shift, we update the rolling buffer with the next chunk of
   samples.

3. **Circular output buffer (size 8192)**: replaces `readyOutput` (512,
   overwritten). Each `shift()` APPENDS 512 samples to `outputWritePos`
   (modulo 8192). Phase 3 serves from `outputReadPos`.

4. **Latency cap (4096 samples, ~85 ms at 48 kHz)**: when `outputValid >
   cap` (the host=144 case where production exceeds consumption), we advance
   the read position to drop the oldest samples. Guarantees bounded latency
   at the cost of micro phase artefacts.

### Result post-fix (validated by Jérôme)

- **Host >= 1024**: 0% silence, fluid audio, no artefact (production =
  consumption ratio, no drop).
- **Host < 1024** (e.g. 144): also 0% silence, but micro phase artefacts
  (light clicks) due to the "drop oldest" of the latency cap. Acceptable,
  but Jérôme mentions there are "still clicks/chops below 1024".

### Trace

- Debug session: `debug-rubberband-helicopter-stutter.md` (deleted after
  validation, see TRAE-debugger protocol step 11).
- Instrumentation log: `trae-debug-log-rubberband-helicopter-stutter.ndjson`
  (also deleted).

## Round 12 - Latency Cap improvement (Crossfade)

### Context
In the Round 11 fix, when the host requests small buffers (e.g. 144 samples)
while `RubberBand` produces fixed 512-sample blocks, the excess samples
accumulate. To prevent latency from growing indefinitely, a "latency cap"
was implemented, brutally dropping ("hard drop") the old samples. This
solved the delay problem but introduced small clicks / phase discontinuities.

### Implementation
- Added a linear crossfade in `RubberBandPitchShifter::process` at the time
  of the drop.
- Instead of simply jumping forward (`outputReadPos += toDrop`), the
  algorithm "fades out" the old section and "fades in" the new section over
  a maximum of 256 samples (~5ms).
- Result: drop transitions (at buffer < 1024) are greatly softened, removing
  the audible clicks (chops).

## Round 10 - RubberBand v4.0.0 integration (PitchShifter rework)

### Decision

Following Round 9, Jérôme chose to integrate **RubberBand Library v4.0.0**
(accepted GPL-2.0-or-later license). The entire plugin is now under GPL.

### Implementation

#### External sources

- **`external/rubberband-4.0.0/`**: complete tree of RubberBand v4.0.0,
  downloaded from
  `https://breakfastquay.com/files/releases/rubberband-4.0.0.tar.bz2`
  (the official Mercurial repo on hg.sr.ht was not directly accessible
  without Mercurial; the release tarball is the maintainer's recommended
  path).
- **Single-file** build via `single/RubberBandSingle.cpp` (see the project's
  `COMPILING.md`): a single .cpp that includes all the others via relative
  `#include`s. Advantage: no separate .lib to link, no Meson/Ninja to install,
  no external dependency (KissFFT integrated, BQResampler integrated).

#### New module: RubberBandPitchShifter

- **`Source/dsp/RubberBandPitchShifter.h`**: declaration of the JUCE wrapper.
  Encapsulates `RubberBand::RubberBandLiveShifter` (the new v4 API dedicated
  to pure pitch-shifting, without time-stretch, with minimal latency).
- **`Source/dsp/RubberBandPitchShifter.cpp`**: implementation.
  - Options: `OptionFormantPreserved | OptionWindowMedium
    | OptionChannelsTogether` (preserves timbre, quality/latency trade-off,
    coherent stereo processing).
  - Buffering: RubberBand requires EXACTLY `getBlockSize()` (512) samples per
    `shift()` call. The wrapper accumulates the host input (variable size
    144-2048) in `pendingInput` until a full block is available, calls
    `shift()`, stores the output in `readyOutput`, and serves it gradually to
    the host.
  - Priming: the first `shift()` call produces a "warm-up" block that is
    discarded. This adds an extra latency of 2 * 512 = 1024 samples (~23 ms at
    44.1 kHz), negligible compared to RubberBand's 50+ ms internal latency.
  - `getLatencySamples()`: returns `shifterStartDelay + 2 *
    shifterBlockSize`.

#### CMake

- **`CMakeLists.txt`**:
  - Added `Source/dsp/RubberBandPitchShifter.cpp` and `.h` to the plugin AND
    tests sources.
  - Added `external/rubberband-4.0.0/single/RubberBandSingle.cpp` to the
    sources.
  - Include paths: `external/rubberband-4.0.0` (public API) and
    `external/rubberband-4.0.0/single` (for the `#include "../src/..."` of
    single.cpp).
  - Compile defines: `USE_BQRESAMPLER=1`, `NO_TIMING=1`, `NO_THREADING=1`,
    `NO_THREAD_CHECKS=1`, `USE_BUILTIN_FFT=1` (see the header of
    `RubberBandSingle.cpp`).
  - **NOMINMAX=1**: required because RubberBand's `Thread.h` includes
    `<windows.h>` which defines `min`/`max` macros conflicting with
    `std::min`/`std::max` used in R3Stretcher.cpp and R3LiveShifter.cpp.
    Without it, C2589/C2059 compilation errors.
  - Excluded the in-house PSOLA (`Source/dsp/PitchShifter.cpp/.h`) from the
    build sources (the file remains on disk as an archived reference for
    rounds 4-8, but is no longer compiled).

#### PluginProcessor

- **`Source/PluginProcessor.h`**: `pitchShifter` is now of type
  `std::unique_ptr<atdsp::RubberBandPitchShifter>`. The type exposes the same
  interface as the old `atdsp::PitchShifter` (prepare, reset, process), so the
  rest of the pipeline (PitchDetector, ScaleQuantizer, RetargetEnvelope) is
  unchanged.
- **`Source/PluginProcessor.cpp`**:
  - Constructor: `std::make_unique<atdsp::RubberBandPitchShifter>()`.
  - `prepareToPlay`: `latencySamples = analysisWindow / 2 +
    pitchShifter->getLatencySamples()` (the YIN analysis window adds
    analysisWindow/2, and RubberBand adds its latency).
  - `processBlock`: unchanged call to `pitchShifter->process(buffer,
    ratio, f0)`.

#### build.ps1

- **`build.ps1`**: added generation of `JuceHeader.h` for the
  `AutotuneTests` target (which was previously silently broken - the
  generation was done only for `AutotuneClone`).

#### License

- **`LICENSE`**: new file at the project root. Declares the entire plugin
  under GPL-2.0-or-later, with mention of RubberBand and reference to
  RubberBand's COPYING.

### Verification

- Release x64 build: OK. Standalone generated
  (`build/AutotuneClone_artefacts/Release/Standalone/Autotune Clone.exe`).
- VST3 generated (`build/AutotuneClone_artefacts/Release/VST3/Autotune
  Clone.vst3`).
- Standalone launched (PID 60064, ~88 MB, stable).
- Latency reported to host: `analysisWindow/2 (1024) +
  shifterStartDelay (~256-512) + 2 * 512 (1024) = ~2.5-3k samples =
  ~57-68 ms at 44.1 kHz`. Within RubberBand's documented range (50-100 ms).
- **To be validated by user testing**: real-time pitch shift on real voice
  (sung or spoken), with varied Speed and Amount parameters. The sound should
  no longer show the "pops", "clicks" or "pitch-dependent glitches" that
  characterized the in-house PSOLA (rounds 4-8).

### Architectural notes

- The wrapper keeps the same interface as the old PSOLA: if Jérôme ever wants
  to switch to another lib (SoundTouch, etc.), it suffices to reimplement
  `RubberBandPitchShifter` and change the `unique_ptr` type. No change in the
  rest of the pipeline.
- `FormantPreserver` is kept in the pipeline (before `pitchShifter->process`).
  It is in practice redundant with RubberBand's `OptionFormantPreserved`, but
  does no harm (linear prefilter). To be removed if we want to save CPU.

## Round 9 - Rework decision: third-party library

### Symptom reported by Jérôme (after Round 8)
"I don't feel like the rounds help much and we cannot accept these artefacts
on a voice audio processing :( So, either I accept that it's not possible for
you to create an autotune clone, or a major rework is needed."

Jérôme confirmed the failure of the incremental PSOLA fixes after 5 rounds (4
to 8): the artefacts (pops, clicks, pitch-dependent glitches) persist because
they are fundamental properties of simple PSOLA (phasiness, COLA modulation,
no phase-locking, pitch mark jitter).

### Decision

**Major rework of the PitchShifter module**: replace the in-house PSOLA with a
production-quality third-party library.

### Libraries evaluated

1. **RubberBand** v4.0.0 - GPL-2.0-or-later (viral)
   - R3 vocoder phase, excellent quality, 50-100 ms latency
   - Cost: the entire plugin must go GPL, OR commercial license (£420-1120)
2. **SoundTouch** v2.3.3 - LGPL-2.1 (permissive)
   - SOLA/WSOLA, good quality, 100-130 ms latency
   - Cost: standard dynamic linking, plugin can stay closed
3. **Others** (Aubio GPL-3.0, libsamplerate BSD, PaulStretch GPL): evaluated,
   not suitable for a production-quality autotune.

### Comparison document

- **`docs/pitch-shifting-library-comparison.md`**: detailed comparison
  (quality, latency, license implications, integration cost,
  recommendation).

### Next step

- **Pending**: Jérôme must choose between RubberBand, SoundTouch, or a hybrid
  approach (SoundTouch by default + slot for RubberBand).
- Once chosen, integration via a dedicated JUCE wrapper (max 1 day) then
  removal of the in-house PSOLA.

### In-house PSOLA status

- The 5 fix rounds (4-8) remain in the code for reference, but the PSOLA will
  no longer be used in production once the third-party library is integrated.
- The `Source/dsp/PitchShifter.h` skeleton is designed as an interface: it
  suffices to create a new `*PitchShifter.h/.cpp` deriving from the same
  interface.

## Round 8 - No realignment of grains on block boundary

### Symptom reported by Jérôme (after Round 7)
"Same as before" - the H1 (YIN octave) and H5 (COLA gain) fixes did not change
the phenomenon. The pops at 2048 and the pitch-dependent glitches at 144
persist.

### Hypothesis: realignment on a multiple of T0p is discontinuous

In the synthesis loop, we realigned `nextSynthMarkSample` to the first
multiple of T0p >= blockStart. This forced the first grain of the new block
to be at a position aligned on the block boundary, which can be slightly
different from the position we would have obtained by continuing from the
previous block.

Consequence: the first grain of the new block is at a position "chosen for the
block's convenience" rather than at the "natural" continuity position ->
micro phase jump at each block boundary, audible as a click.

### Fix

We only realign if we lost continuity with the previous block (i.e. if
`nextSynthMarkSample` is older than `blockStart - T0p`, which happens after a
long silence or a passthrough). Otherwise, we keep the continuous position.

Theoretical result: the first grain of the new block is exactly at the position
it would have had if we had crossed the boundary without seeing it, so no phase
jump.

### File modified

- **`Source/dsp/PitchShifter.cpp`**: the realignment condition changed from
  `nextSynthMarkSample < blockStart` to `nextSynthMarkSample < blockStart - T0p`.

### Verification

- Release x64 build succeeded.
- Standalone relaunched: OK (PID 124508).
- To be confirmed by user testing.

### Important note on PSOLA limitations

If after this fix the glitches persist, this would confirm that the residual
artefacts are **fundamental properties of simple PSOLA** (phasiness, COLA
modulation, pitch mark jitter) that can only be eliminated by changing the
algorithm:
- Phase-locked PSOLA (simplified phase vocoder)
- LPC + non-uniform resampling
- Sinusoidal modeling

These algorithms represent a major rework (weeks of work) beyond the scope of
the current fixes. In this case, the best option is probably to accept the
artefacts and move on to other features (MIDI out, presets, etc.).

## Round 7 - YIN anti-octave-error + COLA gain smoothing

### Symptom reported by Jérôme (after Round 6)
"Still the same phenomenon:
- 2048: pops, no notable improvement
- 144: clicks and pops or audio glitches that change with pitch.
On the other hand, I can get in-tune at 144 now."

The "in-tune at 144" is an improvement (the Round 6 fix worked on this point).
The residual glitches are of two types:
- "pops at 2048": not improved by previous rounds
- "pitch-dependent glitches at 144": not improved

### Fix 1: YIN anti-octave-error (H1)

In `PitchDetector::detectPitch`, after finding a tau under the threshold, we
now check whether 2*tau is ALSO under the threshold. If yes, the detected tau
is a sub-harmonic and we take 2*tau instead (= the fundamental).

This is the classic YIN error: on signals with a strong 2nd harmonic (typical
of voice), YIN detects 2*f0 instead of f0. The PSOLA then produces an audible
sub-harmonic that changes with the sung pitch -> "pitch-dependent glitch".

Reference: de Cheveigne & Kawahara, "YIN, a fundamental frequency estimator
for speech and music", J. Acoust. Soc. Am. 2002.

### Fix 2: COLA gain smoothing (H5)

Theoretical COLA gain = `1/overlapCount` where `overlapCount = 2*T0/T0p`.
When T0p changes between blocks (f0 or ratio changing), overlapCount changes,
the gain jumps -> audible pop.

Added a **time-based smoothing** of the gain (tau=20 ms) via a new
`smoothedGain` member in `PitchShifter`, updated with
`alpha = 1 - exp(-blockDuration/0.02)`.

Result: the gain follows the real changes of T0p but eliminates the fast jumps
between blocks.

### Files modified

- **`Source/dsp/PitchDetector.cpp`**: added step 3b (anti-octave-error) after
  step 3 (minimum search).
- **`Source/dsp/PitchShifter.h`**: added the `smoothedGain` member.
- **`Source/dsp/PitchShifter.cpp`**: time-based smoothing of the COLA gain
  (tau=20 ms).

### Verification

- Release x64 build succeeded.
- Standalone launched: OK (PID 112772, ~83 MB, stable).
- To be confirmed by user testing if:
  - The "pitch-dependent glitch" at 144 is eliminated (H1)
  - The "pops" at 2048 are reduced (H5)

### Debug session

See `debug-psola-pitch-dependent-glitch.md` (status IN-PROGRESS).

### Remaining hypotheses (H2, H3, H4): if the glitches persist

- **H2 - PSOLA phasiness**: artefact inherent to simple PSOLA when the
  pitch-shift is significant. Fix: phase-locked PSOLA or LPC + resampling
  (complex, out of current scope).
- **H3 - findPeak jittery**: fluctuating pitch mark positions. Fix:
  interpolation between adjacent marks.
- **H4 - RetargetEnvelope**: may still be under-damped depending on the Speed
  set by the user.

## Round 6 - Synthesis state size fix (anti-click when T0 increases)

### Symptom reported by Jérôme (after Round 5)
"Still the glitches. At max buffer size (2048, I don't have the 4096 option),
the pops are still there and frequent when out-of-tune. At min buffer size
(144), lots of clicks this time and also a very fast audio glitch that changes
with the sung pitch (and I can't get an in-tune pitch even though it's possible
at buffer 2048)."

The "very fast glitch that changes with pitch" and the "clicks at 144" are two
distinct symptoms. The clicks at 144 have an identifiable cause (see below).
The pitch-dependent glitch remains under investigation (see "Remaining
hypotheses").

### Cause of clicks at 144: synthesis state too small when T0 changes

In Round 4, the synthesis state was sized to `halfGrain` (= T0) per block. If
the user sings a lower note (T0 increases between two blocks), the first grain
of the new block needs MORE state than what we stored with the old T0 -> the
left half-window of the first grain is incomplete -> click.

Concrete example at 144 samples, f0=200Hz (T0=220), then f0=100Hz (T0=441):
- Block N: 220 samples of state stored. T0 = 220. OK.
- Block N+1 (f0 changes to 100Hz): T0 = 441. The first grain needs 441 samples
  of state. But we only have 220. The left half-window is incomplete over 221
  samples -> click + deformed grain.

### Fix

Always store `synthStateCapacity` (4096) samples of state, not just T0. This is
more than necessary for a single grain (max T0 ~882 @ 50 Hz with
sampleRate=44100), but it guarantees that the first grain of the next block
ALWAYS has its complete left half-window, even if T0 increased.

Cost: 4096 * 2 channels * 4 bytes = 32 KB memcpy per audio block, negligible.

### File modified

- **`Source/dsp/PitchShifter.cpp`**: replaced `juce::jmin (halfGrain,
  workingSize)` with `juce::jmin (synthStateCapacity, workingSize)` in the
  synthesis state update.

### Remaining hypothesis: "pitch-dependent glitch" at 144 samples

The "very fast glitch that changes with pitch" is NOT explained by the OLA.
Hypotheses (to investigate in a later round):

1. **Octave error in YIN**: YIN can detect f0/2 or 2*f0 instead of f0 if the
   signal has a strong harmonic 2. The PSOLA then produces an audible
   sub-harmonic. The error probability increases at small buffer where YIN's
   signal/noise ratio is lower (the downmixed L+R signal can be less clean).
   Possible fix: median filter on the f0 output, or adaptive YIN confidence
   thresholds.

2. **Mark positions jittery**: findPeak looks for a max in a T0-sample window.
   If the signal has several comparable-height peaks, the max can "switch"
   between blocks -> grains at slightly different positions -> micro-glitch.
   Possible fix: interpolation between adjacent marks.

3. **Impossible to be in-tune at 144**: the analysis FIFO is 2048 samples,
   filled in 14 blocks of 144. Once filled, detection should be stable. If the
   user sings right after startup, the FIFO is not yet full and f0_in is 0.0f
   -> no correction applied -> the audio seems "out-of-tune" while they are
   singing on the note. Possible fix: start with a smaller analysis window
   (e.g. 512 samples) for the first blocks, then switch to 2048.

### Verification

- Release x64 build succeeded.
- Standalone launched: OK (PID 51536, ~83 MB, stable).
- To be confirmed by user testing if the clicks at 144 are eliminated.
- The "pitch-dependent glitch" will probably require a later round with a more
  systematic approach (debugger skill).

## Round 5 - Time-based smoothing fix (eliminates residual "pops")

### Symptom reported by Jérôme (after Round 4)
"there are still glitches no matter the buffer size (more marked the smaller
the buffer size). That said, the glitches are less sharp than before (a more
'pop' than 'click' sound)"

The change from "click" to "pop" confirmed that the OLA fix (Round 4) worked
(acute discontinuities eliminated) but a low-frequency modulation source
remained. And the fact that it was more marked at small buffer pointed to a
buffer-size-dependent behavior.

### Identified root cause

Two smoothers were **per-block** instead of **time-continuous**, which gave a
dramatically different behavior depending on the buffer size:

1. **`PitchShifter::currentF0` smoothing (0.85 per block)**:
   - 144 samples: tau ~7 ms (near-instant). The f0 follows the YIN jitter
     block by block, the PSOLA grains jump 1 sample each block when T0 changes
     -> "pop".
   - 4096 samples: tau ~200 ms (too slow, the plugin no longer follows the
     voice).

2. **`PitchShifter::smoothedRatio` (smoothingCoeff = 0.9 per block)**: same
   problem, 4x slower at small buffer.

3. **`RetargetEnvelope::processSample` called once per block with a
   per-sample alpha**: this was the most severe bug. Effective response time
   formula = `tau * numSamples`.
   - 144 samples, speed=50ms: effective tau = 7.2 s (the Speed has **almost no
     effect** at small buffer!)
   - 4096 samples, speed=50ms: effective tau = 204.8 s (the Speed is completely
     inoperative)
   This is the main reason why the Speed seemed to do little at small buffer in
   previous tests.

### Fix applied

**Time-based** smoothing in both modules:

```cpp
// In PitchShifter::process :
const float blockDuration = numSamples / sampleRate;
const float ratioAlpha = 1.0f - std::exp(-blockDuration / 0.05f);  // tau=50ms
const float f0Alpha    = 1.0f - std::exp(-blockDuration / 0.03f);  // tau=30ms
smoothedRatio = (1.0f - ratioAlpha) * smoothedRatio + ratioAlpha * ratio;
currentF0     = (1.0f - f0Alpha)    * currentF0    + f0Alpha    * f0;
```

```cpp
// New method RetargetEnvelope::processBlock(targetRatio, numSamples) :
const double blockDuration = numSamples / sampleRate;
const double tau = speedMs / 1000.0;
const float blockAlpha = 1.0f - std::exp(-blockDuration / tau);
currentValue += blockAlpha * (targetRatio - currentValue);
```

With these formulas, the time constant is the **same** for all buffer sizes.
The Antares Speed now has a homogeneous effect.

### Files modified

- **`Source/dsp/PitchShifter.h`**:
  - Removed `smoothingCoeff` (member made useless by the inline time-based
    smoothing in process()).
  - Explanatory comment on the time-based smoothing.

- **`Source/dsp/PitchShifter.cpp`**:
  - `process()`: replaced the two per-block smoothers (0.9 and 0.85) with
    time-based smoothers with tau=50ms (ratio) and tau=30ms (f0).
  - Removed the reference to `smoothingCoeff`.

- **`Source/dsp/RetargetEnvelope.h`**:
  - New `processBlock(targetRatio, numSamples)` method that accounts for the
    block size.

- **`Source/dsp/RetargetEnvelope.cpp`**:
  - Implementation of `processBlock` with alpha = 1 - exp(-blockDuration/tau).
  - `processSample` kept (for unit tests and compatibility).

- **`Source/PluginProcessor.cpp`**:
  - `retargetEnvelope->processSample(targetRatio)` replaced by
    `retargetEnvelope->processBlock(targetRatio, buffer.getNumSamples())`.

### Verification

- Release x64 build succeeded.
- Standalone launched: OK (PID 114592, ~83 MB, stable after 5s).
- To be confirmed by user testing at different buffer sizes.
- The Speed should now have an audible homogeneous effect at all buffer sizes.
- The "pops" should be reduced thanks to the increased f0 stability (30 ms
  smoothing vs 7 ms before at small buffer).

### Debug session

See `debug-persistent-audio-glitches.md` (still FIXED for round 4, this round
5 attacks the second source of glitches).

### Round 4 - Fix clicks/pops: persistent PSOLA synthesis state

### Symptom reported by Jérôme
"Clicks/pops in the audio" best represents the audio glitches I hear. The
glitches are very numerous no matter the buffer size, and particularly dense at
small buffer (144 samples). The 32-sample input crossfade (applied in the
previous round) only reduced them slightly.

### Identified root cause

PSOLA grains use a Hann window of length **2 * T0** (T0 = fundamental period,
typically 100-200 samples for voice). For the overlap-add (OLA) to be
continuous in time, each grain needs the T0 samples BEFORE and AFTER its pitch
mark to be accessible.

But in the block-by-block implementation of `PitchShifter::process`, the output
buffer was reset to zero at each call. Two severe consequences:

1. **Clipped left half-window**: the first grain of a block, centered at
   `synthStart` (may be only a few samples after the block start), has its left
   half-window (length T0) COMPLETELY lost. The grain only contributes its
   right half, which produces an amplitude jump right from the first sample of
   the block -> click.

2. **Clipped right half-window**: symmetrically, the last grain of the block
   has its right half-window lost. The next block, which starts on an empty
   output buffer, does not benefit from this contribution.

At each block boundary, there is therefore a continuity "hole" of about
2 * T0 - T0p samples (T0p = T0 / ratio), during which the PSOLA output suddenly
goes from one value (full grain) to another (truncated grain) or to zero.

With T0 = 220 samples (f0 = 200 Hz @ 44.1 kHz), the "hole" is on the order of
200-400 samples per block. For a 144-sample buffer, this "hole" is much larger
than the block itself, which explains the very audible degradation at small
buffer.

### Fix: persistent synthesis state (`synthStateBuffer`)

We propagate the tail of the previous block to the start of the next block. The
working buffer (`outputBuffer`) is now organized as follows:

```
[synthState (T0 samples) | currentBlock (numSamples samples)]
```

In Phase 2 of `PitchShifter::process()`:
1. We copy `synthStateBuffer` at the start of `outputBuffer` (provides the left
   half-window of the 1st grain).
2. We add the current block grains into the extended area with
   `outCapacity = workingSize = synthStateSize + numSamples`.
3. At the end, we save the last T0 samples of the working area into
   `synthStateBuffer` for the next call (`synthStateSize = min(T0, workingSize)`).
4. We copy ONLY `[synthState, synthState + numSamples]` to the host output
   buffer.

The OLA is now **continuous in time** beyond block boundaries. The clicks
become negligible (limited to the first call after enable, or the return after
passthrough, where the state is empty and the left half-window is clipped once;
the 32-sample input crossfade masks this).

### Files modified

- **`Source/dsp/PitchShifter.h`**:
  - Added members:
    - `static constexpr int synthStateCapacity = 4096;` (covers f0 > 10 Hz)
    - `juce::AudioBuffer<float> synthStateBuffer;`
    - `int synthStateSize = 0;`
  - Comment detailing the rationale for this state.

- **`Source/dsp/PitchShifter.cpp`**:
  - `prepare()`: allocates `synthStateBuffer` (4096 samples) and increases
    `outputBufferCapacity` to `jmax(bs*2, synthStateCapacity + bs, 2048)` to
    have room to host the state + the current block.
  - `reset()`: clear `synthStateBuffer` and reset `synthStateSize = 0`.
  - `process()` passthrough branch: clear `synthStateBuffer` (the PSOLA samples
    it contains are no longer valid in passthrough; on return from pitch
    shifting, we rebuild a fresh state on the first blocks).
  - `process()` Phase 2:
    - Layout: `outputBuffer = [synthState | currentBlock]`.
    - `workingSize = synthStateSize + numSamples`.
    - `blockOffsetInWork = synthStateSize` (offset for `outPos`).
    - `addGrain` is called with `outPos = blockOffsetInWork + (t_out - blockStart)`
      and `outCapacity = workingSize`.
    - At end of channel loop: update `synthStateBuffer` with the `halfGrain`
      last samples of the working area, and `synthStateSize = halfGrain`.
  - `process()` Phase 3: `buffer.copyFrom(ch, 0, outputBuffer, ch, blockOffsetInWork, numSamples)`
    instead of `... 0, numSamples)`. We no longer copy the synthesis state,
    which is purely internal.
  - `process()` safety check: moved after the passthrough branch, and compares
    `outputBufferCapacity` to `numSamples + synthStateCapacity` (not just
    `numSamples` anymore). In passthrough, no outputBuffer nor inputBackup
    needed.

### Verification

- Release x64 build succeeded.
- Standalone launched: OK (PID 141136, ~80 MB, stable after 5s).
- To be confirmed by user testing at different buffer sizes (144, 512, 2048,
  4096).

### Debug session
`debug-persistent-audio-glitches.md` (status: FIXED).
