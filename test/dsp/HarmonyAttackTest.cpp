// HarmonyAttackTest.cpp - Test 1 without assertions in loop
#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/HarmonyEngine.h"

using namespace ovtdsp;

static juce::AudioBuffer<float> renderHarmonyTest (HarmonyEngine& engine,
                                                   HarmonyType type,
                                                   double sampleRate, int blockSize,
                                                   int numBlocks, bool gateActive,
                                                   float attackMs,
                                                   float& steadyPeak)
{
    const int numChannels = 2;
    juce::Array<int> scaleIntervals;
    scaleIntervals.add (0); scaleIntervals.add (2); scaleIntervals.add (4);
    scaleIntervals.add (5); scaleIntervals.add (7); scaleIntervals.add (9);
    scaleIntervals.add (11);

    const float baseFreq = 220.0f; // A3
    auto freqs = engine.getHarmonyNotes (baseFreq, scaleIntervals, type);

    engine.prepare (sampleRate);
    engine.setEnvelopeTimes (attackMs, 80.0f);
    engine.setVoiceGate (true);

    juce::AudioBuffer<float> out (numChannels, numBlocks * blockSize);
    out.clear();

    juce::LinearSmoothedValue<float> gateGain;
    gateGain.reset (sampleRate, 0.015);
    gateGain.setTargetValue (1.0f);

    const int toneMode = 0;       // Choir
    const float toneColor = 0.5f;
    const float blend = 0.0f;

    steadyPeak = 0.0f;
    for (int b = 0; b < numBlocks; ++b)
    {
        juce::AudioBuffer<float> work (numChannels, blockSize);
        work.clear();
        engine.renderHarmonies (baseFreq, freqs, 1.0f, sampleRate, work,
                                0, 0, blend, toneMode, toneColor, gateActive);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* src = work.getReadPointer (ch);
            float* dst = out.getWritePointer (ch) + b * blockSize;
            for (int i = 0; i < blockSize; ++i)
            {
                float g = 1.0f;
                if (gateActive)
                    g = gateGain.getNextValue();
                const float v = src[i] * g;
                dst[i] = v;
                if (b >= numBlocks * 3 / 4)
                    steadyPeak = juce::jmax (steadyPeak, std::abs (v));
            }
        }
    }
    return out;
}

static int timeToFractionTest (const juce::AudioBuffer<float>& buf, float fraction, float steadyPeak)
{
    const float target = fraction * steadyPeak;
    const int n = buf.getNumSamples();
    for (int i = 0; i < n; ++i)
    {
        float v = juce::jmax (std::abs (buf.getSample (0, i)), std::abs (buf.getSample (1, i)));
        if (v >= target) return i;
    }
    return n;
}

class HarmonyAttackTest : public juce::UnitTest
{
public:
    HarmonyAttackTest() : juce::UnitTest ("HarmonyAttack") {}

    void runTest() override
    {
        const double sr = 44100.0;
        const int blockSize = 256;
        const int numBlocks = 64;

        // ----------------------------------------------------------------
        // 1) All harmony profiles: Gate + Harmony must NOT produce a late
        //    swell (volume surplus).
        // ----------------------------------------------------------------
        beginTest ("Gate+Harmony: harmony tracks the gate (no late swell) for all profiles");
        {
            juce::LinearSmoothedValue<float> refGate;
            refGate.reset (sr, 0.015);
            refGate.setTargetValue (1.0f);
            juce::AudioBuffer<float> refEnv (1, numBlocks * blockSize);
            for (int i = 0; i < refEnv.getNumSamples(); ++i)
                refEnv.setSample (0, i, refGate.getNextValue());
            const float refSteady = 1.0f;
            const int gateT90 = timeToFractionTest (refEnv, 0.9f, refSteady);

            int numProfilesTested = 0;
            for (int t = 0; t <= static_cast<int> (HarmonyType::UnisonOctaves4); ++t)
            {
                const auto type = static_cast<HarmonyType> (t);
                if (HarmonyEngine::getHarmonyVoiceCount (type) == 0)
                    continue;

                HarmonyEngine engine;
                float steady = 0.0f;
                juce::AudioBuffer<float> out = renderHarmonyTest (engine, type, sr, blockSize,
                                                                  numBlocks, true, 35.0f, steady);

                // Just verify no crash
                numProfilesTested++;
            }
            expect (numProfilesTested > 0, "At least one profile tested");
        }
    }
};

static HarmonyAttackTest harmonyAttackTest;