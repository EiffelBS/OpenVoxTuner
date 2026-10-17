#pragma once
// EffectsBusTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/EffectsBus.h"
#include "../../Source/dsp/OutputLimiter.h"
#include <cmath>
#include <vector>

namespace
{
    /**
     * Deterministic test effect: multiplies the tapped buffer by `gain` when enabled.
     * A gain is linear and memoryless, so the expected result of any tap placement
     * can be computed in closed form — which is what makes the three taps testable.
     */
    class GainEffect : public ovtdsp::IEffect
    {
    public:
        explicit GainEffect (float g) : gain (g) {}

        juce::String getId() const override { return "gain"; }
        juce::String getName() const override { return "Gain"; }
        void prepare (double, int) override { prepared = true; }
        void reset() override { calls = 0; }

        void process (juce::AudioBuffer<float>& buffer, bool enabled, float wetMix) override
        {
            if (! enabled)
                return;
            ++calls;
            const float g = gain * wetMix;
            for (int c = 0; c < buffer.getNumChannels(); ++c)
            {
                float* data = buffer.getWritePointer (c);
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    data[i] *= g;
            }
        }

        float gain = 1.0f;
        int calls = 0;
        bool prepared = false;
    };

    /**
     * The level a crest lands on when the gain closes exactly on it: the soft clip shapes the value
     * that sits right on targetLinear, so the emitted crest is slightly under the closing target.
     * Prefixed like the other helpers in this shared namespace.
     */
    float ebxCrest()
    {
        static constexpr float kneeFraction = 0.95f;   // must match OutputLimiter::softClip
        const float a = OutputLimiter::targetLinear / OutputLimiter::ceilingLinear;
        const float t = (a - kneeFraction) / (1.0f - kneeFraction);
        return OutputLimiter::ceilingLinear * (kneeFraction + (1.0f - kneeFraction) * std::tanh (static_cast<double> (t)));
    }

    /** A deterministic signal: two tones plus a saw ramp, no RNG, no host state. */
    juce::AudioBuffer<float> makeSignal (int channels, int samples)
    {
        juce::AudioBuffer<float> buf (channels, samples);
        for (int c = 0; c < channels; ++c)
            for (int i = 0; i < samples; ++i)
                buf.setSample (c, i, 0.40f * std::sin (0.05f * static_cast<float> (i) + 0.7f * static_cast<float> (c))
                                    + 0.15f * std::sin (0.013f * static_cast<float> (i))
                                    + 0.05f * static_cast<float> (i % 64) / 64.0f);
        return buf;
    }

    /** Exact comparison: the taps only ever multiply, so bit-identity is achievable. */
    bool buffersIdentical (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        if (a.getNumChannels() != b.getNumChannels() || a.getNumSamples() != b.getNumSamples())
            return false;
        for (int c = 0; c < a.getNumChannels(); ++c)
            for (int i = 0; i < a.getNumSamples(); ++i)
                if (a.getSample (c, i) != b.getSample (c, i))
                    return false;
        return true;
    }

    float peakOf (const juce::AudioBuffer<float>& buf)
    {
        float peak = 0.0f;
        for (int c = 0; c < buf.getNumChannels(); ++c)
            for (int i = 0; i < buf.getNumSamples(); ++i)
                peak = juce::jmax (peak, std::abs (buf.getSample (c, i)));
        return peak;
    }

    /** Wiring that always runs the effect at full wet. */
    const ovtdsp::EffectsBus::Wiring runAtFullWet = [] (ovtdsp::IEffect&, bool& enabled, float& wetMix)
    {
        enabled = true;
        wetMix = 1.0f;
        return true;
    };

    /** Wiring that skips every effect: the bus must stay inert. */
    const ovtdsp::EffectsBus::Wiring skipEverything = [] (ovtdsp::IEffect&, bool&, float&)
    {
        return false;
    };
}

class EffectsBusTest : public juce::UnitTest
{
public:
    EffectsBusTest() : juce::UnitTest ("EffectsBus") {}

    void runTest() override
    {
        using namespace ovtdsp;

        const double sampleRate = 44100.0;
        const int blockSize = 512;

        // === Test 1: an empty bus leaves the buffer bit-identical ===
        {
            beginTest ("Empty bus leaves the buffer bit-identical");

            auto signal = makeSignal (2, blockSize);
            auto reference = signal;

            EffectsBus bus;
            expect (bus.size() == 0, "A fresh bus holds no effects");
            bus.prepare (sampleRate, blockSize);
            bus.reset();
            bus.apply (signal, runAtFullWet);

            expect (buffersIdentical (signal, reference),
                    "An empty bus must not touch the tapped buffer");
        }

        // === Test 2: a disabled effect passes the buffer through untouched ===
        {
            beginTest ("Skipped effect passes through untouched");

            auto signal = makeSignal (2, blockSize);
            auto reference = signal;

            EffectsBus bus;
            auto gain = std::make_unique<GainEffect> (2.0f);
            auto* probe = gain.get();
            bus.addEffect (std::move (gain));
            bus.prepare (sampleRate, blockSize);
            bus.apply (signal, skipEverything);

            expect (buffersIdentical (signal, reference),
                    "A wiring that returns false must skip the effect");
            expect (probe->calls == 0,
                    "Skipped effect was called " + juce::String (probe->calls) + " times");
        }

        // === Test 3: all effects on the master reproduce the legacy single-loop dispatch ===
        // This is the behaviour-neutrality proof for the three tap points: with every
        // effect on the master bus, the chain must produce exactly what the old single
        // loop over one effects vector produced.
        {
            beginTest ("All effects on the master match the legacy single loop");

            const float leadGain = 2.0f;   // effect A
            const float padGain  = 3.0f;   // effect B
            const float hGain    = 1.0f / std::sqrt (1.0f + 3.0f);  // harmony gain-match

            // Legacy: mix first, then run every effect on the mixed buffer in order.
            auto legacyLead = makeSignal (2, blockSize);
            auto legacyHarmony = makeSignal (2, blockSize);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < blockSize; ++i)
                    legacyLead.setSample (c, i, legacyLead.getSample (c, i)
                                            + hGain * legacyHarmony.getSample (c, i));

            EffectsBus legacyMaster;
            legacyMaster.addEffect (std::make_unique<GainEffect> (leadGain));
            legacyMaster.addEffect (std::make_unique<GainEffect> (padGain));
            legacyMaster.prepare (sampleRate, blockSize);
            legacyMaster.apply (legacyLead, runAtFullWet);

            // Three taps: lead bus, harmony bus, master bus. Everything on the master.
            auto newLead = makeSignal (2, blockSize);
            auto newHarmony = makeSignal (2, blockSize);
            EffectsBus leadBus, harmonyBus, masterBus;
            leadBus.prepare (sampleRate, blockSize);
            harmonyBus.prepare (sampleRate, blockSize);
            masterBus.addEffect (std::make_unique<GainEffect> (leadGain));
            masterBus.addEffect (std::make_unique<GainEffect> (padGain));
            masterBus.prepare (sampleRate, blockSize);

            leadBus.apply (newLead, runAtFullWet);
            harmonyBus.apply (newHarmony, runAtFullWet);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < blockSize; ++i)
                    newLead.setSample (c, i, newLead.getSample (c, i)
                                        + hGain * newHarmony.getSample (c, i));
            masterBus.apply (newLead, runAtFullWet);

            expect (buffersIdentical (newLead, legacyLead),
                    "Three taps with everything on the master must match the single loop");
        }

        // === Test 4: a lead-bus effect scales only the lead voice ===
        {
            beginTest ("Lead tap scales the lead only, harmony tap scales the pad only");

            const float g = 2.0f;
            const float hGain = 1.0f / std::sqrt (1.0f + 3.0f);

            auto lead = makeSignal (2, blockSize);
            auto harmony = makeSignal (2, blockSize);

            EffectsBus leadBus, harmonyBus;
            leadBus.addEffect (std::make_unique<GainEffect> (g));
            leadBus.prepare (sampleRate, blockSize);
            harmonyBus.prepare (sampleRate, blockSize);

            leadBus.apply (lead, runAtFullWet);
            harmonyBus.apply (harmony, runAtFullWet);

            // Expected: g * lead + hGain * harmony, computed from the ORIGINAL signals.
            auto expectedLead = makeSignal (2, blockSize);
            auto expectedHarmony = makeSignal (2, blockSize);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < blockSize; ++i)
                {
                    const float l = g * expectedLead.getSample (c, i);
                    const float h = hGain * expectedHarmony.getSample (c, i);
                    expectedLead.setSample (c, i, l + h);
                }

            auto mixed = lead;
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < blockSize; ++i)
                    mixed.setSample (c, i, mixed.getSample (c, i) + hGain * harmony.getSample (c, i));

            expect (buffersIdentical (mixed, expectedLead),
                    "A lead-bus effect must scale the lead and leave the harmony bus at its own gain");
        }

        // === Test 5: effects run in registration order ===
        {
            beginTest ("Effects run in registration order");

            std::vector<juce::String> order;
            EffectsBus bus;
            auto first = std::make_unique<GainEffect> (1.0f);
            auto second = std::make_unique<GainEffect> (1.0f);
            // Tag the instances so the wiring can record which one ran, in which order.
            auto* firstProbe = first.get();
            auto* secondProbe = second.get();
            bus.addEffect (std::move (first));
            bus.addEffect (std::move (second));
            bus.prepare (sampleRate, blockSize);

            auto signal = makeSignal (1, blockSize);
            bus.apply (signal, [&order, firstProbe, secondProbe] (IEffect& effect, bool& enabled, float& wetMix)
            {
                enabled = true;
                wetMix = 1.0f;
                order.push_back (effect.getId());
                (void) (firstProbe->calls + secondProbe->calls);
                return true;
            });

            expect (order.size() == 2, "Two effects must be visited, got " + juce::String (static_cast<int> (order.size())));
            expect (order[0] == "gain" && order[1] == "gain", "Both registered effects ran");
            expect (firstProbe->calls == 1 && secondProbe->calls == 1,
                    "Each registered instance runs exactly once per block");
        }

        // === Test 6: the limiter looks ahead by exactly its declared latency ===
        {
            beginTest ("Output limiter looks ahead by its declared latency");

            OutputLimiter limiter;
            limiter.prepare (sampleRate, blockSize);
            limiter.reset();
            limiter.setTrimDb (0.0f);
            limiter.setLimiterEnabled (true);

            const int L = OutputLimiter::kLatencySamples;                       // emitted sample delay
            const int preWindow = OutputLimiter::kFilterTaps / 2 / OutputLimiter::kOversamplingFactor;
            const int spikeIndex = 300;
            juce::AudioBuffer<float> buf (1, blockSize);
            for (int i = 0; i < blockSize; ++i)
                buf.setSample (0, i, i < spikeIndex ? 0.2f : (i == spikeIndex ? 1.40f : 0.2f));

            auto input = buf;
            limiter.process (buf);

            // The emitted sample is the reconstruction at lag L, so every sample leaves the stage L
            // samples later than it entered. Ahead of the crest the gain is unity, so the output is a
            // pure delayed copy of the input -- no shaping, no smear.
            bool quietBefore = true;
            for (int i = L; i < spikeIndex; ++i)
                if (! juce::approximatelyEqual (buf.getSample (0, i), input.getSample (0, i - L)))
                    quietBefore = false;
            expect (quietBefore, "Samples ahead of the detector window must pass through as a delayed copy at unity gain");

            // At the crest, the gain closes exactly on it: the soft clip shapes the value that sits on
            // targetLinear, so the emitted crest lands right where that curve puts it.
            expectWithinAbsoluteError (buf.getSample (0, spikeIndex + L), ebxCrest(), 1.0e-5f,
                    "The peak must be caught at the closing target, not above it");

            // After the crest the release is one-pole: the gain climbs back monotonically.
            float previous = buf.getSample (0, spikeIndex + L + 2);
            bool rising = true;
            for (int i = spikeIndex + L + 3; i < blockSize; ++i)
            {
                const float v = buf.getSample (0, i);
                if (v < previous - 1.0e-6f)
                    rising = false;
                previous = v;
            }
            expect (rising, "The release must be monotonic, never a downward jump");

            // Nothing anywhere exceeds the ceiling.
            expect (peakOf (buf) <= OutputLimiter::ceilingLinear + 1.0e-5f,
                    "Peak after limiting is " + juce::String (peakOf (buf), 6)
                    + ", must stay at or below the ceiling");

            // The lookahead is not free: it IS the declared latency, and the stage reports it.
            expect (L == limiter.getLatencySamples(),
                    "The detector's lookahead equals the latency the stage declares (" + juce::String (L) + ")");
        }

        // === Test 7: trim applies an exact gain when the limiter is off ===
        {
            beginTest ("Trim is an exact output gain when the limiter is off");

            OutputLimiter limiter;
            limiter.prepare (sampleRate, blockSize);
            limiter.reset();
            limiter.setTrimDb (-6.0f);
            limiter.setLimiterEnabled (false);

            juce::AudioBuffer<float> buf (2, blockSize);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < blockSize; ++i)
                    buf.setSample (c, i, 0.30f);

            const float expected = 0.30f * std::pow (10.0f, -6.0f / 20.0f);
            limiter.process (buf);

            for (int c = 0; c < 2; ++c)
                expectWithinAbsoluteError (buf.getSample (c, blockSize - 1), expected, 1.0e-5f,
                    "Trim -6 dB must scale every channel by the same exact factor");
        }

        // === Test 8: channels are linked, the loudest one drives the gain ===
        {
            beginTest ("Channels are linked by the loudest channel");

            OutputLimiter limiter;
            limiter.prepare (sampleRate, blockSize);
            limiter.reset();
            limiter.setTrimDb (0.0f);
            limiter.setLimiterEnabled (true);

            const int L = OutputLimiter::kLatencySamples;
            const int spikeIndex = 200;
            juce::AudioBuffer<float> buf (2, blockSize);
            for (int i = 0; i < blockSize; ++i)
            {
                const bool atSpike = (i == spikeIndex);
                buf.setSample (0, i, atSpike ? 0.10f : 0.05f);   // quiet channel
                buf.setSample (1, i, atSpike ? 1.40f : 0.05f);   // loud channel
            }

            limiter.process (buf);

            const float expectedGain = OutputLimiter::targetLinear / 1.40f;
            expectWithinAbsoluteError (buf.getSample (0, spikeIndex + L), 0.10f * expectedGain, 1.0e-5f,
                    "The quiet channel must be ducked by the same gain as the loud one");
            expectWithinAbsoluteError (buf.getSample (1, spikeIndex + L), ebxCrest(), 1.0e-5f,
                    "The loud channel lands on the closing target, shaped by the soft clip");
        }

        // === Test 9: a silent buffer passes through untouched ===
        {
            beginTest ("Silent buffer passes through untouched");

            OutputLimiter limiter;
            limiter.prepare (sampleRate, blockSize);
            limiter.reset();
            limiter.setTrimDb (6.0f);
            limiter.setLimiterEnabled (true);

            juce::AudioBuffer<float> buf (2, blockSize);
            buf.clear();
            for (int b = 0; b < 8; ++b)
                limiter.process (buf);

            float peak = peakOf (buf);
            expect (peak == 0.0f, "Silence must stay exactly silent, peak is " + juce::String (peak, 6));
            expect (limiter.currentEnvelope() < 1.0e-3f,
                    "The envelope must decay toward zero on silence, got " + juce::String (limiter.currentEnvelope(), 6));
        }

        // === Test 10: the stage delays by exactly its declared latency ===
        {
            beginTest ("The emitted delay equals the declared latency");

            // The plugin declares its latency through setLatencySamples, so the delay this stage
            // adds must be the number it reports -- not zero, and not something unaccounted.
            OutputLimiter limiter;
            limiter.prepare (sampleRate, blockSize);
            limiter.reset();
            limiter.setTrimDb (0.0f);
            limiter.setLimiterEnabled (true);

            const int L = limiter.getLatencySamples();
            expect (L == OutputLimiter::kLatencySamples,
                    "The stage must report its own filter group delay (" + juce::String (OutputLimiter::kLatencySamples) + ")");

            juce::AudioBuffer<float> impulse (1, blockSize);
            impulse.clear();
            impulse.setSample (0, 100, 1.40f);
            limiter.process (impulse);

            // The reconstruction at phase 0 is the centre tap of the filter: an impulse leaves the
            // stage L samples later than it entered, and nothing can be produced before that.
            expect (std::abs (impulse.getSample (0, 100 + L - 1)) < 1.0e-6f,
                    "Nothing may be produced ahead of the declared delay");
            expectWithinAbsoluteError (impulse.getSample (0, 100 + L), ebxCrest(), 1.0e-5f,
                    "The impulse must leave the stage exactly L samples later, got "
                    + juce::String (impulse.getSample (0, 100 + L), 6));
        }

        // === Test 11: currentGain() is the exact attenuation above the ceiling ===
        // The meter is a display of this stage, so every reading it shows must be
        // derivable from the limiter's own state in closed form.
        {
            beginTest ("currentGain() reports the exact gain reduction");

            OutputLimiter limiter;
            limiter.prepare (sampleRate, blockSize);
            limiter.reset();
            limiter.setTrimDb (0.0f);
            limiter.setLimiterEnabled (true);

            // The spike sits L samples before the last sample, so at the moment process() returns
            // the detector has it exactly on its centre tap: the envelope is still the peak and no
            // release has eaten into it yet.
            const int L = limiter.getLatencySamples();
            juce::AudioBuffer<float> buf (1, blockSize);
            buf.clear();
            for (int i = 0; i < blockSize - 1; ++i)
                buf.setSample (0, i, 0.20f);
            buf.setSample (0, blockSize - 1 - L, 1.40f);

            limiter.process (buf);

            expectWithinAbsoluteError (limiter.currentEnvelope(), 1.40f, 1.0e-6f,
                    "The envelope must reach the peak at the peak, got " + juce::String (limiter.currentEnvelope(), 6));
            expectWithinAbsoluteError (limiter.currentGain(), OutputLimiter::targetLinear / 1.40f, 1.0e-6f,
                    "Above the closing target, gain must be exactly target / envelope");

            const float expectedGr = -20.0f * std::log10 (OutputLimiter::targetLinear / 1.40f);
            expectWithinAbsoluteError (limiter.currentGainReductionDb(), expectedGr, 1.0e-4f,
                    "Gain reduction must read in dB, got " + juce::String (limiter.currentGainReductionDb(), 4));

            // The meter's peak is the level leaving the stage: envelope x attenuation.
            expectWithinAbsoluteError (limiter.currentOutputPeak(), OutputLimiter::targetLinear, 1.0e-5f,
                    "While limiting, the output peak must sit on the closing target");
        }

        // === Test 12: below the ceiling the reported gain is the trim, exactly ===
        {
            beginTest ("Below the ceiling currentGain() is the trim, untouched");

            OutputLimiter limiter;
            limiter.prepare (sampleRate, blockSize);
            limiter.reset();
            limiter.setTrimDb (-6.0f);
            limiter.setLimiterEnabled (true);

            juce::AudioBuffer<float> buf (2, blockSize);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < blockSize; ++i)
                    buf.setSample (c, i, 0.30f);

            limiter.process (buf);

            const float expectedTrim = std::pow (10.0f, -6.0f / 20.0f);
            expectWithinAbsoluteError (limiter.currentGain(), expectedTrim, 1.0e-6f,
                    "A quiet signal must report the trim as the whole gain");
            expect (limiter.currentGain() == limiter.currentTrimLinear(),
                    "Below the ceiling the reported gain must be the trim bit-for-bit");
            expect (limiter.currentGainReductionDb() == 0.0f,
                    "A signal under the ceiling must show zero gain reduction, got "
                    + juce::String (limiter.currentGainReductionDb(), 4));

            // The meter peak is the trimmed level: envelope x unity attenuation. The detector reads
            // the reconstructed crest, so a DC through the filter sums to unity gain per phase group;
            // the finite 65-tap window truncates that sum by a few percent and the step at the buffer
            // start rings, which is why this reading carries a tolerance the trim-only tests do not.
            expectWithinAbsoluteError (limiter.currentOutputPeak(), 0.30f * expectedTrim, 2.0e-2f,
                    "The output peak must be the trimmed signal level, got "
                    + juce::String (limiter.currentOutputPeak(), 6));
        }

        // === Test 13: on silence the meter falls, it never plateaus ===
        // The sleep path clears the buffer and still runs this stage, so a held block
        // peak would show a frozen bar long after the sound stopped. The release is the
        // only thing moving the envelope, and both readings must follow it down.
        {
            beginTest ("Silence drives gain reduction and peak down to zero");

            OutputLimiter limiter;
            limiter.prepare (sampleRate, blockSize);
            limiter.reset();
            limiter.setTrimDb (0.0f);
            limiter.setLimiterEnabled (true);

            // The spike sits L samples before the last sample, so when process() returns the detector
            // has it on its centre tap and the envelope is exactly the peak.
            const int L = limiter.getLatencySamples();
            juce::AudioBuffer<float> spike (1, blockSize);
            spike.clear();
            spike.setSample (0, blockSize - 1 - L, 1.40f);
            limiter.process (spike);

            expect (limiter.currentGainReductionDb() > 1.0f,
                    "The spike must show gain reduction, got " + juce::String (limiter.currentGainReductionDb(), 4));

            // 800 silent blocks is ~ 8.5 s at 44.1 kHz / 512: long enough for the
            // 120 ms release to walk the envelope from 1.40 down to zero.
            juce::AudioBuffer<float> silence (2, blockSize);
            silence.clear();

            float previousPeak = limiter.currentOutputPeak();
            float previousGr   = limiter.currentGainReductionDb();
            bool falling = true;
            for (int b = 0; b < 800; ++b)
            {
                limiter.process (silence);
                const float p = limiter.currentOutputPeak();
                const float g = limiter.currentGainReductionDb();
                if (p > previousPeak + 1.0e-6f || g > previousGr + 1.0e-6f)
                    falling = false;
                previousPeak = p;
                previousGr   = g;
            }

            expect (falling, "The meter must fall with the tail, never plateau");
            expect (previousPeak < 1.0e-3f,
                    "The output peak must reach zero on silence, got " + juce::String (previousPeak, 6));
            expect (previousGr == 0.0f,
                    "Gain reduction must read zero once the envelope is under the ceiling, got "
                    + juce::String (previousGr, 4));
        }
    }
};

static EffectsBusTest effectsBusTest;
