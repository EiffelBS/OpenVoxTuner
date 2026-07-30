// LpcFormantPreserver.cpp
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include "LpcFormantPreserver.h"
#include <cmath>
#include <cstring>

namespace ovtdsp
{
    LpcFormantPreserver::LpcFormantPreserver (int orderIn)
        : order (orderIn)
    {
        aOrig.resize (order);
        aShifted.resize (order);
        autocorrRef.resize (order + 1);
        autocorrIn.resize (order + 1);
    }

    LpcFormantPreserver::~LpcFormantPreserver() = default;

    void LpcFormantPreserver::setOrder (int orderIn)
    {
        order = juce::jmax (4, orderIn);
        aOrig.resize (order);
        aShifted.resize (order);
        autocorrRef.resize (order + 1);
        autocorrIn.resize (order + 1);
    }

    void LpcFormantPreserver::prepare (double sr, int maxBlockSize)
    {
        sampleRate = sr;
        const int ch = 2;
        channels.clear();
        channels.resize (ch);
        for (auto& s : channels)
        {
            s.xHist.assign (order, 0.0f);
            s.yHist.assign (order, 0.0f);
            s.aOrigPrev.assign (order, 0.0f);
            s.aShiftedPrev.assign (order, 0.0f);
            s.preEmphZ = 0.0f;
            s.deEmphZ = 0.0f;
        }
        xCopy.resize (static_cast<size_t> (juce::jmax (1, maxBlockSize)));
        yRec.resize (static_cast<size_t> (juce::jmax (1, maxBlockSize)));
        xBackup.resize (static_cast<size_t> (juce::jmax (1, maxBlockSize)));
    }

    void LpcFormantPreserver::reset()
    {
        for (auto& s : channels)
        {
            s.xHist.assign (order, 0.0f);
            s.yHist.assign (order, 0.0f);
            s.aOrigPrev.assign (order, 0.0f);
            s.aShiftedPrev.assign (order, 0.0f);
            s.preEmphZ = 0.0f;
            s.deEmphZ = 0.0f;
        }
    }

    // Autocorrelation + Levinson-Durbin LPC analysis.
    // Fills `a` (size P, coefficients a_1..a_P) and `R` (size P+1).
    // Returns the final prediction-error energy (used for gain normalization).
    float LpcFormantPreserver::computeLpc (const float* x, int n, int P,
                                           std::vector<float>& a, std::vector<float>& R)
    {
        for (int k = 0; k <= P; ++k)
        {
            double sum = 0.0;
            for (int i = k; i < n; ++i)
                sum += static_cast<double> (x[i]) * static_cast<double> (x[i - k]);
            R[k] = static_cast<float> (sum);
        }

        a.assign (static_cast<size_t> (P), 0.0f);
        if (R[0] <= 1.0e-12f)
            return 0.0f; // essentially silent frame

        // Standard (MINUS) convention: A(z) = 1 - sum a_k z^-k, so the
        // reflection coefficient is k = (R[m] - sum a_i R[m-i]) / E.
        float err = R[0];
        std::vector<float> anew (static_cast<size_t> (P), 0.0f);
        for (int i = 1; i <= P; ++i)
        {
            double acc = static_cast<double> (R[i]);
            for (int j = 1; j < i; ++j)
                acc -= static_cast<double> (a[j - 1]) * static_cast<double> (R[i - j]);
            const float k = static_cast<float> (juce::jlimit (-0.999, 0.999, acc / err));

            anew.assign (static_cast<size_t> (P), 0.0f);
            anew[static_cast<size_t> (i - 1)] = k;
            for (int j = 1; j < i; ++j)
                anew[static_cast<size_t> (j - 1)] =
                    a[static_cast<size_t> (j - 1)] - k * a[static_cast<size_t> (i - 1 - j)];
            a = anew;

            err *= (1.0f - k * k);
            if (err <= 1.0e-12f)
                break;
        }
        return err;
    }

    void LpcFormantPreserver::processChannel (float* out, const float* ref, int n,
                                              ChannelState& s, Mode mode)
    {
        if (n <= 0)
            return;

        const int P = order;
        const bool pre = (mode == Mode::C1Hybrid);

        // 0) Snapshot the raw input so we can restore it verbatim if the LPC
        //    chain blows up (instability guard).
        std::memcpy (xBackup.data(), out, static_cast<size_t> (n) * sizeof (float));

        // 1) LPC analysis of the reference (target formants) and the input.
        const float errRef = computeLpc (ref, n, P, aOrig, autocorrRef);
        const float errIn  = computeLpc (out, n, P, aShifted, autocorrIn);

        const float gRef = std::sqrt (juce::jmax (1.0e-9f, errRef / static_cast<float> (n)));
        const float gIn  = std::sqrt (juce::jmax (1.0e-9f, errIn  / static_cast<float> (n)));
        // Residual-gain match: the whitened input has per-sample energy gIn^2,
        // the reference excitation gRef^2. Scale so the re-colored output
        // matches the reference loudness.
        const float resGain = gRef / gIn;

        // 2) C1 temporal interpolation of LPC coefficients (P2 only).
        if (pre)
        {
            for (int k = 0; k < P; ++k)
            {
                const float ao = (s.aOrigPrev.empty()) ? 0.0f : s.aOrigPrev[static_cast<size_t> (k)];
                const float ai = (s.aShiftedPrev.empty()) ? 0.0f : s.aShiftedPrev[static_cast<size_t> (k)];
                aOrig[static_cast<size_t> (k)]    = c1Alpha * aOrig[static_cast<size_t> (k)]    + (1.0f - c1Alpha) * ao;
                aShifted[static_cast<size_t> (k)] = c1Alpha * aShifted[static_cast<size_t> (k)] + (1.0f - c1Alpha) * ai;
            }
        }

        // 3) Bandwidth expansion for filter stability (poles pulled inward).
        for (int k = 0; k < P; ++k)
        {
            const float lambda = std::pow (bwLambda, static_cast<float> (k + 1));
            aOrig[static_cast<size_t> (k)]    *= lambda;
            aShifted[static_cast<size_t> (k)] *= lambda;
        }

        // 4) Hybrid fallback (P2): bypass LPC on silent/unstable frames.
        float frameRms = 0.0f;
        for (int i = 0; i < n; ++i)
            frameRms += out[i] * out[i];
        frameRms = std::sqrt (frameRms / static_cast<float> (n));
        float scale = resGain;

        if (frameRms < hybridRmsFloor || scale > hybridMaxScale || scale < (1.0f / hybridMaxScale))
        {
            // Pass-through: keep the (already pitch-shifted) output unchanged.
            // Still advance history so the next active frame stays continuous.
            for (int k = 0; k < P; ++k)
            {
                s.xHist[static_cast<size_t> (k)] = out[n - P + k];
                s.yHist[static_cast<size_t> (k)] = out[n - P + k];
            }
            // Update pre-emphasis state with the raw input so it does not drift.
            if (pre)
            {
                s.preEmphZ = out[n - 1];
                s.deEmphZ = out[n - 1];
            }
            s.aOrigPrev = aOrig;
            s.aShiftedPrev = aShifted;
            return;
        }

        // Hard clamp on the residual-gain scale: the prediction-error ratio
        // can swing wildly on real audio (breath, fricatives). Keeping the
        // scale in a tight band avoids both clipping (scale > 1) and muting
        // (scale < 1) on individual frames.
        scale = juce::jlimit (0.5f, 2.0f, scale);

        // 5) Build the (optionally pre-emphasized) input copy for whitening
        //    history reads (out[] is overwritten during processing).
        for (int i = 0; i < n; ++i)
        {
            float xi = out[i];
            if (pre)
            {
                const float xpe = xi - preCoef * s.preEmphZ;
                s.preEmphZ = xi;
                xi = xpe;
            }
            xCopy[static_cast<size_t> (i)] = xi;
        }

        // 6) Whitening (e = A_shifted(z) * x = x - SUM a_k x[n-k], MINUS sign)
        //    then re-color (y = (1/A_orig(z)) * e, i.e. y = e + SUM a_k y[n-k],
        //    PLUS sign), with residual-gain scaling. Standard MINUS convention.
        //
        //    Instability guard: the all-pole re-synthesis can blow up on
        //    pathological frames (e.g. onsets with low LPC order, near-silent
        //    reference, or extreme residual gain). If any sample exceeds
        //    `outputExplodeThresh` we treat the whole block as failed and
        //    restore the pre-LPC output. The next block will retry.
        bool exploded = false;
        for (int i = 0; i < n; ++i)
        {
            float e = xCopy[static_cast<size_t> (i)];
            for (int k = 1; k <= P; ++k)
            {
                const int idx = i - k;
                const float xk = (idx >= 0) ? xCopy[static_cast<size_t> (idx)]
                                            : s.xHist[static_cast<size_t> (P + idx)];
                e -= aShifted[static_cast<size_t> (k - 1)] * xk;
            }

            float y = e;
            for (int k = 1; k <= P; ++k)
            {
                const int idx = i - k;
                const float yk = (idx >= 0) ? yRec[static_cast<size_t> (idx)]
                                            : s.yHist[static_cast<size_t> (P + idx)];
                y += aOrig[static_cast<size_t> (k - 1)] * yk;
            }

            y *= scale;

            if (pre)
            {
                const float yde = y + preCoef * s.deEmphZ;
                s.deEmphZ = yde;
                y = yde;
            }

            if (!std::isfinite (y) || std::abs (y) > 10.0f)
            {
                exploded = true;
                break;
            }

            yRec[static_cast<size_t> (i)] = y; // pre-de-emphasis value for recurrence
            out[i] = y;
        }

        if (exploded)
        {
            // Instability guard: restore the pre-LPC input verbatim so the
            // pitch-shifted signal still reaches the output (just without the
            // formant correction on this single block). This prevents the
            // permanent mute that would otherwise follow a single runaway
            // sample, and the next block re-attempts the LPC.
            std::memcpy (out, xBackup.data(), static_cast<size_t> (n) * sizeof (float));
            s.xHist.assign (order, 0.0f);
            s.yHist.assign (order, 0.0f);
            s.aOrigPrev = aOrig;
            s.aShiftedPrev = aShifted;
            return;
        }

        // 7) Update cross-block histories with the last P samples.
        for (int k = 0; k < P; ++k)
        {
            s.xHist[static_cast<size_t> (k)] = xCopy[static_cast<size_t> (n - P + k)];
            s.yHist[static_cast<size_t> (k)] = yRec[static_cast<size_t> (n - P + k)];
        }
        s.aOrigPrev = aOrig;
        s.aShiftedPrev = aShifted;
    }

    void LpcFormantPreserver::process (juce::AudioBuffer<float>& out,
                                       const juce::AudioBuffer<float>& reference,
                                       float /*ratio*/,
                                       Mode mode)
    {
        const int numChannels = juce::jmin (2, out.getNumChannels(), reference.getNumChannels());
        const int n = out.getNumSamples();
        if (n <= 0 || numChannels <= 0)
            return;

        while (static_cast<int> (channels.size()) < numChannels)
        {
            ChannelState s;
            s.xHist.assign (order, 0.0f);
            s.yHist.assign (order, 0.0f);
            s.aOrigPrev.assign (order, 0.0f);
            s.aShiftedPrev.assign (order, 0.0f);
            channels.push_back (s);
        }

        for (int ch = 0; ch < numChannels; ++ch)
            processChannel (out.getWritePointer (ch),
                            reference.getReadPointer (ch),
                            n,
                            channels[static_cast<size_t> (ch)],
                            mode);
    }
}



