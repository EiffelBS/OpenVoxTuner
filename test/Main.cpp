#pragma once
// Main.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <juce_audio_processors/juce_audio_processors.h>

// Test inclusion (each .cpp file self-registers automatically
// via static juce::UnitTest instances).
#include "dsp/ScaleQuantizerTest.cpp"
#include "dsp/RetargetEnvelopeTest.cpp"
#include "dsp/FormantPreserverTest.cpp"
#include "dsp/LpcFormantPreserverTest.cpp"
#include "dsp/LpcFramingTest.cpp"
#include "dsp/PitchCurveTest.cpp"
#include "dsp/MidiImporterTest.cpp"
#include "dsp/AudioCurveImporterTest.cpp"
#include "dsp/VibratoTest.cpp"
#include "dsp/KeyDetectorTest.cpp"
#include "dsp/KeyBridgeTest.cpp"
#include "dsp/SidechainBusLayoutTest.cpp"
#include "dsp/SpeedFloorTest.cpp"
#include "dsp/HardTuneAttackTest.cpp"
#include "dsp/LatencyDeclarationTest.cpp"
#include "dsp/DetectorResponseTest.cpp"
#include "dsp/FormantPreserverModulationTest.cpp"
#include "dsp/BlockAwareOnePoleTest.cpp"
#include "dsp/PerformanceBudgetTest.cpp"
#include "dsp/PitchShifterOutputTest.cpp"
#include "dsp/PitchShifterOutputRmsTest.cpp"
#include "dsp/PitchShifterClickTest.cpp"
#include "dsp/HarmonyGainMatchTest.cpp"
#include "dsp/HarmonyAttackTest.cpp"
#include "dsp/PluginUndoTest.cpp"
#include "dsp/PluginPresetTest.cpp"
#include "dsp/MorphCoverageTest.cpp"
#include "dsp/UpwardCompressorTest.cpp"
#include "ui/PitchCurveEditorTest.cpp"
#include "ui/HarmonyLayoutTest.cpp"
#include "ui/EffectsCellLayoutTest.cpp"
#include "ui/ScaleKeyboardComponentTest.cpp"
#include "ui/LookAndFeelToggleTest.cpp"
#include "ui/TranslationCompletenessTest.cpp"
#include "ScaleSnapPipelineTest.cpp"
#include "dsp/DoublerTest.cpp"
#include "dsp/UnisonDetuneTest.cpp"
#include "dsp/DeEsserTest.cpp"
#include "dsp/EffectsBusTest.cpp"
#include "dsp/ReverbEffectTest.cpp"
#include "dsp/ReverbDuckerTest.cpp"
#include "dsp/NoiseGateEnableTest.cpp"
#include "dsp/SaturationEffectTest.cpp"
#include "dsp/OutputLimiterTruePeakTest.cpp"
#include "dsp/DelayEffectTest.cpp"

/** Console logger for the test runner.
 *  With no logger installed, JUCE sends Logger::writeToLog to OutputDebugString,
 *  so the runner's per-group "Starting tests in: Suite / group..." lines never
 *  reach the console. When a suite crashes with an access violation and no
 *  memory dump can be read, the last line printed IS the crashing group: this
 *  logger makes that observable from the command line alone. */
struct ConsoleTestLogger : public juce::Logger
{
    void logMessage (const juce::String& message) override
    {
        std::cout << message << "\n" << std::flush;
    }
};

int main (int argc, char* argv[])
{
    std::cout << "main start\n" << std::endl;

    ConsoleTestLogger consoleLogger;
    juce::Logger::setCurrentLogger (&consoleLogger);

    juce::UnitTestRunner runner;

    // Command-line names select which registered suites to run, in the given
    // order, so a crash can be localized to a suite pair without rebuilding.
    //
    // The selection is done here rather than with UnitTestRunner::runTestsWithName():
    // that helper (and the UnitTest::getTestsWithName behind it) only exists from
    // JUCE 8.0.9, and this tree builds against a pinned JUCE whose newest usable
    // tag keeps Font::getStringWidth (removed in 8.0.13) - so the runner API that
    // arrived with 8.0.9 is not always available. Matching getName() over
    // getAllTests() is exactly what getTestsWithName does internally, and it
    // compiles on every version.
    if (argc > 1)
    {
        juce::Array<juce::UnitTest*> selected;

        for (int a = 1; a < argc; ++a)
        {
            const juce::String wanted (argv[a]);

            // An empty name means "every suite", as getTestsWithName() reads it.
            if (wanted.isEmpty())
            {
                selected.addArray (juce::UnitTest::getAllTests());
                continue;
            }

            bool matched = false;

            for (auto* test : juce::UnitTest::getAllTests())
                if (test->getName() == wanted)
                {
                    selected.add (test);
                    matched = true;
                }

            if (! matched)
                std::cout << "[WARN] no test suite named '" << wanted << "'\n";
        }

        if (! selected.isEmpty())
            runner.runTests (selected);
    }
    else
        runner.runAllTests();

    // Print a summary to the console.
    int numPassed = 0, numFailed = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        auto* r = runner.getResult (i);
        if (r->failures > 0)
        {
            numFailed++;
            std::cout << "[FAIL] " << r->unitTestName << "\n";
            for (auto& msg : r->messages)
                std::cout << "       " << msg << "\n";
        }
        else
        {
            numPassed++;
            std::cout << "[ OK ] " << r->unitTestName
                      << " (" << r->passes << " assertions)\n";
        }
    }

    std::cout << "\n=========================\n";
    std::cout << "Result: " << numPassed << " OK, " << numFailed << " FAILED\n";
    std::cout << "=========================\n";
    std::cout.flush();

    juce::Logger::setCurrentLogger (nullptr);
    return numFailed == 0 ? 0 : 1;
}


