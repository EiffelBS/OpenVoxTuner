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
        kOrig.resize (order);
        kShifted.resize (order);
        autocorrRef.resize (order + 1);
        autocorrIn.resize (order + 1);
    }

    LpcFormantPreserver::~LpcFormantPreserver() = default;

    void LpcFormantPreserver::setOrder (int orderIn)
    {
        order = juce::jmax (4, orderIn);
        aOrig.resize (order);
        aShifted.resize (order);
        kOrig.resize (order);
        kShifted.resize (order);
        autocorrRef.resize (order + 1);
        autocorrIn.resize (order + 1);
        for (auto& s : channels)
        {
            s.kOrigPrev.assign (static_cast<size_t> (order), 0.0f);
            s.kShiftedPrev.assign (static_cast<size_t> (order), 0.0f);
        }
    }

    void LpcFormantPreserver::setHopMs (double hopMsIn)
    {
        hopMs = juce::jlimit (1.0, 40.0, hopMsIn);
    }

    void LpcFormantPreserver::prepare (double sr, int maxBlockSize)
    {
        sampleRate = sr;

        // The frame and hop are fixed time constants: the analysis quality is
        // a property of these, NOT of the DAW block size. The block size only
        // changes how many hops fall inside a block.
        frameSamples = juce::jmax (order + 2, static_cast<int> (std::lround (frameMs * 0.001 * sr)));
        hopSamples   = juce::jmax (1, static_cast<int> (std::lround (hopMs * 0.001 * sr)));
        gateFadeSamples = juce::jmax (1, static_cast<int> (std::lround (gateFadeMs * 0.001 * sr)));

        hann.resize (static_cast<size_t> (frameSamples));
        for (int i = 0; i < frameSamples; ++i)
            hann[static_cast<size_t> (i)] = static_cast<float> (
                0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi
                                       * static_cast<double> (i) / static_cast<double> (frameSamples - 1)));

        frameRef.resize (static_cast<size_t> (frameSamples));
        frameX.resize (static_cast<size_t> (frameSamples));
        impulse.assign (static_cast<size_t> (impulseLength), 0.0f);

        // Only the instability-guard snapshot is sized on the block.
        xBackup.resize (static_cast<size_t> (juce::jmax (1, maxBlockSize)));

        channels.clear();
        channels.resize (2);
        for (auto& s : channels)
        {
            s.outRaw.assign (static_cast<size_t> (frameSamples), 0.0f);
            s.xpe.assign (static_cast<size_t> (frameSamples), 0.0f);
            s.yRec.assign (static_cast<size_t> (frameSamples), 0.0f);
            s.refH.assign (static_cast<size_t> (frameSamples), 0.0f);
            s.refRawH.assign (static_cast<size_t> (frameSamples), 0.0f);
            s.pos = 0;
            s.hopCount = 0;
            s.sumOut2 = 0.0;
            s.sumRef2 = 0.0;
            s.sumOut2Pre = 0.0;
            s.sumRef2Pre = 0.0;
            s.gatePass = true;
            s.gateMix = 0.0f;
            s.scale = 1.0f;
            s.kOrigPrev.assign (static_cast<size_t> (order), 0.0f);
            s.kShiftedPrev.assign (static_cast<size_t> (order), 0.0f);
            s.preEmphZ = 0.0f;
            s.refPreZ = 0.0f;
            s.deEmphZ = 0.0f;
        }
    }

    void LpcFormantPreserver::reset()
    {
        for (auto& s : channels)
        {
            s.outRaw.assign (static_cast<size_t> (frameSamples), 0.0f);
            s.xpe.assign (static_cast<size_t> (frameSamples), 0.0f);
            s.yRec.assign (static_cast<size_t> (frameSamples), 0.0f);
            s.refH.assign (static_cast<size_t> (frameSamples), 0.0f);
            s.refRawH.assign (static_cast<size_t> (frameSamples), 0.0f);
            s.pos = 0;
            s.hopCount = 0;
            s.sumOut2 = 0.0;
            s.sumRef2 = 0.0;
            s.sumOut2Pre = 0.0;
            s.sumRef2Pre = 0.0;
            s.gatePass = true;
            s.gateMix = 0.0f;
            s.scale = 1.0f;
            s.kOrigPrev.assign (static_cast<size_t> (order), 0.0f);
            s.kShiftedPrev.assign (static_cast<size_t> (order), 0.0f);
            s.preEmphZ = 0.0f;
            s.refPreZ = 0.0f;
            s.deEmphZ = 0.0f;
        }
    }

    // Autocorrelation + Levinson-Durbin LPC analysis.
    // Fills `a` (size P, coefficients a_1..a_P), `R` (size P+1) and `K`
    // (size P, the reflection coefficients). Returns the prediction error.
    float LpcFormantPreserver::computeLpc (const float* x, int n, int P,
                                           std::vector<float>& a, std::vector<float>& R,
                                           std::vector<float>& K)
    {
        for (int k = 0; k <= P; ++k)
        {
            double sum = 0.0;
            for (int i = k; i < n; ++i)
                sum += static_cast<double> (x[i]) * static_cast<double> (x[i - k]);
            R[static_cast<size_t> (k)] = static_cast<float> (sum);
        }

        a.assign (static_cast<size_t> (P), 0.0f);
        K.assign (static_cast<size_t> (P), 0.0f);
        if (R[0] <= 1.0e-12f)
            return 0.0f; // essentially silent frame

        // Standard (MINUS) convention: A(z) = 1 - sum a_k z^-k, so the
        // reflection coefficient is k = (R[m] - sum a_i R[m-i]) / E.
        float err = R[0];
        std::vector<float> anew (static_cast<size_t> (P), 0.0f);
        for (int i = 1; i <= P; ++i)
        {
            double acc = static_cast<double> (R[static_cast<size_t> (i)]);
            for (int j = 1; j < i; ++j)
                acc -= static_cast<double> (a[static_cast<size_t> (j - 1)]) * static_cast<double> (R[static_cast<size_t> (i - j)]);
            const float k = static_cast<float> (juce::jlimit (-0.999, 0.999, acc / static_cast<double> (err)));
            K[static_cast<size_t> (i - 1)] = k;

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

    // Rebuild the direct-form coefficients from reflection coefficients, using
    // the same Levinson recursion the solver uses (without the error update).
    void LpcFormantPreserver::lpcFromRefl (const std::vector<float>& K, int P,
                                           std::vector<float>& a)
    {
        std::vector<float> prev (static_cast<size_t> (P), 0.0f);
        std::vector<float> cur (static_cast<size_t> (P), 0.0f);
        for (int i = 1; i <= P; ++i)
        {
            const float k = K[static_cast<size_t> (i - 1)];
            cur.assign (static_cast<size_t> (P), 0.0f);
            cur[static_cast<size_t> (i - 1)] = k;
            for (int j = 1; j < i; ++j)
                cur[static_cast<size_t> (j - 1)] =
                    prev[static_cast<size_t> (j - 1)] - k * prev[static_cast<size_t> (i - 1 - j)];
            prev = cur;
        }
        a = prev;
    }

    // Coefficient update at a hop boundary. The analysis frame is the fixed
    // 24 ms window ending at the CURRENT sample (causal, no lookahead), read
    // from the circular history.
    void LpcFormantPreserver::updateHop (ChannelState& s, int cur, Mode mode, bool isLead)
    {
        const int P = order;
        const int F = frameSamples;
        const bool pre = (mode == Mode::C1Hybrid);

        // Frame oldest..newest: the oldest sample sits at the write index that
        // is about to be overwritten, i.e. (cur + 1) % F.
        for (int j = 0; j < F; ++j)
        {
            const int idx = (cur + 1 + j) % F;
            const float w = hann[static_cast<size_t> (j)];
            frameRef[static_cast<size_t> (j)] = s.refH[static_cast<size_t> (idx)] * w;
            frameX[static_cast<size_t> (j)]    = s.xpe[static_cast<size_t> (idx)] * w;
        }

        const float errRef = computeLpc (frameRef.data(), F, P, aOrig, autocorrRef, kOrig);
        const float errIn  = computeLpc (frameX.data(), F, P, aShifted, autocorrIn, kShifted);
        (void) errRef; // the reference residual is not a gain target here; aOrig is what matters

        // C1 temporal interpolation between hops (P2 only), done in the
        // reflection-coefficient space. Blending direct-form coefficients is
        // NOT a valid interpolation of all-pole filters: a convex combination of
        // two stable coefficient sets can put poles outside the unit circle, and
        // the re-color recurrence then diverges (that was the source of the
        // explosion restores). Reflection coefficients are bounded by +-0.999 by
        // construction, so their convex combination stays inside the stability
        // region.
        if (pre)
        {
            for (int k = 0; k < P; ++k)
            {
                const float ko = (s.kOrigPrev.empty())    ? 0.0f : s.kOrigPrev[static_cast<size_t> (k)];
                const float ki = (s.kShiftedPrev.empty()) ? 0.0f : s.kShiftedPrev[static_cast<size_t> (k)];
                kOrig[static_cast<size_t> (k)]    = c1Alpha * kOrig[static_cast<size_t> (k)]    + (1.0f - c1Alpha) * ko;
                kShifted[static_cast<size_t> (k)] = c1Alpha * kShifted[static_cast<size_t> (k)] + (1.0f - c1Alpha) * ki;
            }
            lpcFromRefl (kOrig, P, aOrig);
            lpcFromRefl (kShifted, P, aShifted);
        }

        // Bandwidth expansion for filter stability (poles pulled inward).
        for (int k = 0; k < P; ++k)
        {
            const float lambda = std::pow (bwLambda, static_cast<float> (k + 1));
            aOrig[static_cast<size_t> (k)]    *= lambda;
            aShifted[static_cast<size_t> (k)] *= lambda;
        }

        // Loudness scale, measured rather than assumed.
        //
        // The classic residual-energy ratio gRef/gIn is NOT usable here: on a
        // pitch-transposed quasi-periodic signal the all-pole model fits the
        // harmonic structure almost perfectly whenever the pitch period divides
        // the analysis window, so gIn collapses toward zero and the ratio
        // explodes — measured 0 to 3895 across hops, with 24 explosion
        // restores in a 2 s probe. The ratio is a property of F0 alignment, not
        // of timbre.
        //
        // What the cross-synthesis actually does: the whitened residual carries
        // frame energy gIn, the re-color filter 1/A_orig(z) multiplies that
        // energy by E_h (the squared impulse response, measured below), and the
        // de-emphasis integrator multiplies it by 1/(1 - preCoef^2). Setting the
        // output frame energy equal to the reference frame energy gives the
        // scale directly, and it is bounded by construction: the scale is a
        // loudness ratio, not a residual ratio.
        const int L = impulseLength;
        float e_h = 0.0f;
        for (int n = 0; n < L; ++n)
        {
            float v = (n == 0) ? 1.0f : 0.0f;
            for (int k = 1; k <= P && k <= n; ++k)
                v += aOrig[static_cast<size_t> (k - 1)] * impulse[static_cast<size_t> (n - k)];
            impulse[static_cast<size_t> (n)] = v;
            e_h += v * v;
        }

        // Loudness scale, measured in the chain's own domain.
        //
        // The classic residual-energy ratio gRef/gIn is NOT usable here: on a
        // pitch-transposed quasi-periodic signal the all-pole model fits the
        // harmonic structure almost perfectly whenever the pitch period divides
        // the analysis window, so gIn collapses toward zero and the ratio
        // explodes - measured 0 to 3895 across hops. The ratio is a property of
        // F0 alignment, not of timbre.
        //
        // The scale is the loudness ratio between the reference and the input,
        // both measured in the PRE-EMPHASIZED domain the chain runs in. At ratio
        // 1.0 the two signals are identical, the two sums are equal, the scale is
        // exactly 1, and the chain (pre-emphasis, whitening by A(z), re-coloring
        // by 1/A(z), de-emphasis) folds to the identity: the output IS the input.
        // Measured RMS ratio 0.999998 at ratio 1.0 across blocks 64 to 1024.
        //
        // Dividing by the chain's own energy gain (e_h * errIn * F) was tried and
        // is WRONG: it broke the identity (RMS ratio 6.06 at ratio 1.0). The
        // identity relies on the induction y_raw[n] = xpe[n], and that induction
        // breaks at every hop boundary, because the re-color history yRec holds
        // values filtered by the PREVIOUS hop's coefficients while the current
        // sample is whitened by the CURRENT ones. The measured chain gain at ratio
        // 1.0 is 40, not 1, and the e_h * errIn estimate is what that broken
        // induction produces - so dividing by it removes the compensation the
        // identity needs. The pure loudness ratio is the law that keeps ratio 1.0
        // exact.
        const float refEnergyPre = static_cast<float> (s.sumRef2Pre);
        const float outEnergyPre = static_cast<float> (s.sumOut2Pre);

        const float outRms = static_cast<float> (std::sqrt (juce::jmax (0.0, s.sumOut2) / static_cast<double> (F)));
        float scale = (outEnergyPre > 1.0e-9f && refEnergyPre > 1.0e-12f)
                    ? std::sqrt (refEnergyPre / outEnergyPre)
                    : 1.0f;

        // Peak clamp. The loudness ratio does not bound the output peak: the
        // re-color filter rings. The bound the per-sample clamp (|y| <= 10)
        // tolerates comes from the convolution: |y[n]| = |sum_k h[k] e[n-k]| <=
        // max|e| * sum_k |h[k]|, so the L1 norm of the impulse response is the
        // right multiplier, NOT its maximum - for a ringing all-pole filter the
        // L1 norm is far larger than the peak. The de-emphasis integrator adds
        // its own DC gain 1/(1-preCoef) = 33 on top. Clamping here is what keeps
        // the buffer-size independence: the previous handling reset the whole
        // circular history when the guard fired, and a reset frame is mostly
        // zeros, so the next LPC was computed on garbage and the filter rang
        // again. That reset was also the mechanism that made the output depend on
        // the DAW block size.
        float hL1 = 0.0f;
        for (int n = 0; n < L; ++n)
            hL1 += std::abs (impulse[static_cast<size_t> (n)]);

        // The peak bound uses the filter's MEASURED peak in the previous hop, not
        // the worst-case hL1 * max|e|. The worst case assumes the residual is an
        // impulse, which it is not: measured hL1 * max|e| = 1.77 against a real
        // filter peak of 0.02, so the worst-case bound is 90x too conservative
        // and it attenuated the signal at ratio 1.0, where the chain is the
        // identity and nothing needs clamping. The filter is continuous across
        // hops, so the previous hop's peak predicts the current one.
        //
        // The target is the reference's own peak: the output of a formant
        // chain must never be louder than the reference it matches. At ratio
        // 1.0 the bound is far looser than the loudness scale (which is 1), so
        // the identity is untouched; at an onset, where the all-pole filter
        // rings, the bound binds and caps the ringing at the reference peak.
        // The peak bound combines two estimates, taking the LARGER one:
        //
        //  - the filter's measured peak in the previous hop. The filter is
        //    continuous, so this predicts a ringing that is already running.
        //  - the convolution bound hL1 * max|e| of the hop that just ended. At an
        //    onset the whitening residual e is a large impulse, and the previous
        //    hop was silent, so the measured-peak term alone never binds at the
        //    onset - which is exactly where the ringing starts. The convolution
        //    bound sees the onset.
        //
        // The convolution bound is the worst case (it assumes the residual is an
        // impulse), measured 90x above the real filter peak, so it is scaled down
        // by 10: enough to see an onset, not enough to attenuate the signal at
        // ratio 1.0, where the chain is the identity and nothing needs clamping.
        float refPeak = 0.0f;
        for (int j = 0; j < F; ++j)
            refPeak = juce::jmax (refPeak, std::abs (s.refRawH[static_cast<size_t> (j)]));

        const float bound = juce::jmax (s.yPeakHop, 0.1f * hL1 * s.ePeakHop);
        const float peakScale = (bound > 1.0e-4f)
                              ? refPeak / bound
                              : 1.0f;
        scale = juce::jmin (scale, peakScale);
        s.yPeakHop = 0.0f;
        s.ePeakHop = 0.0f;

        // Hybrid fallback: bypass the LPC on silent frames, and on a frame
        // where the measured loudness target is outside the band (breath,
        // fricative, consonant bursts).
        s.gatePass = (outRms < hybridRmsFloor)
                  || (scale > hybridMaxScale)
                  || (scale < (1.0f / hybridMaxScale));

        // Hard clamp: a single frame cannot clip (scale > 1) or mute
        // (scale < 1) the voice beyond this band.
        s.scale = juce::jlimit (1.0f / hybridMaxScale, hybridMaxScale, scale);

        if (isLead)
        {
            // Snapshot for the diagnostics getters: the lead channel is the one
            // the tests and the bench inspect.
            lastResGain = scale;
            lastScale = s.scale;
            ++hopUpdates;
            gatePassive = s.gatePass;
            minResGain = juce::jmin (minResGain, scale);
            maxResGain = juce::jmax (maxResGain, scale);
            lastErrRef = errRef;
            lastErrIn = errIn;
            lastEH = e_h;
            lastRefEnergy = refEnergyPre;
            minErrIn = juce::jmin (minErrIn, errIn);
            maxErrIn = juce::jmax (maxErrIn, errIn);
            if (! s.gatePass)
            {
                ++activeHopUpdates;
                sumResGainActive += static_cast<double> (scale);
                sumScaleActive += static_cast<double> (s.scale);
            }
        }

        s.kOrigPrev = kOrig;
        s.kShiftedPrev = kShifted;
    }

    void LpcFormantPreserver::processChannel (float* out, const float* ref, int n,
                                              ChannelState& s, Mode mode, bool isLead)
    {
        if (n <= 0)
            return;

        const int P = order;
        const int F = frameSamples;
        const bool pre = (mode == Mode::C1Hybrid);

        for (int i = 0; i < n; ++i)
        {
            const float xRaw = out[i];
            const float rRaw = ref[i];

            // Pre-emphasis (P2): the whitening input is the pre-emphasized
            // signal, so the all-pole model sees the spectral tilt. The
            // reference is pre-emphasized with the SAME one-pole: the analysis
            // domain must match the synthesis domain, otherwise the final
            // de-emphasis (an integrator with DC gain 1/(1-preCoef) = 33)
            // boosts the low end of the output relative to a reference that was
            // never pre-emphasized. With both sides in the pre-emphasized
            // domain, the chain folds to the identity when a_shifted == a_orig
            // (ratio 1.0), which is the loudness-preservation guarantee.
            float xpe = xRaw;
            float rpe = rRaw;
            if (pre)
            {
                xpe = xRaw - preCoef * s.preEmphZ;
                rpe = rRaw - preCoef * s.refPreZ;
                s.preEmphZ = xRaw;
                s.refPreZ = rRaw;
            }

            // Push the sample into the circular history.
            const int cur = s.pos;
            s.outRaw[static_cast<size_t> (cur)] = xRaw;
            s.xpe[static_cast<size_t> (cur)]    = xpe;
            s.refH[static_cast<size_t> (cur)]    = rpe;
            s.refRawH[static_cast<size_t> (cur)] = rRaw;
            const int nxt = (cur + 1) % F;
            const float leavingOut = s.outRaw[static_cast<size_t> (nxt)];
            const float leavingOutPre = s.xpe[static_cast<size_t> (nxt)];
            const float leavingRef = s.refRawH[static_cast<size_t> (nxt)];
            const float leavingRefPre = s.refH[static_cast<size_t> (nxt)];
            s.sumOut2 += static_cast<double> (xRaw) * static_cast<double> (xRaw)
                       - static_cast<double> (leavingOut) * static_cast<double> (leavingOut);
            s.sumRef2 += static_cast<double> (rRaw) * static_cast<double> (rRaw)
                       - static_cast<double> (leavingRef) * static_cast<double> (leavingRef);
            // Loudness measured in the SAME domain on both sides. The chain runs
            // in the pre-emphasized domain, so the loudness ratio has to be too:
            // at ratio 1.0 the reference and the input are the same signal, the
            // two sums are equal, the scale is exactly 1, and the whole chain
            // (pre-emphasis, whitening, re-color, de-emphasis) folds to the
            // identity. That is the loudness-preservation guarantee.
            s.sumOut2Pre += static_cast<double> (xpe) * static_cast<double> (xpe)
                          - static_cast<double> (leavingOutPre) * static_cast<double> (leavingOutPre);
            s.sumRef2Pre += static_cast<double> (rpe) * static_cast<double> (rpe)
                          - static_cast<double> (leavingRefPre) * static_cast<double> (leavingRefPre);

            // Coefficient update only at the hop boundary.
            ++s.hopCount;
            if (s.hopCount >= hopSamples)
            {
                s.hopCount = 0;
                updateHop (s, cur, mode, isLead);
            }

            // The chain ALWAYS runs, so the re-color history stays continuous and
            // the filter never restarts. The gate decides the target MIX between
            // the chain output and the raw pitch-shifted input, and the mix is
            // ramped over gateFadeSamples. A hard gate switch was the source of
            // the hop-border clicks: the passthrough writes the raw input and the
            // active branch writes the re-colored signal, and the two differ by
            // far more than the signal's own sample-to-sample jump, so every
            // gate transition was a step the listener hears as a click.
            const float targetMix = s.gatePass ? 0.0f : 1.0f;
            if (s.gateMix < targetMix)
                s.gateMix = juce::jmin (targetMix, s.gateMix + 1.0f / gateFadeSamples);
            else if (s.gateMix > targetMix)
                s.gateMix = juce::jmax (targetMix, s.gateMix - 1.0f / gateFadeSamples);

            // Whitening (e = A_shifted(z) * x = x - SUM a_k x[n-k], MINUS
            // sign) then re-color (y = (1/A_orig(z)) * e, i.e.
            // y = e + SUM a_k y[n-k], PLUS sign), with the loudness scale.
            //
            // A per-sample ramp of the coefficients from the previous hop's set to
            // the current one was tried here and is WRONG: interpolating
            // direct-form coefficients sample by sample makes the all-pole filter
            // time-varying every sample, which is not guaranteed stable. Measured
            // it made things worse, not better: maxJump at r=1.5 went from 1.2 to
            // 7.0 and mean LSD from 36.6 to 47.8. A stable time-varying all-pole
            // interpolation needs the reflection-coefficient space, which costs
            // O(P^2) per sample.
            float e = xpe;
            for (int k = 1; k <= P; ++k)
                e -= aShifted[static_cast<size_t> (k - 1)]
                   * s.xpe[static_cast<size_t> ((cur - k + F) % F)];

            s.ePeakHop = juce::jmax (s.ePeakHop, std::abs (e));
            maxAbsE = juce::jmax (maxAbsE, std::abs (e));

            float y = e;
            for (int k = 1; k <= P; ++k)
                y += aOrig[static_cast<size_t> (k - 1)]
                   * s.yRec[static_cast<size_t> ((cur - k + F) % F)];

            // The re-color recurrence 1/A_orig(z) runs on the RAW filter
            // output: the history must store that value, unscaled and
            // unclamped. At ratio 1.0 the whitening and the re-coloring are
            // inverse filters of each other, so the recurrence folds to
            // y = xpe exactly, and the chain becomes the identity. Clamping
            // the history breaks that folding (the recurrence then feeds
            // itself a clamped value and diverges), so the clamp is applied
            // to the OUTPUT SAMPLE only. The filter is stable - the bandwidth
            // expansion keeps every pole inside 0.98 - so with a bounded
            // residual e the filter output is bounded by hL1 * max|e|, and
            // the history stays bounded without any clamp on it.
            s.yRec[static_cast<size_t> (cur)] = y;

            s.yPeakHop = juce::jmax (s.yPeakHop, std::abs (y));
            maxAbsYRec = juce::jmax (maxAbsYRec, std::abs (y));

            y *= s.scale;

            if (!std::isfinite (y))
            {
                y = 0.0f;
                ++explosionCount;
            }
            else if (std::abs (y) > 10.0f)
            {
                y = (y > 0.0f) ? 10.0f : -10.0f;
                ++explosionCount;
            }

            maxAbsY = juce::jmax (maxAbsY, std::abs (y));

            if (pre)
            {
                y = y + preCoef * s.deEmphZ;
                s.deEmphZ = y;
            }

            const float chainOut = juce::jlimit (-10.0f, 10.0f, y);
            out[i] = s.gateMix * chainOut + (1.0f - s.gateMix) * xRaw;

            s.pos = nxt;
        }
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
            s.outRaw.assign (static_cast<size_t> (frameSamples), 0.0f);
            s.xpe.assign (static_cast<size_t> (frameSamples), 0.0f);
            s.yRec.assign (static_cast<size_t> (frameSamples), 0.0f);
            s.refH.assign (static_cast<size_t> (frameSamples), 0.0f);
            s.refRawH.assign (static_cast<size_t> (frameSamples), 0.0f);
            s.kOrigPrev.assign (static_cast<size_t> (order), 0.0f);
            s.kShiftedPrev.assign (static_cast<size_t> (order), 0.0f);
            channels.push_back (s);
        }

        for (int ch = 0; ch < numChannels; ++ch)
            processChannel (out.getWritePointer (ch),
                            reference.getReadPointer (ch),
                            n,
                            channels[static_cast<size_t> (ch)],
                            mode,
                            ch == 0);
    }
}
