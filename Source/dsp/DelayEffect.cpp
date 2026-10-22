// DelayEffect.cpp
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include "DelayEffect.h"

namespace ovtdsp
{
    int DelayEffect::nextPowerOfTwo (int wanted)
    {
        int v = 1;
        // Guard the shift: a request beyond 1<<30 would wrap signed ints, and such a size is
        // not a delay line anyone can use. The clamp keeps capacity positive in every case.
        while (v < wanted && v < (1 << 29))
            v <<= 1;
        return v;
    }

    void DelayEffect::prepare (double sampleRateIn, int maximumBlockSize)
    {
        // A host handing over 0 gets the classic rate: every coefficient below is computed from
        // this value, so a zero would divide by zero in the ramp maths.
        sampleRate = sampleRateIn > 0.0 ? sampleRateIn : 44100.0;

        const int maxBlock = juce::jmax (maximumBlockSize, 1);

        minCopySamples = static_cast<int> (std::llround (kMinDelaySeconds * sampleRate));
        if (minCopySamples < 1) minCopySamples = 1;   // a sub-sample floor would read the newest write

        // The longest copy this instance will ever be asked for: bpm at its clamp floor (20) and the
        // division at its longest fraction (1/2D, 3 beats) => 9.0 s. Sizing here means process()
        // allocates nothing, which is the codebase rule (ReverbEffect.h sizes its buffers in prepare()).
        // The ring is rounded up to a power of two, so at the common 44.1/48/96/192 kHz rates 9 s fits
        // the same block count 6 s used to (measured: 524288 samples at 48 kHz either way).
        maxCopySamples = static_cast<int> (std::llround (maxCopySeconds() * sampleRate));

        // The ring needs room for the oldest readable sample PLUS the samples written while reading it,
        // so a whole block of writes cannot overwrite history that is still being read.
        capacity = nextPowerOfTwo (maxBlock + maxCopySamples + 1);

        // Sized ONCE. fillWithSampleData=false + the clear below = zeros, so a fresh instance reads
        // silence and the tail starts empty.
        lines.setSize (2, capacity, false, true, false);

        for (auto& pole : feedbackLowCut)
            pole.prepare (sampleRate, kFeedbackLowCutHz);   // 150 Hz per channel, inside the loop

        rampSamples = juce::jmax (1, static_cast<int> (std::llround (kRampSeconds * sampleRate)));
        copyRamp.reset (rampSamples);   // a division/tempo change walks to its target over 25 ms

        bypassCoeff = 1.0f - static_cast<float> (std::exp (-1.0 / (kRampSeconds * sampleRate)));
        crossCoeff  = bypassCoeff;     // the copy crossfade rides the same 25 ms constant
        crossFade   = 1.0f;            // nothing in flight on a fresh instance
        bypassGain  = 0.0f;            // fully bypassed at first, exactly as Saturation/DeEsser start
        pos         = 0;
        copyInteger = minCopySamples;   // the first block must not read closer than the comb floor
        copyFraction = 0.0f;

        updateTargetCopy (false);       // no crossfade: there is no previous distance to fade from
        copyRamp.setCurrentAndTargetValue (copyRamp.getTargetValue());   // seed at target: no ramp from a stale value
    }

    void DelayEffect::reset()
    {
        lines.clear();          // zero both lines; the tail is gone, nothing is read back
        pos = 0;

        for (auto& pole : feedbackLowCut)
            pole.reset();

        bypassGain = 0.0f;      // fully bypassed: the ramp fades the stage back in
        crossFade  = 1.0f;      // a reset clears any copy crossfade in flight

        // Re-target for the CURRENT bpm/division and seed the read there. Seeding at the comb floor
        // instead (as a plain ramp reset does) pins the stage at 40 ms for the rest of the session:
        // hosts call reset() from prepareToPlay(), and only a later setBpm/setDivision sets copyDirty.
        updateTargetCopy (false);
        const float seeded = copyRamp.getTargetValue();
        copyRamp.setCurrentAndTargetValue (seeded);
        copyInteger  = juce::jmax (minCopySamples, static_cast<int> (seeded));
        copyFraction = juce::jlimit (0.0f, 1.0f, seeded - static_cast<float> (copyInteger));
    }

    void DelayEffect::process (juce::AudioBuffer<float>& buffer, bool enabled, float wetMix)
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();

        if (numChannels <= 0 || numSamples <= 0 || capacity <= 0) return;   // not prepared: untouched pass-through

        if (copyDirty) updateTargetCopy();   // bpm/division changes re-target the ramp, never mid-sample

        const int wetChannels = juce::jmin (numChannels, kMaxChannels);
        const float depth     = mix * juce::jlimit (0.0f, 1.0f, wetMix);   // internal mix; wiring hands wetMix = 1.0

        const bool fullyBypassed = (! enabled) && bypassGain < kBypassFloor;

        const auto* inL  = buffer.getReadPointer (0);
        const auto* inR  = wetChannels > 1 ? buffer.getReadPointer (1) : inL;   // mono: one signal feeds both lines
        auto*       outL = buffer.getWritePointer (0);
        auto*       outR = wetChannels > 1 ? buffer.getWritePointer (1) : nullptr;

        if (fullyBypassed)
        {
            // OUTPUT NEVER TOUCHED in this branch: a disabled delay leaves the buffer bit-identical,
            // so bypass cannot add DC, noise, or any change to the voice. The lines are still fed with
            // the input, which is what a muted console does -- the tape keeps running -- so re-enabling
            // resumes on the CURRENT timeline instead of resurrecting an old segment. Cost: two writes per
            // sample, no read, no interpolation, no feedback.
            auto* l = lines.getWritePointer (0);
            auto* r = lines.getWritePointer (1);

            for (int n = 0; n < numSamples; ++n)
            {
                pos += 1;
                if (pos >= capacity) pos = 0;
                l[pos] = inL[n];
                r[pos] = inR[n];
            }
            return;
        }

        for (int n = 0; n < numSamples; ++n)
        {
            // Enable ramp per sample, the same 25 ms crossfade as DeEsser/Saturation: doing it once per
            // block would leave a phase jump inside long blocks when the switch flips mid-block.
            bypassGain += ((enabled ? 1.0f : 0.0f) - bypassGain) * bypassCoeff;

            advanceReadPosition();   // one ramp step, so the copy length moves continuously (no click)

            float tailL = 0.0f, tailR = 0.0f;
            readTailPair (tailL, tailR);   // steady tap, or the 25 ms two-tap crossfade of a copy change

            // Tails are read BEFORE the write position moves: that is what keeps a zero-delay feedback
            // loop unreachable, and cross-feeding them is what alternates echoes left/right.
            writeWithCrossFeedback (inL[n], inR[n], tailL, tailR);

            // Dry stays at unity, the echo is ADDED: wetMix arrives as 1.0 from the wiring lambda, so this
            // multiplier is identity there and the stage behaves as an additive send. The master trim +
            // true-peak limiter sit after every bus, so a loud ping-pong is guarded by exactly the same
            // output stage as every other level in the plugin (SaturationEffect.h notes the same reasoning).
            const float contribution = bypassGain * depth;

            outL[n] += contribution * tailL;
            if (outR != nullptr)
                outR[n] += contribution * tailR;   // mono output takes the L side: documented in the class doc
        }
    }
}
