#pragma once
// ReverbEffectTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/ReverbEffect.h"
#include "../../Source/dsp/OnePoleLowCut.h"
#include <cmath>

namespace
{
    /** A single-sample impulse: the room's response to it IS the room's impulse
        response, so a tail measured from an impulse is a known shape, not a guess. */
    juce::AudioBuffer<float> roomImpulse (int channels, int samples, float amplitude)
    {
        juce::AudioBuffer<float> buf (channels, samples);
        buf.clear();
        for (int c = 0; c < channels; ++c)
            buf.setSample (c, 0, amplitude);
        return buf;
    }

    /** Deterministic signal, phase-continuous across blocks: no RNG, no host state. */
    juce::AudioBuffer<float> roomSignal (int channels, int samples, int blockIndex)
    {
        juce::AudioBuffer<float> buf (channels, samples);
        for (int c = 0; c < channels; ++c)
            for (int i = 0; i < samples; ++i)
            {
                const float n = static_cast<float> (blockIndex * samples + i);
                buf.setSample (c, i, 0.40f * std::sin (0.05f * n + 0.7f * static_cast<float> (c))
                                    + 0.15f * std::sin (0.013f * n)
                                    + 0.05f * std::sin (0.31f * n));
            }
        return buf;
    }

    float roomPeakOf (const juce::AudioBuffer<float>& buf, int channel = -1)
    {
        float peak = 0.0f;
        const int first = channel < 0 ? 0 : channel;
        const int last  = channel < 0 ? buf.getNumChannels() : channel + 1;
        for (int c = first; c < last; ++c)
            for (int i = 0; i < buf.getNumSamples(); ++i)
                peak = juce::jmax (peak, std::abs (buf.getSample (c, i)));
        return peak;
    }

    /** Bit-identity: the dry path is never written by a send, so exact equality is the
        assertion a send must pass, with no tolerance. */
    bool sendBuffersIdentical (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        if (a.getNumChannels() != b.getNumChannels() || a.getNumSamples() != b.getNumSamples())
            return false;
        for (int c = 0; c < a.getNumChannels(); ++c)
            for (int i = 0; i < a.getNumSamples(); ++i)
                if (a.getSample (c, i) != b.getSample (c, i))
                    return false;
        return true;
    }

    /** The ducking expression the processor uses: gate open -> 1.0, depth 0 -> always
        1.0, depth 1 with a closed gate -> 0. */
    float duckGainFor (float depth, float gateGain)
    {
        return 1.0f - depth * (1.0f - gateGain);
    }

    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;
    // 25 ms ramps at 48 kHz, the length ReverbEffect::prepare() picks.
    constexpr int kRampSamples = 1200;
    // Blocks needed for an asymptotic linear ramp to settle inside float precision:
    // the residual is exp(-samples/kRampSamples), so 60 blocks leave ~3e-6.
    constexpr int kWarmUpBlocks = 60;
}

class SharedReverbRoomTest : public juce::UnitTest
{
public:
    SharedReverbRoomTest() : juce::UnitTest ("Shared Reverb Room") {}

    void runTest() override
    {
        // === Send is parallel, not an insert ===

        beginTest ("A send at 0 leaves the dry voice bit-identical");
        {
            // A send at 0 must leave the tapped voice exactly as it found it. The dry
            // path is the contract: a send adds to a room, it never steals from the voice.
            ovtdsp::ReverbEffect room;
            room.prepare (kSr, kBlock);

            juce::AudioBuffer<float> lead = roomSignal (2, kBlock, 0);
            const juce::AudioBuffer<float> reference = lead;

            room.processLeadSend (lead, 0.0f);
            room.processPadSend (lead, 0.0f);

            expect (sendBuffersIdentical (lead, reference),
                    "A send at 0 changed the dry path");
        }

        beginTest ("A return on a silent send line leaves the master untouched");
        {
            // The skip guard means the room is not even run when nothing is being sent
            // and the tail has decayed, so nothing is added and nothing is subtracted.
            ovtdsp::ReverbEffect room;
            room.prepare (kSr, kBlock);

            juce::AudioBuffer<float> master = roomSignal (2, kBlock, 0);
            const juce::AudioBuffer<float> masterReference = master;

            room.processReturn (master, 1.0f);

            expect (sendBuffersIdentical (master, masterReference),
                    "A return on a silent send line changed the master");
        }

        beginTest ("A send at unity does not attenuate the dry voice");
        {
            // This is the exact behaviour the card set out to correct: the old in-place
            // wet sum multiplied the dry by (1 - mix), so opening the room quietly took
            // the voice away.
            ovtdsp::ReverbEffect room;
            room.prepare (kSr, kBlock);

            juce::AudioBuffer<float> lead = roomSignal (2, kBlock, 0);
            const juce::AudioBuffer<float> reference = lead;

            room.processLeadSend (lead, 1.0f);

            expect (sendBuffersIdentical (lead, reference),
                    "A send at unity changed the dry path");
        }

        beginTest ("process() at mix 0 is a no-op, at full wet it ADDS the room");
        {
            // The insert path: wetMix 0 is a no-op, wetMix 1 adds the room, and the dry
            // term is never scaled.
            ovtdsp::ReverbEffect insert;
            insert.prepare (kSr, kBlock);

            juce::AudioBuffer<float> dry = roomSignal (2, kBlock, 0);
            const juce::AudioBuffer<float> dryReference = dry;
            insert.process (dry, false, 0.0f);

            expect (sendBuffersIdentical (dry, dryReference),
                    "process() at mix 0 changed the input");

            juce::AudioBuffer<float> wet = roomSignal (2, kBlock, 0);
            insert.process (wet, true, 1.0f);

            bool anyAdded = false;
            for (int c = 0; c < wet.getNumChannels(); ++c)
                for (int i = 0; i < wet.getNumSamples(); ++i)
                    if (wet.getSample (c, i) != dryReference.getSample (c, i))
                        anyAdded = true;

            expect (anyAdded, "process() at full wet added nothing");
        }

        // === Known tail ===

        beginTest ("An impulse at unity send decays to exact zero");
        {
            // Feed an impulse at unity send, then stop sending: the room's response is a
            // decaying tail that must fall to exact zero, because the skip guard stops
            // running the room once the tail is inaudible. A tail that never reaches 0
            // means the send line is accumulating, which is the failure this test exists
            // to catch.
            ovtdsp::ReverbEffect room;
            room.prepare (kSr, kBlock);

            juce::AudioBuffer<float> impulse = roomImpulse (2, kBlock, 0.8f);
            room.processLeadSend (impulse, 1.0f);

            juce::AudioBuffer<float> master (2, kBlock);
            master.clear();
            room.processReturn (master, 1.0f);

            const float firstPeak = roomPeakOf (master);
            expectGreaterThan (firstPeak, 0.0f);

            float previous = firstPeak;
            float lastPeak = firstPeak;
            bool reachedZero = false;

            for (int k = 0; k < 40; ++k)
            {
                juce::AudioBuffer<float> m (2, kBlock);
                m.clear();
                room.processReturn (m, 1.0f);

                const float p = roomPeakOf (m);
                // Modal beating can lift a block slightly, so the bound is generous; a
                // tail that grows by more than 5 % per block is not decaying.
                expectLessThan (p, previous * 1.05f + 1.0e-6f);
                previous = p;
                lastPeak = p;
                if (p == 0.0f)
                    reachedZero = true;
            }

            expectLessThan (lastPeak, firstPeak * 0.01f);
            expect (reachedZero, "the tail never reached exact zero: the room is still being run");
        }

        beginTest ("A unity pad send equals the same room as an insert at full wet");
        {
            // The closed-form expectation for a send at unity: the room sees exactly the
            // signal that was sent, so the return must equal what the same room adds when
            // run as an insert at full wet on the same blocks. The pad send is used here
            // because it has no pre-send filter, so the two paths see identical input.
            ovtdsp::ReverbEffect sendPath;
            ovtdsp::ReverbEffect insertPath;
            sendPath.prepare (kSr, kBlock);
            insertPath.prepare (kSr, kBlock);

            // The room's comb feedback is 0.896 per round trip on delay lines of
            // 1200-1760 samples at 48 kHz, so the send ramp that runs during the
            // first ~1200 samples keeps echoing in the comb memory for seconds.
            // A short warm-up therefore compares two rooms that are still hearing
            // different histories; 600 blocks (3.2 s) decay that history far below
            // the tolerance, which is what makes this comparison mean something.
            const int warmUp = 600;

            for (int k = 0; k < warmUp; ++k)
            {
                juce::AudioBuffer<float> src = roomSignal (2, kBlock, k);
                sendPath.processPadSend (src, 1.0f);

                juce::AudioBuffer<float> m (2, kBlock);
                m.clear();
                sendPath.processReturn (m, 1.0f);

                insertPath.process (src, true, 1.0f);
            }

            juce::AudioBuffer<float> src = roomSignal (2, kBlock, warmUp);
            const juce::AudioBuffer<float> dry = src;

            sendPath.processPadSend (src, 1.0f);
            juce::AudioBuffer<float> m (2, kBlock);
            m.clear();
            sendPath.processReturn (m, 1.0f);

            insertPath.process (src, true, 1.0f);

            float maxDiff = 0.0f;
            float wetPeak = 0.0f;
            for (int c = 0; c < src.getNumChannels(); ++c)
                for (int i = 0; i < src.getNumSamples(); ++i)
                {
                    const float insertWet = src.getSample (c, i) - dry.getSample (c, i);
                    maxDiff = juce::jmax (maxDiff, std::abs (insertWet - m.getSample (c, i)));
                    wetPeak = juce::jmax (wetPeak, std::abs (insertWet));
                }

            expectGreaterThan (wetPeak, 0.0f);
            // Both paths ramp their level from 0 (the send ramp is linear over 25 ms, the
            // insert master gain is exponential with the same time constant), so the two
            // rooms heard slightly different inputs for the first few blocks. Measured
            // residual after the warm-up is ~2e-5 against a wet peak of ~0.51, i.e. about
            // 4e-5 relative; the 1e-3 tolerance keeps that history in the comparison
            // honest without pretending the two paths are bit-identical.
            expectWithinAbsoluteError (maxDiff, 0.0f, wetPeak * 1.0e-3f);
        }

        // === reset() flushes the line ===

        beginTest ("reset() flushes the send line and the room without reallocating");
        {
            ovtdsp::ReverbEffect room;
            room.prepare (kSr, kBlock);

            const int preparedSamples = room.getScratchSamples();
            expectEquals (preparedSamples, kBlock);

            juce::AudioBuffer<float> impulse = roomImpulse (2, kBlock, 1.0f);
            room.processLeadSend (impulse, 1.0f);

            juce::AudioBuffer<float> m (2, kBlock);
            m.clear();
            room.processReturn (m, 1.0f);
            expectGreaterThan (roomPeakOf (m), 0.0f);   // the tail is ringing

            room.reset();

            juce::AudioBuffer<float> after (2, kBlock);
            after.clear();
            room.processReturn (after, 1.0f);

            // A transport restart must not carry a room from the previous take: both the
            // send line and the room state have to be empty, so the return is exactly zero.
            expectEquals (roomPeakOf (after), 0.0f);
            // Flushing must not reallocate: the capacity prepared for this block size stays.
            expectEquals (room.getScratchSamples(), preparedSamples);
        }

        // === Ducking follows the noise gate ===

        beginTest ("Ducking corners: depth 0, gate open, and a closed gate at full depth");
        {
            // The three corners of the expression the processor uses.
            expectEquals (duckGainFor (0.0f, 0.0f), 1.0f);   // depth 0: room never ducks
            expectEquals (duckGainFor (1.0f, 1.0f), 1.0f);   // gate open: nothing to duck
            expectEquals (duckGainFor (1.0f, 0.0f), 0.0f);   // depth 1 + closed gate: full duck
        }

        beginTest ("A fully ducked return is silent, an unducked one is not");
        {
            ovtdsp::ReverbEffect open;
            ovtdsp::ReverbEffect ducked;
            open.prepare (kSr, kBlock);
            ducked.prepare (kSr, kBlock);

            for (int k = 0; k < kWarmUpBlocks; ++k)
            {
                juce::AudioBuffer<float> src = roomSignal (2, kBlock, k);
                open.processPadSend (src, 1.0f);
                ducked.processPadSend (src, 1.0f);

                juce::AudioBuffer<float> mo (2, kBlock);
                juce::AudioBuffer<float> md (2, kBlock);
                mo.clear();
                md.clear();
                open.processReturn (mo, duckGainFor (0.5f, 1.0f));    // gate open -> 1.0
                ducked.processReturn (md, duckGainFor (1.0f, 0.0f));  // closed + depth 1 -> 0
            }

            juce::AudioBuffer<float> src = roomSignal (2, kBlock, kWarmUpBlocks);
            juce::AudioBuffer<float> mo (2, kBlock);
            juce::AudioBuffer<float> md (2, kBlock);
            mo.clear();
            md.clear();

            open.processPadSend (src, 1.0f);
            ducked.processPadSend (src, 1.0f);
            open.processReturn (mo, 1.0f);
            ducked.processReturn (md, 0.0f);

            const float openPeak = roomPeakOf (mo);
            expectGreaterThan (openPeak, 0.0f);
            expectWithinAbsoluteError (roomPeakOf (md), 0.0f, openPeak * 1.0e-3f);
        }

        // === Mono bus degradation ===

        beginTest ("A mono source drives both sides of the stereo room");
        {
            // A mono source feeding a stereo room must drive BOTH sides: the send stays
            // centred instead of vanishing from one channel.
            ovtdsp::ReverbEffect room;
            room.prepare (kSr, kBlock);

            juce::AudioBuffer<float> mono = roomImpulse (1, kBlock, 1.0f);

            room.processLeadSend (mono, 1.0f);

            juce::AudioBuffer<float> stereo (2, kBlock);
            stereo.clear();
            room.processReturn (stereo, 1.0f);

            expectGreaterThan (roomPeakOf (stereo, 0), 0.0f);
            expectGreaterThan (roomPeakOf (stereo, 1), 0.0f);
        }

        beginTest ("A mono master receives the down-mixed return");
        {
            // A mono master receives the down-mixed return, so the level matches what a
            // stereo listener hears rather than one side of the room.
            ovtdsp::ReverbEffect monoRoom;
            monoRoom.prepare (kSr, kBlock);

            juce::AudioBuffer<float> mono = roomImpulse (1, kBlock, 1.0f);
            monoRoom.processPadSend (mono, 1.0f);

            juce::AudioBuffer<float> monoMaster (1, kBlock);
            monoMaster.clear();
            monoRoom.processReturn (monoMaster, 1.0f);

            expectGreaterThan (roomPeakOf (monoMaster), 0.0f);
        }

        // === No allocation at block time ===

        beginTest ("The scratch capacity never grows at block time");
        {
            // What this CAN prove in JUCE 8. juce_core exposes no malloc counter, and
            // HeapBlock goes through std::malloc, so a temporary juce::AudioBuffer created
            // inside process() is invisible to any JUCE-side probe. Replacing global
            // operator new at program scope was tried here and ABORTED the whole test
            // binary, so it is not the tool for this job. What stays provable is that the
            // scratch never grows: every buffer the send / return path touches is sized in
            // prepare(), so a block LONGER than the prepared maximum must leave the
            // capacity exactly where it was. A growing capacity IS an allocation on the
            // audio thread, which is the failure this card cares about; these assertions
            // catch that and nothing more, and the header comment says the same.
            ovtdsp::ReverbEffect room;
            room.prepare (kSr, kBlock);

            const int preparedSamples = room.getScratchSamples();
            expectEquals (preparedSamples, kBlock);

            juce::AudioBuffer<float> src (2, kBlock);
            juce::AudioBuffer<float> master (2, kBlock);

            for (int k = 0; k < kWarmUpBlocks + 40; ++k)
            {
                src = roomSignal (2, kBlock, k);
                master = src;
                room.process (master, true, 0.5f);
                room.processLeadSend (src, 0.3f);
                room.processPadSend (src, 0.2f);
                room.processReturn (master, 0.7f);

                expectEquals (room.getScratchSamples(), preparedSamples);
            }

            // A block longer than the prepared maximum: the capacity must not move, and
            // the room must still process the samples it was sized for.
            juce::AudioBuffer<float> big (2, kBlock * 4);
            big.clear();
            room.process (big, true, 0.5f);
            room.processLeadSend (big, 0.3f);
            room.processReturn (big, 1.0f);

            expectEquals (room.getScratchSamples(), preparedSamples);
        }

        // === Pre-send low-cut ===

        beginTest ("The pre-send low-cut bypasses at cutoff 0 and blocks DC at 150 Hz");
        {
            // Cutoff 0 is the documented bypass: the send passes through bit-identical,
            // which is how a "no pre-send filter" state is expressed.
            ovtdsp::OnePoleLowCut off;
            off.prepare (kSr, 0.0f);
            expect (off.isBypassed(), "cutoff 0 did not bypass the filter");

            bool passthrough = true;
            for (int i = 0; i < kBlock; ++i)
                if (off.process (0.4f * std::sin (0.05f * static_cast<float> (i)))
                    != 0.4f * std::sin (0.05f * static_cast<float> (i)))
                    passthrough = false;
            expect (passthrough, "a bypassed filter changed the signal");

            // DC is blocked by construction: a constant input drives the pole to x, so
            // the output falls to zero. This is the breath rumble the filter exists for.
            ovtdsp::OnePoleLowCut cut;
            cut.prepare (kSr, 150.0f);
            expect (! cut.isBypassed(), "a positive cutoff bypassed the filter");

            const float first = cut.process (1.0f);
            float last = 0.0f;
            for (int i = 1; i < kBlock * 20; ++i)
                last = cut.process (1.0f);
            expectGreaterThan (first, 0.0f);
            expectWithinAbsoluteError (last, 0.0f, 1.0e-3f);

            // The pole coefficient is the -3 dB point: at 150 Hz, alpha = 1 - exp(-2*pi*150/48000).
            const float expectedCoeff = 1.0f - static_cast<float> (std::exp (-2.0 * 3.14159265358979323846 * 150.0 / 48000.0));
            expectWithinAbsoluteError (cut.getCoefficient(), expectedCoeff, 1.0e-5f);
        }

        beginTest ("A tone above the cutoff survives the pre-send low-cut");
        {
            // A tone well above the cutoff must survive: the whole point of a pre-send
            // cut is to remove lows without taking the body of the singing away.
            ovtdsp::OnePoleLowCut toneCut;
            toneCut.prepare (kSr, 150.0f);

            float inPeak = 0.0f, outPeak = 0.0f;
            for (int i = 0; i < kBlock * 20; ++i)
            {
                const float x = std::sin (2.0f * 3.14159265f * 1000.0f / static_cast<float> (kSr) * static_cast<float> (i));
                inPeak  = juce::jmax (inPeak,  std::abs (x));
                outPeak = juce::jmax (outPeak, std::abs (toneCut.process (x)));
            }

            expectGreaterThan (outPeak, inPeak * 0.95f);
        }
    }
};

static SharedReverbRoomTest sharedReverbRoomTest;
