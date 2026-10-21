# Testing

OpenVoxTuner ships with a unit test suite that verifies the DSP pipeline, UI
components, and plugin behavior. Tests run in a console application named
**`OpenVoxTunerTests`**, a JUCE console app built from `test/Main.cpp`.

!!! note "Test framework"
    The tests use the **JUCE unit test framework** (`juce::UnitTest` / `juce::UnitTestRunner`).
    Each test file registers a `juce::UnitTest` subclass at static initialization time,
    and `Main.cpp` includes every test file so a single binary runs the whole suite.

## Where the tests live

```
test/
├─ Main.cpp                          # Test runner entry point (runs all tests)
├─ ScaleSnapPipelineTest.cpp         # Scale snap pipeline
├─ formant_preservation_benchmark.py # Python benchmark (not part of the C++ suite)
├─ dsp/                              # DSP test suites (one file per module)
│  ├─ AudioCurveImporterTest.cpp
│  ├─ BlockAwareOnePoleTest.cpp
│  ├─ DeEsserTest.cpp
│  ├─ DelayEffectTest.cpp
│  ├─ DoublerTest.cpp
│  ├─ EffectsBusTest.cpp
│  ├─ FormantPreserverTest.cpp
│  ├─ FormantPreserverModulationTest.cpp
│  ├─ HarmonyAttackTest.cpp
│  ├─ HarmonyGainMatchTest.cpp
│  ├─ KeyBridgeTest.cpp
│  ├─ KeyDetectorTest.cpp
│  ├─ LpcFormantPreserverTest.cpp
│  ├─ LpcFramingTest.cpp
│  ├─ MidiImporterTest.cpp
│  ├─ PerformanceBudgetTest.cpp
│  ├─ PitchCurveTest.cpp
│  ├─ PitchShifterClickTest.cpp
│  ├─ PitchShifterOutputTest.cpp
│  ├─ PitchShifterOutputRmsTest.cpp
│  ├─ PluginPresetTest.cpp
│  ├─ PluginUndoTest.cpp
│  ├─ RetargetEnvelopeTest.cpp
│  ├─ ReverbEffectTest.cpp
│  ├─ SaturationEffectTest.cpp
│  ├─ ScaleQuantizerTest.cpp
│  ├─ SidechainBusLayoutTest.cpp
│  ├─ SpeedFloorTest.cpp
│  ├─ UpwardCompressorTest.cpp
│  ├─ UnisonDetuneTest.cpp
│  └─ VibratoTest.cpp
└─ ui/                               # UI component tests
   ├─ LookAndFeelToggleTest.cpp
   ├─ PitchCurveEditorTest.cpp
   └─ ScaleKeyboardComponentTest.cpp
```

The test suites found under `test/dsp/` cover pitch detection (YIN), scale
quantization, pitch shifting (PSOLA), retarget envelope, formant preservation
(LPC and multi-formant), MIDI file analysis/import into pitch curves, audio-file
analysis and note segmentation into pitch curves (WAV/AIFF/FLAC/OGG),
harmony gain-match & attack, the stereo doubler (detune ratio, Haas offset, constant
perceived RMS, mono compatibility, amount-0 bypass), the de-esser (band-limited
detection, reduction depth, hysteresis window, stereo link, release, click-free
power toggle, bit-identical bypass), the fixed Unison detune
(identical voice pair, correlated +6 dB sum decorrelated to +3 dB, beat rate,
mono L+R sum), key detection and the key
bridge, sidechain bus layout, speed floor, block-aware one-pole filtering,
performance budgets, plugin preset/undo, upward compression, the effects-bus dispatch
(empty bus bit-identical, master all-on equal to the legacy single loop, trim, linked
channels), the shared send room, and vibrato. The master saturation stage is covered by
11 suites: drive 0 as a transparent bypass, disable fading out, measured makeup within
1 dB across the drive range, DC blocked on a sine and on a ramp, a hot ARA buffer kept
inside full scale, aliasing bins killed by the 2x oversampling, warmth rolling off the
added harmonics, drive rising monotonically, stereo independence, wet-mix neutrality, and
the internal delay equal to the measured filter centre.

The tempo-synced lead delay (`test/dsp/DelayEffectTest.cpp`) is covered by suites asserting:
bit-identical bypass after the enable ramp + tail decay, and a no-op at `wetMix = 0`; copy length
equal to `round((60/bpm) * fraction * sr)` for all five division indices, with the L-input echo arriving in R
within ±3 samples of that target (measured after a silence warm-up, because `reset()` re-seeds the copy ramp to
the floor); 60 BPM doubling the 120 BPM quarter copy; successive echo peaks stepping down by a ratio following the
feedback knob, and feedback 0 leaving exactly one echo; L/R energy alternating across echoes (R → L → R) with an
R-only input mirroring it; mid-stream division change ramped without a single-sample jump and settled inside ~35 ms;
no DC build-up and no explosion at feedback 0.9 on a sustained low tone; the clamp floor respected at bpm 400, the
target staying inside `getLineCapacity()` for bpm 20 × "1/2", every sample finite, the dry sample bit-identical, mono
input echoing on both sides, and `reset()` flushing both lines. **Suite and assertion counts are taken from the green
run of `OpenVoxTunerTests` — they must not be quoted before that run.**

## Building the tests

The tests are defined in `CMakeLists.txt` with
`juce_add_console_app(OpenVoxTunerTests ...)`. To build them:

```bash
# Windows
cmake --build build --config Release --target OpenVoxTunerTests

# macOS
cmake --build build-mac --config Release --target OpenVoxTunerTests
```

## Running the tests

Run the produced console executable directly:

=== "Windows"

    ```powershell
    .\build\OpenVoxTunerTests_artefacts\Release\OpenVoxTunerTests.exe
    ```

=== "macOS"

    ```bash
    ./build/OpenVoxTunerTests_artefacts/Release/OpenVoxTunerTests
    ```

The runner runs all registered tests and prints one line per suite
(`[ OK ]` or `[FAIL]`), followed by a summary:

```text
[ OK ] ScaleQuantizer (28 assertions)
[ OK ] PitchShifterOutput (12 assertions)
[ OK ] DeEsser (1024 assertions)
[ OK ] Saturation (512 assertions)
...
=========================
Result: 174 OK, 0 FAILED
=========================
```

The process exits with code **0** when all tests pass, and **1** if any test fails,
so it can be used in CI.

### Running one suite, and localizing a crash

Passing suite names on the command line runs them in order:

```powershell
.\build\OpenVoxTunerTests_artefacts\Release\OpenVoxTunerTests.exe Doubler HarmonyGainMatch
```

Two runner quirks matter when a suite crashes (an access violation kills the
process, and no memory dump is readable):

- **One name per run.** `UnitTestRunner::runTests()` clears its results at the
  start of every call, so a multi-name run prints only the **last** suite's
  results. Multi-name runs detect crashes (the process dies) but hide assertion
  failures of the earlier suites — verify each suite individually and check the
  exit code.
- **The name filter is per test object.** A file with one `UnitTest` subclass
  containing many `beginTest(...)` groups cannot be split by name, so `Main.cpp`
  installs a stdout logger: JUCE sends `Logger::writeToLog` to
  `OutputDebugString` when no logger is set, which hides the runner's per-group
  "Starting tests in: Suite / group..." lines. With the logger, the last line
  printed before the process dies **is** the crashing group.

### Writing files from a confined test run

A binary whose image lives inside the workspace runs at **Low integrity**
(`S-1-16-4096`), so it cannot write to `%TEMP%` (Medium integrity). Suites that
generate files — `MidiImporterTest` writes temporary `.mid` files,
`AudioCurveImporterTest` writes temporary `.wav` files — must write next to the
executable, inside the build tree, not to the system temp folder.

## CI

The test target is built and executed by the GitHub Actions CI workflows (see
`.github/workflows/`). A passing `OpenVoxTunerTests` run is a required gate before
a pull request can be merged.

## Related pages

- [Build Guide](build-guide.md) — build OpenVoxTuner and its test target.
- [Contributing](contributing.md) — the contribution workflow, including the
  requirement to run the test suite.
