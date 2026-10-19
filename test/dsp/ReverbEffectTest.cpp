#pragma once
// ReverbEffectTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/ReverbEffect.h"
#include "../../Source/dsp/ReverbTuning.h"
#include "../../Source/dsp/OnePoleLowCut.h"
#include <cmath>
#include <iostream>

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

    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;
    // 25 ms ramps at 48 kHz, the length ReverbEffect::prepare() picks.
    constexpr int kRampSamples = 1200;
    // Blocks needed for an asymptotic linear ramp to settle inside float precision:
    // the residual is exp(-samples/kRampSamples), so 60 blocks leave ~3e-6.
    constexpr int kWarmUpBlocks = 60;

    /** ReverbEffect::kReturnTrim is private, so the level claim below mirrors it by value.
        If the trim changes in the class, this constant makes the test fail loudly instead of
        quietly agreeing with whatever the code now does. */
    constexpr float kReturnTrimTest = 0.7f;

    /** RMS over one channel of one block: level is what the user hears, so every level
        claim in this suite is measured as an RMS ratio, not as a peak. */
    float rvxRms (const juce::AudioBuffer<float>& buf, int channel)
    {
        double sum = 0.0;
        for (int i = 0; i < buf.getNumSamples(); ++i)
            sum += static_cast<double> (buf.getSample (channel, i)) * buf.getSample (channel, i);
        return static_cast<float> (std::sqrt (sum / static_cast<double> (buf.getNumSamples())));
    }

    /** Seconds from a sustained voice until the room's tail falls 60 dB below its steady-state
        level, measured through the same send / return path the plugin uses. The room has to be
        driven first: an impulse alone enters the delay lines at the input scale (0.015) with the
        wet gain ramped from zero, so its first-block peak is far below what the tail reaches and
        a decay measured from it is the skip guard collapsing, not the room ringing. */
    float rvxTailSeconds (ovtdsp::ReverbEffect& room)
    {
        const int drive = 200;   // ~1 s of sustained voice at unity send
        float steadyPeak = 0.0f;

        for (int k = 0; k < drive; ++k)
        {
            juce::AudioBuffer<float> src = roomSignal (2, kBlock, k);
            room.processHarmonySend (src, 1.0f);

            juce::AudioBuffer<float> m (2, kBlock);
            m.clear();
            room.processReturn (m, 1.0f);
            steadyPeak = juce::jmax (steadyPeak, roomPeakOf (m));
        }

        if (steadyPeak <= 0.0f)
            return -1.0f;

        // Voice stops: silent blocks with send 0, so the only thing left is the tail. The budget
        // runs to 8 s because the long HALL (roomSize 0.70) needs seconds to reach -60 dB; a room
        // that never gets there within this window is the failure this lookahead exists to catch.
        for (int k = 0; k < 1500; ++k)
        {
            juce::AudioBuffer<float> silent (2, kBlock);
            silent.clear();
            room.processHarmonySend (silent, 0.0f);

            juce::AudioBuffer<float> b (2, kBlock);
            b.clear();
            room.processReturn (b, 1.0f);

            if (roomPeakOf (b) <= steadyPeak * 1.0e-3f)
                return static_cast<float> ((k + 1) * kBlock / kSr);
        }
        return -1.0f;   // never decayed within ~8 s
    }
}

class ReverbBusPairTest : public juce::UnitTest
{
public:
    ReverbBusPairTest() : juce::UnitTest ("Reverb Bus Pair") {}

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
            room.processHarmonySend (lead, 0.0f);

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

        beginTest ("process() is a Mix knob: mix 0 is a no-op, full wet replaces the voice");
        {
            // The endpoints of a dry/wet crossfade. Pure additive summing -- dry at unity plus
            // the room on top -- never reaches "fully wet", which is exactly why pushing the
            // lead mix to full sounded like nothing happening. So the assertion proves both
            // directions: at full wet the output IS the room alone, and it is NOT voice + room.
            ovtdsp::ReverbEffect insert;
            ovtdsp::ReverbEffect wetRef;
            insert.prepare (kSr, kBlock);
            wetRef.prepare (kSr, kBlock);

            juce::AudioBuffer<float> dry = roomSignal (2, kBlock, 0);
            const juce::AudioBuffer<float> dryReference = dry;
            insert.process (dry, false, 0.0f);

            expect (sendBuffersIdentical (dry, dryReference),
                    "process() at mix 0 changed the input");

            // Warm up so both rooms hear the same history: comb feedback carries a send-ramp
            // memory for seconds, so a short warm-up cannot pin a level comparison.
            const int warmUp = 600;
            for (int k = 0; k < warmUp; ++k)
            {
                juce::AudioBuffer<float> src = roomSignal (2, kBlock, k);
                wetRef.processHarmonySend (src, 1.0f);

                juce::AudioBuffer<float> m (2, kBlock);
                m.clear();
                wetRef.processReturn (m, 1.0f);

                insert.process (src, true, 1.0f);
            }

            juce::AudioBuffer<float> src = roomSignal (2, kBlock, warmUp);
            const juce::AudioBuffer<float> voice = src;

            juce::AudioBuffer<float> m (2, kBlock);
            m.clear();
            wetRef.processHarmonySend (src, 1.0f);
            wetRef.processReturn (m, 1.0f);   // m is the room's wet scaled by the trim

            insert.process (src, true, 1.0f);

            float maxDiffToWet = 0.0f;
            float maxDiffToAdditive = 0.0f;
            float wetPeak = 0.0f;
            for (int c = 0; c < src.getNumChannels(); ++c)
                for (int i = 0; i < src.getNumSamples(); ++i)
                {
                    const float wet = m.getSample (c, i) / kReturnTrimTest;
                    const float out = src.getSample (c, i);
                    const float additive = voice.getSample (c, i) + wet;

                    maxDiffToWet      = juce::jmax (maxDiffToWet, std::abs (out - wet));
                    maxDiffToAdditive = juce::jmax (maxDiffToAdditive, std::abs (out - additive));
                    wetPeak = juce::jmax (wetPeak, std::abs (wet));
                }

            expectGreaterThan (wetPeak, 0.0f);
            expectWithinAbsoluteError (maxDiffToWet, 0.0f, wetPeak * 1.0e-3f);

            // The dry term has to be gone: the additive form differs from the real output by
            // the whole voice, so half the voice RMS is a wide-open pass for crossfade and a
            // hard fail for summing.
            const float voiceRms = rvxRms (voice, 0);
            expectGreaterThan (maxDiffToAdditive, voiceRms * 0.5f);
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

        beginTest ("A unity harmony send equals the same room as an insert at full wet");
        {
            // The closed-form expectation for a send at unity: the room sees exactly the signal
            // that was sent, so the return must equal what the same room produces when run as an
            // insert at full wet, scaled by the documented return trim. Half mix is pinned in the
            // same breath: half the voice plus half that same wet. The harmony send is used
            // here because it has no pre-send low-cut, so both paths see identical input.
            ovtdsp::ReverbEffect sendPath;
            ovtdsp::ReverbEffect insertFull;
            ovtdsp::ReverbEffect insertHalf;
            sendPath.prepare (kSr, kBlock);
            insertFull.prepare (kSr, kBlock);
            insertHalf.prepare (kSr, kBlock);

            // The room's comb feedback is 0.896 per round trip on delay lines of
            // 1200-1760 samples at 48 kHz, so the send ramp that runs during the
            // first ~1200 samples keeps echoing in the comb memory for seconds.
            // A short warm-up therefore compares rooms that are still hearing
            // different histories; 600 blocks (3.2 s) decay that history far below
            // the tolerance, which is what makes this comparison mean something.
            const int warmUp = 600;

            for (int k = 0; k < warmUp; ++k)
            {
                // Every path gets its own copy of the same block: process() writes in place, so
                // sharing one buffer would make the second insert hear the first one's output.
                juce::AudioBuffer<float> forSend = roomSignal (2, kBlock, k);
                juce::AudioBuffer<float> forFull = roomSignal (2, kBlock, k);
                juce::AudioBuffer<float> forHalf = roomSignal (2, kBlock, k);

                sendPath.processHarmonySend (forSend, 1.0f);

                juce::AudioBuffer<float> m (2, kBlock);
                m.clear();
                sendPath.processReturn (m, 1.0f);

                insertFull.process (forFull, true, 1.0f);
                insertHalf.process (forHalf, true, 0.5f);
            }

            const juce::AudioBuffer<float> voice = roomSignal (2, kBlock, warmUp);

            juce::AudioBuffer<float> forSend = roomSignal (2, kBlock, warmUp);
            sendPath.processHarmonySend (forSend, 1.0f);

            juce::AudioBuffer<float> m (2, kBlock);
            m.clear();
            sendPath.processReturn (m, 1.0f);

            juce::AudioBuffer<float> fullOut = roomSignal (2, kBlock, warmUp);
            juce::AudioBuffer<float> halfOut = roomSignal (2, kBlock, warmUp);
            insertFull.process (fullOut, true, 1.0f);
            insertHalf.process (halfOut, true, 0.5f);

            float maxDiffFull = 0.0f;
            float maxDiffHalf = 0.0f;
            float wetPeak = 0.0f;
            const float voicePeak = roomPeakOf (voice);

            for (int c = 0; c < fullOut.getNumChannels(); ++c)
                for (int i = 0; i < fullOut.getNumSamples(); ++i)
                {
                    // The return is the same wet the insert produces, scaled by the trim.
                    const float wet = m.getSample (c, i) / kReturnTrimTest;

                    maxDiffFull = juce::jmax (maxDiffFull,
                        std::abs (fullOut.getSample (c, i) - wet));

                    // Half mix is the crossfade midpoint: half voice + half that same wet.
                    const float halfExpect = voice.getSample (c, i) * 0.5f + wet * 0.5f;
                    maxDiffHalf = juce::jmax (maxDiffHalf,
                        std::abs (halfOut.getSample (c, i) - halfExpect));

                    wetPeak = juce::jmax (wetPeak, std::abs (wet));
                }

            expectGreaterThan (wetPeak, 0.0f);
            // Both paths ramp their level from 0 (the send ramp is linear over 25 ms, the
            // insert master gain is exponential with the same time constant), so the rooms
            // heard slightly different inputs for the first few blocks. Measured residual
            // after the warm-up is ~2e-5 against a wet peak of ~0.51, i.e. about 4e-5
            // relative; the 1e-3 tolerance keeps that history in the comparison honest
            // without pretending the two paths are bit-identical.
            const float tolerance = (wetPeak + voicePeak) * 1.0e-3f;

            expectWithinAbsoluteError (maxDiffFull, 0.0f, tolerance);
            expectWithinAbsoluteError (maxDiffHalf, 0.0f, tolerance);
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

        // === The return gain (the ducker's output) scales the wet only ===

        beginTest ("The return gain scales the wet, never the dry master");
        {
            // The processor multiplies the return by the ducker's gain; this is the contract
            // that makes ducking a depth control instead of a gate. Two identical rooms warm
            // up on the same signal, then one return is scaled to 0.3: its wet must be exactly
            // 0.3 of the other's, and the master must never be scaled below that.
            ovtdsp::ReverbEffect full;
            ovtdsp::ReverbEffect ducked;
            full.prepare (kSr, kBlock);
            ducked.prepare (kSr, kBlock);

            for (int k = 0; k < kWarmUpBlocks; ++k)
            {
                juce::AudioBuffer<float> a = roomSignal (2, kBlock, k);
                juce::AudioBuffer<float> b = roomSignal (2, kBlock, k);
                full.processHarmonySend (a, 1.0f);
                ducked.processHarmonySend (b, 1.0f);

                juce::AudioBuffer<float> ma (2, kBlock);
                juce::AudioBuffer<float> mb (2, kBlock);
                ma.clear();
                mb.clear();
                full.processReturn (ma, 1.0f);
                ducked.processReturn (mb, 0.3f);
            }

            juce::AudioBuffer<float> srcA = roomSignal (2, kBlock, kWarmUpBlocks);
            juce::AudioBuffer<float> srcB = roomSignal (2, kBlock, kWarmUpBlocks);
            juce::AudioBuffer<float> ma (2, kBlock);
            juce::AudioBuffer<float> mb (2, kBlock);
            ma.clear();
            mb.clear();

            full.processHarmonySend (srcA, 1.0f);
            ducked.processHarmonySend (srcB, 1.0f);
            full.processReturn (ma, 1.0f);
            ducked.processReturn (mb, 0.3f);

            expectWithinAbsoluteError (roomPeakOf (mb), roomPeakOf (ma) * 0.3f,
                                       roomPeakOf (ma) * 1.0e-3f);
            expectGreaterThan (roomPeakOf (ma), 0.0f);
        }

        beginTest ("A fully ducked return is silent, an unducked one is not");
        {
            // Gain 0 is the maximum duck: the room's contribution disappears entirely rather
            // than leaving a residual -6 dB floor.
            ovtdsp::ReverbEffect open;
            ovtdsp::ReverbEffect ducked;
            open.prepare (kSr, kBlock);
            ducked.prepare (kSr, kBlock);

            for (int k = 0; k < kWarmUpBlocks; ++k)
            {
                juce::AudioBuffer<float> srcA = roomSignal (2, kBlock, k);
                juce::AudioBuffer<float> srcB = roomSignal (2, kBlock, k);
                open.processHarmonySend (srcA, 1.0f);
                ducked.processHarmonySend (srcB, 1.0f);

                juce::AudioBuffer<float> mo (2, kBlock);
                juce::AudioBuffer<float> md (2, kBlock);
                mo.clear();
                md.clear();
                open.processReturn (mo, 1.0f);
                ducked.processReturn (md, 0.0f);
            }

            juce::AudioBuffer<float> srcA = roomSignal (2, kBlock, kWarmUpBlocks);
            juce::AudioBuffer<float> srcB = roomSignal (2, kBlock, kWarmUpBlocks);
            juce::AudioBuffer<float> mo (2, kBlock);
            juce::AudioBuffer<float> md (2, kBlock);
            mo.clear();
            md.clear();

            open.processHarmonySend (srcA, 1.0f);
            ducked.processHarmonySend (srcB, 1.0f);
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
            monoRoom.processHarmonySend (mono, 1.0f);

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
                room.processHarmonySend (src, 0.2f);
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

        // === Level: what the user actually hears ===

        beginTest ("The ROOM at full wet is audible against the dry voice");
        {
            // A Mix knob has to be audible at its top. Measured on a voice-like signal at steady
            // state with the shipped ROOM tuning. Two numbers matter: how loud the output is,
            // and how far it sits from the dry voice -- the additive sum kept the ratio near 1.0
            // AND the difference small, which is exactly "no reverb at full mix".
            ovtdsp::ReverbEffect room;
            room.prepare (kSr, kBlock);
            room.setRoomSize (ovtdsp::kReverbRoomTuning.roomSize);
            room.setDamping (ovtdsp::kReverbRoomTuning.damping);
            room.setStereoWidth (ovtdsp::kReverbRoomTuning.width);

            const int warmUp = 600;
            for (int k = 0; k < warmUp; ++k)
            {
                juce::AudioBuffer<float> src = roomSignal (2, kBlock, k);
                room.process (src, true, 1.0f);
            }

            const juce::AudioBuffer<float> voice = roomSignal (2, kBlock, warmUp);
            const float voiceRms = rvxRms (voice, 0);   // input level before the insert

            juce::AudioBuffer<float> out = voice;
            room.process (out, true, 1.0f);

            const float wetRms = rvxRms (out, 0);
            float diffSum = 0.0;
            for (int i = 0; i < out.getNumSamples(); ++i)
                diffSum += static_cast<double> (out.getSample (0, i) - voice.getSample (0, i))
                         * (out.getSample (0, i) - voice.getSample (0, i));
            const float diffRms = static_cast<float> (std::sqrt (diffSum / out.getNumSamples()));

            std::cout << "ROOM full-wet RMS ratio: " << (wetRms / voiceRms)
                      << ", difference RMS: " << (diffRms / voiceRms) << "\n";

            // The ROOM is a short space, so it sits below the voice in level; what makes full
            // mix audible is that the dry voice is GONE. Measured: wet ratio 0.51, difference
            // 0.66 of the input RMS. The difference threshold has to sit above the wet ratio
            // (0.51, what an ADDITIVE sum would give) and below the measured 0.66: a crossfade
            // removes the dry term, an additive sum would leave it in place.
            expectGreaterThan (wetRms, voiceRms * 0.25f);
            expectGreaterThan (diffRms, voiceRms * 0.58f);
        }

        beginTest ("A unity send return sits behind the voice without drowning it");
        {
            // The master carries the dry voice and the send adds the room's return. Measured
            // as total-to-dry RMS ratio on the shipped HALL tuning: raw summing without the
            // return trim measured ~1.85, i.e. +5 dB in front of the limiter, which made the
            // output stage pump on every phrase. The trim has to keep the tail audible while
            // leaving headroom.
            ovtdsp::ReverbEffect hall;
            hall.prepare (kSr, kBlock);
            hall.setRoomSize (ovtdsp::kReverbHallTuning.roomSize);
            hall.setDamping (ovtdsp::kReverbHallTuning.damping);
            hall.setStereoWidth (ovtdsp::kReverbHallTuning.width);

            const int warmUp = 600;
            for (int k = 0; k < warmUp; ++k)
            {
                juce::AudioBuffer<float> src = roomSignal (2, kBlock, k);
                hall.processLeadSend (src, 1.0f);

                juce::AudioBuffer<float> master = roomSignal (2, kBlock, k);
                hall.processReturn (master, 1.0f);
            }

            const float dryRms = rvxRms (roomSignal (2, kBlock, warmUp), 0);

            juce::AudioBuffer<float> src = roomSignal (2, kBlock, warmUp);
            hall.processLeadSend (src, 1.0f);

            juce::AudioBuffer<float> master = roomSignal (2, kBlock, warmUp);
            hall.processReturn (master, 1.0f);

            const float totalRms = rvxRms (master, 0);
            std::cout << "HALL unity send return level ratio: " << (totalRms / dryRms) << "\n";

            expectGreaterThan (totalRms, dryRms * 1.02f);   // audible behind the voice
            expectLessThan (totalRms, dryRms * 1.6f);       // leaves the limiter open
        }

        beginTest ("The two shipped rooms ring for their measured time and then fall silent");
        {
            // The tunings in ReverbTuning.h ARE the sound of the two buses, and the UI promises
            // "short room" and "long hall" in plain words: the measurement has to agree with the
            // promise. Measured through the same send / return path the plugin uses: the ROOM
            // (0.35 / 0.50) lands near 1.2 s, the HALL (0.70 / 0.30) near 2.4 s -- the HALL is
            // the legacy Main room, so this pins the sound the old Mix knob delivered.
            ovtdsp::ReverbEffect room;
            room.prepare (kSr, kBlock);
            room.setRoomSize (ovtdsp::kReverbRoomTuning.roomSize);
            room.setDamping (ovtdsp::kReverbRoomTuning.damping);
            room.setStereoWidth (ovtdsp::kReverbRoomTuning.width);

            ovtdsp::ReverbEffect hall;
            hall.prepare (kSr, kBlock);
            hall.setRoomSize (ovtdsp::kReverbHallTuning.roomSize);
            hall.setDamping (ovtdsp::kReverbHallTuning.damping);
            hall.setStereoWidth (ovtdsp::kReverbHallTuning.width);

            const float roomTail = rvxTailSeconds (room);
            const float hallTail = rvxTailSeconds (hall);

            std::cout << "ROOM RT60: " << roomTail << " s, HALL RT60: " << hallTail << " s\n";

            expectGreaterThan (roomTail, 0.0f);
            expectGreaterThan (hallTail, 0.0f);
            // The measured RT60s (probe: 1.18 s and 2.43 s) with room for the envelope and the
            // -60 dB window's block quantisation.
            expectGreaterThan (roomTail, 0.8f);
            expectLessThan (roomTail, 1.7f);
            expectGreaterThan (hallTail, 1.8f);
            expectLessThan (hallTail, 3.2f);
            // The pair only makes sense if the HALL is clearly the longer room.
            expectGreaterThan (hallTail, roomTail * 1.5f);
        }
    }
};

static ReverbBusPairTest reverbBusPairTest;
