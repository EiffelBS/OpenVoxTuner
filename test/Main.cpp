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
#include "dsp/VibratoTest.cpp"
#include "dsp/KeyDetectorTest.cpp"
#include "dsp/KeyBridgeTest.cpp"
#include "dsp/SidechainBusLayoutTest.cpp"
#include "dsp/SpeedFloorTest.cpp"
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
#include "ScaleSnapPipelineTest.cpp"
#include "dsp/DoublerTest.cpp"
#include "dsp/UnisonDetuneTest.cpp"
#include "dsp/DeEsserTest.cpp"
#include "dsp/EffectsBusTest.cpp"
#include "dsp/ReverbEffectTest.cpp"
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
    if (argc > 1)
    {
        for (int a = 1; a < argc; ++a)
            runner.runTestsWithName (juce::String (argv[a]));
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


