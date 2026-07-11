# Changelog - June 9, 2026

## Autotune Clone project initialization

### Architectural decisions
- Framework: **JUCE 8.0.12** (git clone in `C:\JUCE`)
- Toolchain: **CMake + Visual Studio 2022 + NMake** on Windows 11
- Build system: `CMakeLists.txt` (no Projucer, more modern and simpler)
- Active formats: **VST3** and **Standalone**
- Formats prepared but not buildable from Windows: **AU** (macOS), **AAX** (macOS + Avid dev license)
- DSP approach: simple start, iterate (YIN -> quantizer -> PSOLA)
- Scales: tonic + custom mode (major, minor, pentatonic, chromatic)
- GUI: 4 knobs (Speed, Amount, Key, Scale) + real-time pitch visualizer

### Files created

**DSP pipeline (Phase 1 + 4)**:
- `Source/PluginProcessor.h` / `.cpp`: complete pipeline
- `Source/PluginEditor.h` / `.cpp`: custom GUI (knobs + visualizer)
- `Source/dsp/PitchDetector.h` / `.cpp`: complete YIN algorithm
- `Source/dsp/ScaleQuantizer.h` / `.cpp`: tonic + mode quantization (5 modes)
- `Source/dsp/PitchShifter.h` / `.cpp`: PSOLA (Phase 4) - ring buffer + pitch marks + OLA
- `Source/dsp/FormantPreserver.h` / `.cpp`: Butterworth low-pass biquad
- `Source/dsp/RetargetEnvelope.h` / `.cpp`: 1st-order IIR for Speed
- `Source/dsp/PitchCurve.h` / `.cpp`: editable pitch curve (Graphic mode)
- `Source/ui/PitchVisualizer.h` / `.cpp`: semi-log visualization of pitch curves
- `Source/ui/PitchCurveEditor.h` / `.cpp`: interactive editor (drag, add, delete, preset)

**Build system**:
- `CMakeLists.txt`: native JUCE 8 CMake build (no Projucer)
- `init_vs_env.ps1`: MSVC + Windows SDK initialization in pure PowerShell
- `build.ps1`: configure + compile + tests
- `.gitignore`

**Documentation**:
- `roadmap.md`: complete project tracking with checkbox per phase
- `docs/changelogs/changelog-2026-06-09.md` (this file)
- `docs/architecture.md`: complete documentation (PSOLA, formants, retarget)

**Unit tests**:
- `test/Main.cpp`: test entry point
- `test/dsp/PitchDetectorTest.cpp`: 5 cases (440, 220, 100 Hz, silence, buffer too small)
- `test/dsp/ScaleQuantizerTest.cpp`: 7 cases (in/out of scale, tonic, chromatic, etc.)
- `test/dsp/RetargetEnvelopeTest.cpp`: 4 cases (Speed=0, Speed=200, target=1, reset)
- `test/dsp/FormantPreserverTest.cpp`: 3 cases (disabled, ratio=1, extreme ratio)
- `test/dsp/PitchCurveTest.cpp`: 10 cases (empty, 1 point, 2 points, modify, delete, sort, snap, serialization, presets, perf)

### Installation
- JUCE 8.0.12 installed in `C:\JUCE` via `git clone --branch 8.0.12`
- CMake 4.3.3 installed via winget (Kitware.CMake)

### Complete DSP pipeline implemented
1. **YIN detection** (4 steps, sub-sample parabolic interpolation)
2. **Auto mode: Quantization** (5 modes, circular distance)
3. **Graphic mode: editable PitchCurve** (linear interpolation, snap, presets)
4. **Mode selection** (AudioParameterInt parameter 0/1)
5. **Anti-formant filter** (Butterworth biquad, sqrt compensation)
6. **PSOLA** (pitch marks + 2-period Hann OLA, ring buffer)
7. **Retarget Envelope** (1st-order IIR for the Antares-style Speed)
8. **Wiring**: AudioIn -> PitchDetector -> [Auto: Quantize | Graphic: PitchCurve] -> Retarget -> FormantPreserver -> PSOLA -> AudioOut
9. **Transport time** read via `getPlayHead()` (used in Graphic mode)
10. **Latency** reported to the host (`getLatencySamples`)
11. **PitchCurve serialized** in `getStateInformation()` (XML sub-element `<PITCH_CURVE>`)

### Environment blocker
The detected Visual Studio 2022 installation is **incomplete**: no
STL headers (empty `include/` folder), no Desktop libs (only
`onecore`). CMake configures OK, but `cl.exe` cannot compile anything.

### User action required
Reinstall or complete Visual Studio 2022 with the
**"Desktop development with C++"** workload:
1. Open **Visual Studio Installer**
2. Modify the VS 2022 Community installation
3. Check the **"Individual components"** tab:
   - `MSVC v143 - VS 2022 C++ x64/x86 build tools`
   - `Windows 11 SDK` (or Windows 10 SDK)
4. Click "Modify" and reinstall (~5 GB)

Once VS is repaired, run:
```powershell
. .\init_vs_env.ps1
.\build.ps1 -RunTests
```

### Next steps
- Phase 4bis: transient preservation (onset detection)
- Phase 4bis: exact formant preservation via LPC
- Phase 4bis: Graphic mode - Bezier curves + pitch capture by click
- Phase 5: configure AU/AAX from a Mac
- Phase 6: subjective audio testing
- Bonus phase: temporal zoom on the pitch curve


## VS2022 toolchain validation (after user update)

### Full build succeeded (Release x64)
- CMake configured successfully (Visual Studio 17 2022 generator, A x64)
- Compilation succeeded for VST3 and Standalone
- Binaries produced:
  - Autotune Clone.vst3 (3.5 MB) - Windows x64 VST3 plugin
  - Autotune Clone.exe (4.7 MB) - standalone executable

### Source code fixes applied
- **dsp namespace renamed to `atdsp`** in all DSP modules
  - Cause: ambiguity with juce::dsp brought in by the juce_dsp module
    (included via JuceHeader.h)
  - Files modified: ScaleQuantizer, PitchDetector, PitchShifter,
    FormantPreserver, RetargetEnvelope, PitchCurve (h+cpp), PluginProcessor
    (h+cpp), PluginEditor.cpp, PitchCurveEditor (h+cpp), test/dsp/*, docs
- **Updated JUCE 8 APIs**:
  - XmlElement::getFloatAttribute no longer exists, replaced by
    getDoubleAttribute with a cast (PitchCurve.cpp)
  - juce::Array<T>::resize no longer takes an initial value as 2nd arg,
    replaced by clear() + add() in a loop (PitchVisualizer.cpp)
  - AudioProcessor::getLatencySamples() is NOT virtual in JUCE 8,
    removed the override (PluginProcessor.h)
  - acceptsMidi(), producesMidi(), isMidiEffect() must be
    declared const (PluginProcessor.cpp)
  - juce::Array<T>::indexOf requires operator== on T, added on
    PitchPoint (PitchCurve.h)
- **Missing include**: ScaleQuantizer.h added in PitchCurve.h
  (required for the `atdsp::Scale` type used by snapToScale)
- **VST2/VST3 warning**: JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1
  added to the compile definitions (CMakeLists.txt) since we do not
  ship a VST2

### Build workaround: manual generation of JuceHeader.h
- JUCE 8 CMake support does not automatically generate JuceHeader.h
  in all configurations
- Added a step in build.ps1 that:
  1. Locates juceaide.exe in _juce_build/
  2. Generates JuceHeader.h in Release/ from Defs.txt
  3. Copies the file one level up (JuceLibraryCode/) to match the
     default include path

### Environment verification
- MSVC 14.44.35207 (VS 2022 17.14): OK
- Windows SDK 10.0.19041.0: OK
- CMake 3.28.6: OK
- JUCE 8.0.12 in C:\JUCE: OK
- cl.exe, link.exe, rc.exe: all operational

### Next steps
- Unit tests: validate with -RunTests (after build verification)
- Propose to Jérôme the previously validated improvement ideas:
  onset detection, automatic harmonization, additional presets
  (vocoder, hard tune), user documentation (docs/user-guide.md)


## Launch crash fixes and UI corrections

### Crash #1: access violation `0x38` in `Rectangle<int>::getX()`
- **Symptom**: `Autotune Clone.exe` launches but crashes silently
  (invisible window)
- **Cause**: `setSize(800, 600)` and `setResizable(true,true)` were
  called BEFORE the creation of `pitchVisualizer` and `curveEditor`.
  `resized()` is triggered during `setSize` and accessed these
  still-null unique_ptrs
- **Fix**: move `setSize`, `setResizable`, `setResizeLimits` after
  the `addAndMakeVisible(*curveEditor)`
- **File**: `Source/PluginEditor.cpp`

### Crash #2: `AudioProcessor::Bus::isLayoutSupported` (after a few seconds)
- **Symptom**: the plugin launches but crashes after audio is activated
- **Cause**: `AudioProcessor::BusesProperties` was not specified in
  the constructor; the buses were created with `isActivatedByDefault=false`
  and once audio was active the host requested a layout that was not
  supported
- **Fix**: add
  ```cpp
  AudioProcessor::BusesProperties()
      .withInput  ("Input",  AudioChannelSet::stereo(), true)
      .withOutput ("Output", AudioChannelSet::stereo(), true)
  ```
  to the ctor of `AutotuneCloneAudioProcessor`
- **File**: `Source/PluginProcessor.cpp`

### Crash #3: `~HeapBlock` in the audio pipeline
- **Symptom**: crash during audio playback, in the destructor of
  `juce::HeapBlock<float>`
- **Cause**: `PitchShifter::addGrain` bounded its write by
  `outIdx < N` where `N = ringBufferSize = 8192`, but the output buffer
  is only `numSamples = 512` long; the write silently overflowed and
  the destructor later detected the inconsistency
- **Fix**: add an `outCapacity` parameter to `addGrain` and use
  `outIdx < outCapacity`; the caller passes `numSamples`
- **File**: `Source/dsp/PitchShifter.cpp`

### UI: invisible knobs, inactive drag, inconsistent key/scale
- **Knobs not visible**: the bottom bar was too short (80 px); fixed
  to 160 px. The `centerArea` calculation formula incorrectly divided
  the whole height instead of subtracting 160 px: replaced
  `bounds.removeFromBottom(getHeight() - 80)` with
  `bounds.removeFromTop(bounds.getHeight() - 160).reduced(10)`
- **Key/Scale in ComboBox**: a rotary slider makes no sense for
  discrete values. Replaced with `juce::ComboBox`
  (C, C#, ..., B / Major, Minor, Pent. Maj, Pent. Min, Chromatic) with
  manual binding to the `AudioParameterInt` via `getRawParameterValue`
- **Inactive point drag (Graphic mode)**: `mouseDown` and `mouseDrag`
  were correctly called, but the write via the setter did not propagate
  the new value (the read in `paint()` always showed
  `pt.pitch=12.0`). Cause: `Array<T>::operator[]` returns a
  *reference* but the read was probably made on a temporary copy
  - **Fix**: new `setPointPitch(int, float)` method in `PitchCurve`
    that uses `points.getReference(index).pitch = pitch` to write
    directly
  - **Hit-test** widened from 12 to 30 px to ease selection
  - **Gray overlay** in Auto mode (text "Auto mode - read only")
- **Files**: `Source/PluginEditor.cpp`, `Source/PluginEditor.h`,
  `Source/dsp/PitchCurve.h`, `Source/ui/PitchCurveEditor.cpp`,
  `Source/ui/PitchCurveEditor.h`

### Debug instrumentation cleanup
- Removed the `dbgLog` function and all its calls in
  `Source/PluginProcessor.cpp` and `Source/PluginEditor.cpp`
- Removed the `void dbgLog(...)` declaration in
  `Source/PluginProcessor.h`
- Removed the `juce::Logger::writeToLog` and the `autotune_paint.log`
  write blocks in:
  - `Source/PluginEditor.cpp::paint`
  - `Source/ui/PitchCurveEditor.cpp::paint` and `mouseDrag`
  - `Source/ui/PitchVisualizer.cpp::paint`
- Removed `Source/DebugMinidump.cpp` (SetUnhandledExceptionFilter +
  AddVectoredExceptionHandler) and its line in `CMakeLists.txt`
  (the sandbox could not write to `C:\Users\User\Desktop`, so the
  minidump never worked; the productive approach = VS breakpoints)
- Removed `debug-standalone-no-window.md` (debug session note)

### Known regression: PSOLA audible but distorted
- The PSOLA grains are produced without memory overflow (post-fix #3)
  but the resulting sound remains distorted (lost grains, approximate
  alignment). To be rewritten properly in Phase 4bis (transient
  preservation + onset detection)


## Full PSOLA rewrite

### Bugs identified in the previous implementation
- **Grain = 4*T0 instead of 2*T0**: `halfLen = 2*period` made the grain
  4 periods instead of the expected 2, excessively spreading the grain
- **Completely wrong synthesis position**: `(m * synthPeriod) %
  ringBufferSize` modulo the ring (8192) instead of the real output
  sample number (typically 512); the majority of grains were silently
  discarded
- **markA/markB mapping unused**: the `markB` variable was computed then
  explicitly set to `(void)`; no real purpose
- **No continuity between blocks**: no memory of the next synthesis
  mark position, so it was impossible to correctly handle several
  successive blocks

### New implementation (PitchShifter.cpp)
- **PitchMark = { absoluteSample, ringIdx }**: we keep the absolute time
  in addition to the ring buffer index, which lets us reason in linear
  time and fall back on the correct input sample
- **Grain = 2*T0**: half-window = T0, Hann window centered on the
  analysis mark, as specified in the PSOLA literature
- **Synthesis by binary search**: for each synthesis mark at time
  `t_out` (T0' interval), we find the closest analysis mark via
  `findClosestAnalysisMark()` (bsearch on `absoluteSample`)
- **Inter-block continuity**: `nextSynthMarkSample` keeps the next mark
  position between `process()` calls and is re-aligned on `blockStart`
  if we ever drifted
- **Adaptive COLA normalization**: `grainGain = 1 / max(1, 2*T0/T0p)`,
  divided by the real number of overlapping grains (1, 2, 4, ...
  depending on the ratio)
- **Simplified `addGrain`**: the `gain` passed as a parameter is already
  normalized (no magic division by 2 inside)
- **Renamed `findNextPitchMark` -> `findPeak`**: the function only looks
  for a max, not a "next" mark
- **Zero-crossing passthrough**: if `f0 <= 0` or `ratio = 1`, the ring
  buffer is still fed and `nextSynthMarkSample` is reset to
  `totalSamplesWritten` to avoid a big jump if the user re-enables pitch

### Files modified
- `Source/dsp/PitchShifter.h`: new `nextSynthMarkSample` member, new
  `PitchMark` struct with `absoluteSample`, new `findClosestAnalysisMark`
  method, `findNextPitchMark` -> `findPeak` signature
- `Source/dsp/PitchShifter.cpp`: `process()` fully rewritten, `addGrain`
  simplified, `findPeak` added, `findClosestAnalysisMark` added,
  `prepare` and `reset` update `nextSynthMarkSample`

### Expected result
- Audible and clean pitch shift for `ratio` in [0.5, 2.0]
- No clicks or lost grains
- Latency unchanged (same 8192-sample ring buffer)

### Known limitations (to improve later)
- No transient preservation (onset detection to do)
- For `ratio < 0.5` or `ratio > 2.0`, the grain becomes too long
  relative to the hop and the OLA produces artefacts
- MPM or another more robust pitch detector complementing YIN
  would improve pitch mark stability


## Ring buffer wraparound bug fix (PSOLA)

### Symptom reported by Jérôme
"I don't feel like I'm hearing my tuned voice no matter which parameter
I change, even at 0ms and amount=1. Also, if I take the visualizer out
of bypass and Amount>0, the audio is completely wrong, hatched and
distorted."

### Root cause
In the PSOLA rewrite, each PitchMark stores a `ringIdx` (the index in the
ring buffer at the moment the peak was detected). But after ~186 ms
(8192 samples at 44100 Hz), the ring buffer wraps and this index becomes
stale. In phase 2, we then read OLD data (dating back several hundred ms)
instead of the current samples. This produces:
- Inconsistent output (mix of current and old data)
- "Holes" and "spikes" that give a hatched sound
- No correct note in output (the pitch shift is completely lost in the
  noise)

### Fix
New `ringPosAt(long long absSample)` method that dynamically computes the
ring buffer position from the mark's absolute time:
```cpp
pos = (writeIndex - (totalSamplesWritten - absSample)) mod N
```
Called in phase 2 instead of `analysisMarks[idx].ringIdx`.
The ringIdx stored in the PitchMark is kept for informational purposes
(useful for debug) but is no longer used for reading.

### Verification
- The ring buffer index changes every sample (writeIndex increments).
  By storing ringIdx statically, we would read a sample further and
  further back in the past. After 8192 samples, we read a very old
  sample (0 or N-1) depending on the wraparound.
- With the dynamic calculation, ringPosAt always returns the position of
  the sample at the absolute instant `absSample` in the CURRENT state of
  the ring buffer.

### Files modified
- `Source/dsp/PitchShifter.h`: declaration of `ringPosAt`
- `Source/dsp/PitchShifter.cpp`: implementation of `ringPosAt`,
  modified phase 2 to use it instead of `ringIdx`


## Stereo bug fix (silent right channel)

### Symptom reported by Jérôme
"I still feel like I'm hearing my voice and no tuning is applied,
whether the visualization is enabled or not. On the other hand, if the
visualization is enabled, the sound is completely distorted in the left
ear and silent, hatched or intermittent on the right ear"

### Root cause
In PSOLA phase 2, the update of `nextSynthMarkSample` (= `t_out` after
the while loop) happened INSIDE the channel loop. Consequence:
- Channel 0 (left): synthesis runs normally, t_out advances up to
  `blockEnd + T0p`, then `nextSynthMarkSample = blockEnd + T0p`
- Channel 1 (right): `t_out = nextSynthMarkSample = blockEnd + T0p`, the
  `t_out < blockEnd` condition is FALSE, the while loop never runs, so
  the right channel is never written
- The right channel output buffer stays zero (= silence)
- In output: left ear = PSOLA (with the other wraparound bug), right ear
  = silence (with sometimes transients when the block was partially
  processed, hence the "hatched/intermittent")

### Bonus bug detected at the same time
Pitch mark detection was also inside the channel loop. Result: the same
pitch mark was added N times (once per channel) in `analysisMarks`, which:
- Makes the list grow 2x faster than necessary
- The bsearch always returns the 1st occurrence (channel 0), so no
  visible functional impact, but it is wasteful

### Fix
- Phase 1 refactored: we write ALL channels into the ring buffer
  (shared position memory), then detect pitch marks ONCE on channel 0
- Phase 2 refactored: `synthStart` is computed once before the channel
  loop, `t_out` is a LOCAL variable in the while loop, and
  `nextSynthMarkSample` is updated only after the loop (and systematically,
  to avoid any drift)

### Files modified
- `Source/dsp/PitchShifter.cpp`: refactoring of phases 1 and 2


## Pitch mark absoluteSample off-by-one fix

### Symptom reported by Jérôme
"No more stereo problem. On the other hand, still glitches on the audio
when the visualizer is enabled. I noticed the problem only happens when
there is a red line, i.e. an out-of-tune detected and normally corrected.
I still hear my voice, but the version that is supposed to be tuned is
100% distorted."

### Root cause
In PSOLA phase 1, after writing a sample and incrementing `writeIndex` +
`totalSamplesWritten`, the pitch mark was added with
`analysisMarks.add({ totalSamplesWritten, ... })`. But `totalSamplesWritten`
represents the NEXT sample to write, not the LAST one written. The last
written one is at time `totalSamplesWritten - 1` at position
`writeIndex - 1`.

Consequence for `ringPosAt()`:
- Formula: pos = (writeIndex - totalSamplesWritten + absSample) mod N
- With absoluteSample = totalSamplesWritten (bug): pos = writeIndex (the
  position of the NEXT write = old data N samples back, up to 186 ms)
- With absoluteSample = totalSamplesWritten - 1 (fix): pos = writeIndex
  - 1 (the position of the LAST written = current data)

Audio effect: for in-scale notes (ratio = 1.0), the code takes the
passthrough branch, so nothing goes through PSOLA. For out-of-scale notes
(ratio != 1.0), PSOLA reads old data from the ring buffer from ~186 ms on,
which produces an inconsistent mix of current and old data -> "100%
distorted" reported by Jérôme.

### Fix
- `analysisMarks.add({ totalSamplesWritten - 1, markRingPos })` instead
  of `analysisMarks.add({ totalSamplesWritten, markRingPos })`
- `ringPosAt` keeps its formula: pos = writeIndex - totalSamplesWritten
  + absSample, which is now correct since absoluteSample properly denotes
  the time of the last write

### Notes
- The T0/2 offset between the detection position (markRingPos) and the
  "totalSamplesWritten - 1" time is acceptable: the 2*T0 Hann window is
  wide enough to absorb a shift of a few % of the grain.
- For an "exact" version, we could store the exact peak offset relative
  to the last write, but that complicates the code for a negligible audio
  gain.

### File modified
- `Source/dsp/PitchShifter.cpp`: phase 1, added "- 1" on
  totalSamplesWritten when adding the pitch mark
