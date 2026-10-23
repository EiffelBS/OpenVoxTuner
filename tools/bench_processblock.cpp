// bench_processblock.cpp
// OpenVoxTuner — timing harness for the real OpenVoxTunerAudioProcessor::processBlock
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// WHY THIS EXISTS
// The unit-test target cannot instantiate the processor (it does not compile the
// plugin sources: no JucePlugin_* macros, no plugin wrapper), and
// PerformanceBudgetTest therefore times PitchShifter + FormantPreserver in
// isolation. That leaves processBlock() — 2099 lines of the real-time path —
// with no gate of any kind: a refactor of it could not be validated by the test
// suite. This tool instantiates the real processor through createPluginFilter(),
// prepares it like a host would, and reports the cost per block.
//
// It links the plugin's shared-code target, which is the whole point: the code
// under measurement is the shipped code, compiled with the shipped flags.
//
// USAGE
//   bench_processblock [--blocks N] [--block-size N] [--rate N] [--json]
//
// Prints, for the default parameter state and then with Harmony enabled (the two
// shapes of the hot path), the mean/min/max cost per block and the fraction of
// one core an audio stream costs. Compare two builds by running it before and
// after the change on the same machine, idle, same output.

#include <juce_audio_processors/juce_audio_processors.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

// Defined in Source/PluginProcessor.cpp (the JUCE plugin entry point).
extern juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{

struct Options
{
    int blocks = 2000;
    int blockSize = 128;
    double sampleRate = 44100.0;
    bool json = false;
};

Options parseArgs (int argc, char** argv)
{
    Options o;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp (argv[i], "--blocks") == 0 && i + 1 < argc)
            o.blocks = std::atoi (argv[++i]);
        else if (std::strcmp (argv[i], "--block-size") == 0 && i + 1 < argc)
            o.blockSize = std::atoi (argv[++i]);
        else if (std::strcmp (argv[i], "--rate") == 0 && i + 1 < argc)
            o.sampleRate = std::atof (argv[++i]);
        else if (std::strcmp (argv[i], "--json") == 0)
            o.json = true;
        else
        {
            std::printf ("usage: bench_processblock [--blocks N] [--block-size N] "
                         "[--rate N] [--json]\n");
            std::exit (2);
        }
    }
    return o;
}

/** Sets a parameter by its stable ID, the way a host automation write would. */
bool setParam (juce::AudioProcessor& proc, const juce::String& id, float normalisedValue)
{
    for (auto* p : proc.getParameters())
    {
        // Only the value-tree parameters carry a stable ID; the host-facing
        // base class (AudioProcessorParameter) does not expose one.
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            if (ranged->getParameterID() == id)
            {
                ranged->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalisedValue));
                return true;
            }
        }
    }
    return false;
}

/** A voice-like test signal: harmonic tone with vibrato and a little noise.
    Not silence, so the silence gate never short-circuits the heavy path. */
void fillVoiceLike (juce::AudioBuffer<float>& buffer, double sampleRate, int blockStartSample)
{
    const int n = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    for (int i = 0; i < n; ++i)
    {
        const double t = (double) (blockStartSample + i) / sampleRate;
        const double vib = 1.0 + 0.006 * std::sin (2.0 * juce::MathConstants<double>::pi * 5.0 * t);
        const double f0 = 220.0 * vib;                       // A3 with 5 Hz vibrato
        double sample = 0.0;
        for (int h = 1; h <= 6; ++h)                          // 6 harmonics: formants/DSP see content
            sample += std::sin (2.0 * juce::MathConstants<double>::pi * f0 * h * t) / (double) h;
        sample *= 0.12;                                       // about -18 dBFS peak
        sample += 0.0005 * std::sin (2.0 * juce::MathConstants<double>::pi * 3000.0 * t); // a little air
        for (int ch = 0; ch < numCh; ++ch)
            buffer.setSample (ch, i, (float) sample);
    }
}

struct Stats
{
    double meanMs = 0.0, minMs = 0.0, maxMs = 0.0, p95Ms = 0.0;
    double deadlineMs = 0.0;
};

Stats runCase (const char* label, const juce::StringArray& paramWrites,
               const Options& o, bool verbose)
{
    std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());
    if (proc == nullptr)
    {
        std::printf ("createPluginFilter() returned null\n");
        std::exit (1);
    }

    juce::AudioBuffer<float> buffer (juce::jmax (1, proc->getTotalNumInputChannels()), o.blockSize);
    juce::MidiBuffer midi;

    proc->setRateAndBufferSizeDetails (o.sampleRate, o.blockSize);

    for (const auto& w : paramWrites)
    {
        const int colon = w.indexOfChar (':');
        const auto id = w.substring (0, colon);
        const auto value = w.substring (colon + 1).getFloatValue();
        if (! setParam (*proc, id, value))
            std::printf ("  (parameter not found: %s)\n", id.toRawUTF8());
    }

    proc->prepareToPlay (o.sampleRate, o.blockSize);

    // Warm up (caches, first-run allocations, denormal state): 300 blocks.
    for (int b = 0; b < 300; ++b)
    {
        fillVoiceLike (buffer, o.sampleRate, b * o.blockSize);
        midi.clear();
        proc->processBlock (buffer, midi);
    }

    std::vector<double> perBlockMs;
    perBlockMs.reserve ((size_t) o.blocks);
    const auto ticksPerSecond = (double) juce::Time::getHighResolutionTicksPerSecond();

    for (int b = 0; b < o.blocks; ++b)
    {
        fillVoiceLike (buffer, o.sampleRate, (b + 300) * o.blockSize);
        midi.clear();

        const auto t0 = juce::Time::getHighResolutionTicks();
        proc->processBlock (buffer, midi);
        const auto t1 = juce::Time::getHighResolutionTicks();

        perBlockMs.push_back ((double) (t1 - t0) * 1000.0 / ticksPerSecond);
    }

    proc->releaseResources();

    Stats s;
    s.deadlineMs = (double) o.blockSize / o.sampleRate * 1000.0;
    std::sort (perBlockMs.begin(), perBlockMs.end());
    double sum = 0.0;
    for (double v : perBlockMs)
        sum += v;
    s.meanMs = sum / (double) perBlockMs.size();
    s.minMs = perBlockMs.front();
    s.maxMs = perBlockMs.back();
    s.p95Ms = perBlockMs[(size_t) ((double) perBlockMs.size() * 0.95)];

    if (verbose)
    {
        std::printf ("  %-28s mean %7.4f ms  min %7.4f  p95 %7.4f  max %7.4f  %s\n",
                     label, s.meanMs, s.minMs, s.p95Ms, s.maxMs,
                     (o.json ? "" : ""));
    }
    return s;
}

} // namespace

int main (int argc, char** argv)
{
    const auto o = parseArgs (argc, argv);

    // MessageManager: the processor's constructor creates juce::Timer objects
    // (file logger flush, update check) which need a message thread.
    juce::ScopedJuceInitialiser_GUI juceInit;

    const double audioSecondPerBlock = (double) o.blockSize / o.sampleRate; // seconds
    (void) audioSecondPerBlock;

    std::printf ("bench_processblock: %.0f Hz, %d samples/block, %d blocks measured "
                 "(%d warm-up), deadline %.3f ms\n",
                 o.sampleRate, o.blockSize, o.blocks, 300,
                 (double) o.blockSize / o.sampleRate * 1000.0);

    // (1) the shipped default state, (2) the full hot path (harmony + formant on).
    const auto defaults = runCase ("default state", {}, o, true);
    const auto harmonyOn = runCase ("harmony + formant on",
                                    { "amount:1.0", "harmony_enable:1.0", "harmony_gain:1.0",
                                      "formant_enable:1.0", "noise_gate_enable:1.0",
                                      "deess_enable:1.0", "saturation_enable:1.0" },
                                    o, true);

    if (o.json)
    {
        std::printf ("{\"rate\":%.0f,\"block\":%d,\"default_ms\":%.6f,"
                     "\"harmony_ms\":%.6f,\"deadline_ms\":%.6f}\n",
                     o.sampleRate, o.blockSize, defaults.meanMs, harmonyOn.meanMs,
                     defaults.deadlineMs);
    }
    else
    {
        const double deadline = defaults.deadlineMs;
        std::printf ("\n  default state        %6.2f %% of the block deadline\n",
                     100.0 * defaults.meanMs / deadline);
        std::printf ("  harmony + formant    %6.2f %% of the block deadline\n",
                     100.0 * harmonyOn.meanMs / deadline);
        std::printf ("  (one audio second at this setting = %.1f %% of one core, "
                     "harmony path)\n",
                     100.0 * harmonyOn.meanMs / deadline);
    }
    return 0;
}
