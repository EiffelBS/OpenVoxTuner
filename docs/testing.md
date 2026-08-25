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
│  ├─ BlockAwareOnePoleTest.cpp
│  ├─ FormantPreserverTest.cpp
│  ├─ FormantPreserverModulationTest.cpp
│  ├─ HarmonyAttackTest.cpp
│  ├─ HarmonyGainMatchTest.cpp
│  ├─ KeyBridgeTest.cpp
│  ├─ KeyDetectorTest.cpp
│  ├─ LpcFormantPreserverTest.cpp
│  ├─ MidiImporterTest.cpp
│  ├─ PerformanceBudgetTest.cpp
│  ├─ PitchCurveTest.cpp
│  ├─ PitchShifterClickTest.cpp
│  ├─ PitchShifterOutputTest.cpp
│  ├─ PitchShifterOutputRmsTest.cpp
│  ├─ PluginPresetTest.cpp
│  ├─ PluginUndoTest.cpp
│  ├─ RetargetEnvelopeTest.cpp
│  ├─ ScaleQuantizerTest.cpp
│  ├─ SidechainBusLayoutTest.cpp
│  ├─ SpeedFloorTest.cpp
│  ├─ UpwardCompressorTest.cpp
│  └─ VibratoTest.cpp
└─ ui/                               # UI component tests
   ├─ PitchCurveEditorTest.cpp
   └─ ScaleKeyboardComponentTest.cpp
```

The test suites found under `test/dsp/` cover pitch detection (YIN), scale
quantization, pitch shifting (PSOLA), retarget envelope, formant preservation
(LPC and multi-formant), MIDI file analysis/import into pitch curves, harmony
gain-match & attack, key detection and the key bridge, sidechain bus layout,
speed floor, block-aware one-pole filtering, performance budgets, plugin
preset/undo, upward compression, and vibrato.

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
...
=========================
Result: 111 OK, 0 FAILED
=========================
```

The process exits with code **0** when all tests pass, and **1** if any test fails,
so it can be used in CI.

## CI

The test target is built and executed by the GitHub Actions CI workflows (see
`.github/workflows/`). A passing `OpenVoxTunerTests` run is a required gate before
a pull request can be merged.

## Related pages

- [Build Guide](build-guide.md) — build OpenVoxTuner and its test target.
- [Contributing](contributing.md) — the contribution workflow, including the
  requirement to run the test suite.
