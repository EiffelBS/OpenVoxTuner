// Doubler.cpp
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include "Doubler.h"
#include <cmath>

namespace ovtdsp
{
    // Ramp time constant for the amount knob: 60 ms. Long enough that a knob
    // move never steps the level (the compensation is applied to the lead too),
    // short enough that the spread feels immediate on a fast automation pass.
    static constexpr float kAmountTauSeconds = 0.060f;

    void Doubler::prepare (double sr, int maxBlockSize) noexcept
    {
        sampleRate = (sr > 0.0) ? sr : 44100.0;

        // Delay line must cover the maximum Haas offset (25 ms) plus one whole
        // block, so a read never lands on a position we are about to write.
        const int maxDelaySamples = static_cast<int> (std::ceil (25.0 * 0.001 * sampleRate)) + 2;
        lineSize = maxDelaySamples + juce::jmax (1, maxBlockSize) + 2;

        delayLine.setSize (2, lineSize, false, true, true);
        delayLine.clear();

        writePos   = 0;
        prevAmount = 0.0f;
        curAmount  = 0.0f;
    }

    void Doubler::reset() noexcept
    {
        // Clear the delay line so a transport stop / sample-rate change does not
        // leave audible audio in the Haas tail. The parameter target is NOT
        // cleared: it is re-read from APVTS on every block by the caller.
        delayLine.clear();
        writePos   = 0;
        prevAmount = 0.0f;
        curAmount  = 0.0f;
    }

    void Doubler::process (const juce::AudioBuffer<float>& copy,
                           juce::AudioBuffer<float>& main) noexcept
    {
        const int n = main.getNumSamples();
        if (n <= 0 || lineSize <= 0)
            return;

        // CPU short-circuit: at amount 0 there is no copy to mix, the lead path
        // stays bit-identical, and the caller skips the pitch-shifter call.
        if (! isActive())
        {
            prevAmount = 0.0f;
            curAmount  = 0.0f;
            writePos  += n;   // keep indices consistent for the next activation
            return;
        }

        const int nCopy = juce::jmin (n, copy.getNumSamples());
        if (nCopy <= 0)
        {
            writePos += n;
            return;
        }

        // Block-aware smoothing: the time constant is in seconds, so the ramp
        // is identical at 64 and at 1024 samples per block.
        const double blockDur = static_cast<double> (n) / sampleRate;
        const float alpha = (kAmountTauSeconds > 0.0f)
            ? static_cast<float> (1.0 - std::exp (-blockDur / kAmountTauSeconds))
            : 1.0f;

        prevAmount = curAmount;
        curAmount += alpha * (targetAmount - curAmount);

        const int numCh = main.getNumChannels();
        float* outL  = main.getWritePointer (0);
        float* outR  = (numCh > 1) ? main.getWritePointer (1) : nullptr;

        const float* copyL = copy.getReadPointer (0);
        const float* copyR = (copy.getNumChannels() > 1) ? copy.getReadPointer (1) : copyL;

        const float invN = 1.0f / static_cast<float> (n);
        const float* dl0 = delayLine.getReadPointer (0);
        const float* dl1 = delayLine.getReadPointer (1);
        float* wl0 = delayLine.getWritePointer (0);
        float* wl1 = delayLine.getWritePointer (1);

        for (int i = 0; i < n; ++i)
        {
            // Per-sample ramp of the amount across the block, so the gains and
            // the Haas offset glide sample-by-sample (no click, no quantisation step).
            const float a = prevAmount + (curAmount - prevAmount) * (static_cast<float> (i) + 1.0f) * invN;

            const float s = a * 0.01f;
            const float g = s * 0.6f;                       // copy level, always <= lead
            const float comp = 1.0f / std::sqrt (1.0f + g * g);   // constant perceived RMS

            // Write the newest copy sample first, then read backwards: a delay
            // shorter than one block reads audio written earlier in this block.
            const int wIdx = static_cast<int> (writePos % lineSize);
            wl0[wIdx] = (i < nCopy) ? copyL[i] : 0.0f;
            if (numCh > 1)
                wl1[wIdx] = (i < nCopy) ? copyR[i] : 0.0f;

            const float delaySamples = haasDelaySamples (a, sampleRate);
            const double pos = static_cast<double> (writePos) - static_cast<double> (delaySamples);

            float delayedL = 0.0f, delayedR = 0.0f;
            if (pos >= 0.0)
            {
                const int i0 = static_cast<int> (std::floor (pos));
                const float frac = static_cast<float> (pos - static_cast<double> (i0));
                const int idx0 = i0 % lineSize;
                const int idx1 = (i0 + 1) % lineSize;
                delayedL = dl0[idx0] + frac * (dl0[idx1] - dl0[idx0]);
                if (numCh > 1)
                    delayedR = dl1[idx0] + frac * (dl1[idx1] - dl1[idx0]);
            }

            if (numCh > 1)
            {
                // Equal-power pan of the copy, normalised to unity at centre. The
                // copy level g is applied ON TOP of the pan gains: gL^2 + gR^2 = 2,
                // so the mean copy power added to the stereo pair is exactly g^2,
                // which is what `comp = 1/sqrt(1+g^2)` compensates. Max g*gR is
                // 0.6*sqrt(2) = 0.848, so the copy never exceeds the lead.
                float gL, gR;
                panGains (s, gL, gR);   // spread = +s (copy goes right of the centred lead)
                outL[i] = (outL[i] + g * gL * delayedL) * comp;
                outR[i] = (outR[i] + g * gR * delayedR) * comp;
            }
            else
            {
                // Mono: pan has no meaning, so the copy is added in phase at its
                // level and the compensation keeps the perceived level flat.
                outL[i] = (outL[i] + g * delayedL) * comp;
            }

            ++writePos;
        }
    }
}
