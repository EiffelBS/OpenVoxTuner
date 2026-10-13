// LpcFormantPreserver.cpp
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include "LpcFormantPreserver.h"
#include <algorithm>
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
            s.kFromOrig.assign    (static_cast<size_t> (order), 0.0f);
            s.kToOrig.assign      (static_cast<size_t> (order), 0.0f);
            s.kFromShifted.assign (static_cast<size_t> (order), 0.0f);
            s.kToShifted.assign   (static_cast<size_t> (order), 0.0f);
            s.kCurOrig.assign     (static_cast<size_t> (order), 0.0f);
            s.kCurShifted.assign  (static_cast<size_t> (order), 0.0f);
            s.bdA.assign (static_cast<size_t> (order), 0.0f);
            s.bdB.assign (static_cast<size_t> (order), 0.0f);
            s.bdC.assign (static_cast<size_t> (order), 0.0f);
            s.bdD.assign (static_cast<size_t> (order), 0.0f);
        }
    }

    // Reset one channel's circular history, ramp state and sliding sums. Used by
    // prepare() and reset() so the two paths cannot drift apart.
    void LpcFormantPreserver::initChannel (ChannelState& s)
    {
        s.outRaw.assign   (static_cast<size_t> (frameSamples), 0.0f);
        s.xpe.assign      (static_cast<size_t> (frameSamples), 0.0f);
        s.yRec.assign     (static_cast<size_t> (frameSamples), 0.0f);
        s.chainH.assign   (static_cast<size_t> (frameSamples), 0.0f);
        s.refH.assign     (static_cast<size_t> (frameSamples), 0.0f);
        s.refRawH.assign  (static_cast<size_t> (frameSamples), 0.0f);
        s.deH.assign      (static_cast<size_t> (frameSamples + 1), 0.0f);
        s.dePos = 0;
        s.pos = 0;
        s.hopCount = 0;
        s.rampPos = 0;
        s.sumOut2 = 0.0;
        s.sumRef2 = 0.0;
        s.sumOut2Pre = 0.0;
        s.sumRef2Pre = 0.0;
        s.sumChain2 = 0.0;
        s.emaIn = 0.0;
        s.emaChain = 0.0;
        s.emaOut = 0.0;
        s.emaSeeded = false;
        s.emaActive = true;
        s.runPeak = 0.0f;
        s.regionStart = 0;
        s.regionIn = 0.0;
        s.regionChain = 0.0;
        s.regionOut = 0.0;
        s.regionFinal = 0.0;
        s.regionCross = 0.0;
        s.regionMix = 0.0;
        s.regionInActive = 0.0;
        s.regionChainActive = 0.0;
        s.regionOutActive = 0.0;
        s.regionIdealOut = 0.0;
        s.regionActiveSamples = 0;
        s.regionSamples = 0;
        s.gatePass = true;
        s.gateMix = 0.0f;
        s.scale = 1.0f;
        s.scaleFrom = 1.0f;
        s.scaleTo = 1.0f;
        s.kFromOrig.assign    (static_cast<size_t> (order), 0.0f);
        s.kToOrig.assign      (static_cast<size_t> (order), 0.0f);
        s.kFromShifted.assign (static_cast<size_t> (order), 0.0f);
        s.kToShifted.assign   (static_cast<size_t> (order), 0.0f);
        s.kCurOrig.assign     (static_cast<size_t> (order), 0.0f);
        s.kCurShifted.assign  (static_cast<size_t> (order), 0.0f);
        s.bdA.assign (static_cast<size_t> (order), 0.0f);
        s.bdB.assign (static_cast<size_t> (order), 0.0f);
        s.bdC.assign (static_cast<size_t> (order), 0.0f);
        s.bdD.assign (static_cast<size_t> (order), 0.0f);
        s.preEmphZ = 0.0f;
        s.refPreZ = 0.0f;
        s.deEmphZ = 0.0f;
        s.ePeakHop = 0.0f;
        s.yPeakHop = 0.0f;
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

        // One-hop time constant for the loudness averages. The identity
        // sum(out^2) = scale^2 * sum(chain^2) = sum(in) only closes when the scale
        // is built from the SAME hop's sums it is applied to. A 1 s average chased a
        // denominator that swings six orders of magnitude between hops (REALsumChain2
        // 0.006 .. 2.24e6): the scale collapsed to 0.000 on the ringing hops and the
        // output measured 7x too quiet (region voiced in = 129.84, out = 18.62 at
        // r=1.5). A one-hop average makes scale(k) = sqrt(sumIn(k-1)/sumChain(k-1)),
        // the exact per-hop energy match with a single 10 ms delay.
        // One-hop loudness window: the fastest measured convergence of the AGC loop.
        // A 0.5 s window measured WORSE (RMS ratio 0.634/0.636 against 0.656/0.663),
        // and damping the step to 0.3 was worse still (0.591/0.597): over the 3 s test
        // signal the loop is under-converged, not oscillating, so the response must be
        // as fast as the statistic allows. The residual deficit (applied energy-weighted
        // scale 0.110 against the ideal 0.173) is NOT a clamp: the peak clamp and the
        // headroom limiter both measure non-binding, so the all-pole re-color gain is
        // the term that cannot be matched by any level law on this architecture.
        emaBeta = 1.0f / static_cast<float> (hopSamples);

        hann.resize (static_cast<size_t> (frameSamples));
        for (int i = 0; i < frameSamples; ++i)
            hann[static_cast<size_t> (i)] = static_cast<float> (
                0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi
                                       * static_cast<double> (i) / static_cast<double> (frameSamples - 1)));

        frameRef.resize (static_cast<size_t> (frameSamples));
        frameX.resize (static_cast<size_t> (frameSamples));
        impulse.assign (static_cast<size_t> (impulseLength), 0.0f);
        simX.assign (static_cast<size_t> (frameSamples + order), 0.0f);
        simY.assign (static_cast<size_t> (frameSamples + order), 0.0f);

        // Only the instability-guard snapshot is sized on the block.
        xBackup.resize (static_cast<size_t> (juce::jmax (1, maxBlockSize)));

        channels.clear();
        channels.resize (2);
        for (auto& s : channels)
            initChannel (s);
    }

    void LpcFormantPreserver::reset()
    {
        for (auto& s : channels)
            initChannel (s);
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

    // Allocation-free Levinson recursion from reflection coefficients to direct
    // form, ping-ponging between two preallocated buffers. Used by the
    // per-sample coefficient ramp, where a std::vector allocation every sample
    // (44100 times a second, on up to 5 instances) is not acceptable.
    // Returns the buffer that holds the result.
    float* LpcFormantPreserver::buildDirectFromRefl (const float* K, int P,
                                                     float* bufA, float* bufB)
    {
        for (int i = 0; i < P; ++i)
        {
            bufA[static_cast<size_t> (i)] = 0.0f;
            bufB[static_cast<size_t> (i)] = 0.0f;
        }
        float* src = bufA;
        float* dst = bufB;
        for (int i = 1; i <= P; ++i)
        {
            const float k = K[static_cast<size_t> (i - 1)];
            dst[static_cast<size_t> (i - 1)] = k;
            for (int j = 1; j < i; ++j)
                dst[static_cast<size_t> (j - 1)] =
                    src[static_cast<size_t> (j - 1)] - k * src[static_cast<size_t> (i - 1 - j)];
            std::swap (src, dst);
        }
        return src;
    }

    // Coefficient update at a hop boundary. The analysis frame is the fixed
    // 24 ms window ending at the CURRENT sample (causal, no lookahead), read
    // from the circular history.
    void LpcFormantPreserver::updateHop (ChannelState& s, int cur, Mode mode, bool isLead)
    {
        const int P = order;
        const int F = frameSamples;
        (void) mode; // the coefficient ramp applies in both modes; mode only selects pre-emphasis

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

        // Advance the coefficient ramp. The set fully reached at the END of the
        // previous hop becomes the start of this hop's ramp, so the coefficients
        // are continuous across the boundary: sample `cur` runs on kFrom (alpha
        // 0) and the last sample of this hop runs on kTo (alpha ~1). The ramp is
        // carried in reflection-coefficient space, where a convex combination of
        // two sets bounded by +-0.999 is guaranteed stable; the direct form is
        // rebuilt from the interpolated set every sample.
        s.kFromOrig    = s.kToOrig;
        s.kFromShifted = s.kToShifted;
        s.kToOrig      = kOrig;
        s.kToShifted   = kShifted;
        s.rampPos = 0;

        // Bandwidth expansion for filter stability (poles pulled inward). It is
        // applied to the DIRECT form, exactly as the per-sample ramp applies it,
        // so the impulse response measured below matches what the chain runs.
        {
            float lambda = bwLambda;
            for (int k = 0; k < P; ++k)
            {
                aOrig[static_cast<size_t> (k)]    *= lambda;
                aShifted[static_cast<size_t> (k)] *= lambda;
                lambda *= bwLambda;
            }
        }

        // Impulse response of the re-color filter 1/A_orig(z) at the hop's
        // endpoint coefficients: its squared energy is the filter's energy gain.
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

        hopErrRef = errRef;
        hopErrIn  = errIn;
        hopEH     = e_h;
    }

    // The per-hop chain simulation that yields the loudness scale. Called from
    // processChannel AFTER the current sample's yRec is written, so the P history
    // samples the simulation seeds are exactly the values the chain will read when
    // it runs the next sample.
    void LpcFormantPreserver::measureScale (ChannelState& s, int cur, Mode mode, bool isLead)
    {
        const int P = order;
        const int F = frameSamples;
        const bool pre = (mode == Mode::C1Hybrid);

        // Chain energy, SIMULATED over the analysis frame. The chain is run on the
        // UNWINDOWED input frame with this hop's coefficients, and the energy of the
        // simulated output (after the de-emphasis, in the RAW domain) is the gain
        // the scale has to cancel. Nothing in the simulation involves the chain's
        // running OUTPUT, so the scale cannot feed back into the measurement.
        //
        // The simulation has to start on the chain's REAL history, not on zeros:
        // the re-color filter rings across hop boundaries, and a zero-state
        // simulation of a frame that follows a ringing hop measures almost nothing.
        // Measured chainEnergySim swinging 0.0015 to 139.5 against a reference
        // energy of 0.37 to 24.97, the scale swinging 0.024 to 68.2, and the output
        // peak hitting the +-10 clamp (9.92 against a reference peak of 0.82).
        //
        // The identity at ratio 1.0 with the input equal to the reference is
        // preserved by construction: aShifted == aOrig, whitening then re-coloring
        // is a round trip and the de-emphasis inverts the pre-emphasis, so the
        // simulated output is the raw input, the two energies are equal, and the
        // scale is exactly 1. The induction that makes the REAL chain fold to the
        // identity SAMPLE BY SAMPLE is the per-sample coefficient ramp in reflection
        // space, measured max |y_raw - xpe| = 0.000042.
        //
        // Measured alternatives, all rejected:
        //
        //  - The classic residual ratio gRef/gIn is unusable on a pitch-transposed
        //    quasi-periodic signal: the all-pole model fits the harmonic structure
        //    almost perfectly whenever the pitch period divides the window, so gIn
        //    collapses toward zero and the ratio explodes - measured 0 to 3895
        //    across hops. It is a property of F0 alignment, not of timbre.
        //  - The Levinson shortcut errIn * e_h = R0 is NOT the chain energy: it
        //    holds only for the UNEXPANDED predictor. The chain whitens with the
        //    bandwidth-expanded coefficients, so errIn * e_h measured 1/36.9 of the
        //    input energy at ratio 1.0 and put the scale at 6.08.
        //  - e_h * (energy of the whitened frame) assumes the residual is white. It
        //    is not - the bandwidth-expanded predictor is not the optimal one - and
        //    the estimate came in 2.05x high, RMS ratio 1.43 at ratio 1.0.
        //  - Measuring the chain's own running output energy is a feedback loop. At
        //    ratio 1.0 it has no restoring force (every constant scale is a fixed
        //    point), measured the applied scale swinging 0.07 to 5.3 with RMS ratio
        //    1.10. At ratio 1.5 the chain rings at every onset, the measured energy
        //    is 100-250x the reference, the scale collapses to 0.05 and ramps back
        //    through 9.7, and the output lands at RMS ratio 4.08 with 15121
        //    explosion restores.
        // The loudness target is the INPUT's energy over the frame, not the
        // reference's. The card's criterion is RMS(out/in) within +-2% at ratio
        // 1.5 and 2.0: transposing must not change the loudness of the signal
        // entering the plugin. outRaw is the chain's raw input, so sumOut2 is
        // exactly that energy, on the same sliding frame the simulation runs on.
        // At ratio 1.0 the input and the reference are the same signal, so this
        // is identical to the reference energy and the scale stays 1.
        //
        // sumOut2 is a SLIDING sum (add the new sample, drop the one leaving the
        // frame), so float drift drives it slightly below zero on a silent frame -
        // measured refEnergy printing -0.000000. A negative target makes the sqrt
        // return NaN, and jlimit propagates it straight into the ramped scale.
        const float targetEnergyWin = juce::jmax (0.0, s.sumOut2);
        float chainEnergySim  = 0.0f;
        float peakSim         = 0.0f;
        float simErr2         = 0.0f;
        float simMaxErr       = 0.0f;
        float simYMaxErr      = 0.0f;
        float simHistErr      = 0.0f;

        // The de-emphasis integrator state the frame's FIRST sample read. The
        // frame is F samples and the history ring is F slots, so that state sits
        // in the same slot as the newest sample and is already overwritten:
        // seeding from s.deEmphZ (the state at the newest sample) made the
        // simulated integrator start with the frame's ENDING state and carry a
        // constant offset decaying as 0.97^j over the whole frame. Measured at
        // hop 3, seed -0.230919 against the true d[265] = 0, offset -0.223992,
        // offset^2 * sum(0.9409^j) = 0.849 = the recorded simErr2 0.848941. The
        // F+1 slot ring below keeps that state, so the seed is exact.
        const float simDeStart = s.deH[static_cast<size_t> (s.dePos)];
        float simDe = simDeStart;

        for (int k = 1; k <= P; ++k)
        {
            // The frame's oldest sample sits at (cur + 1) % F, so the P samples
            // BEFORE the frame are cur, cur-1, ... cur-P+1. Laid out oldest first
            // at index 0..P-1 of the scratch buffers, so that simX[P-1] is the
            // sample just before simX[P] = xpe[cur+1], the frame's oldest.
            // Two measured indexing bugs: (cur + P + 1 - k) lands INSIDE the frame,
            // so simX[P-1] and simX[P] were the same sample and the simulation ran
            // the frame's first P samples twice, deviating from the identity by 9%
            // of the frame energy at every bwLambda, including 0.60 where the
            // induction error is 1e-6. And the whole simulation has to run AFTER
            // yRec[cur] is written: called from updateHop, yRec[cur] still held the
            // value written F samples earlier, one full frame stale.
            const int idx = (cur - P + k + F) % F;
            simX[static_cast<size_t> (k - 1)] = s.xpe[static_cast<size_t> (idx)];
            simY[static_cast<size_t> (k - 1)] = s.yRec[static_cast<size_t> (idx)];
            simHistErr = juce::jmax (simHistErr,
                                     std::abs (s.yRec[static_cast<size_t> (idx)]
                                             - s.xpe[static_cast<size_t> (idx)]));
        }
        for (int j = 0; j < F; ++j)
            simX[static_cast<size_t> (P + j)] = s.xpe[static_cast<size_t> ((cur + 1 + j) % F)];

        // The simulation runs on UNWINDOWED samples, exactly like the chain: the
        // hann window is only what the LPC sees. Simulating on the windowed frame
        // broke the identity, because the de-emphasis integrator is the exact
        // inverse of the pre-emphasis only on the unwindowed signal - the window
        // taper adds sum_k 0.97^k (w[n-k] - w[n]) xpe[n-k] to the integrator, so
        // the simulated energy was not the raw energy and the scale measured
        // 0.2541 to 1.0329 (mean 0.9504) at ratio 1.0, where the chain is the
        // identity and the scale must be exactly 1.
        std::vector<float> headX, headY, headDe, headOut;
        for (int j = 0; j < F; ++j)
        {
            const int p = P + j;

            float e = simX[static_cast<size_t> (p)];
            for (int k = 1; k <= P; ++k)
                e -= aShifted[static_cast<size_t> (k - 1)] * simX[static_cast<size_t> (p - k)];

            float y = e;
            for (int k = 1; k <= P; ++k)
                y += aOrig[static_cast<size_t> (k - 1)] * simY[static_cast<size_t> (p - k)];
            simY[static_cast<size_t> (p)] = y;

            const float yErr = y - simX[static_cast<size_t> (p)];
            simYMaxErr = juce::jmax (simYMaxErr, std::abs (yErr));

            if (pre)
                simDe = y + preCoef * simDe;
            else
                simDe = y;
            chainEnergySim += simDe * simDe;
            peakSim = juce::jmax (peakSim, std::abs (simDe));

            const float dErr = simDe - s.outRaw[static_cast<size_t> ((cur + 1 + j) % F)];
            simErr2 += dErr * dErr;
            simMaxErr = juce::jmax (simMaxErr, std::abs (dErr));

            if (isLead && diagFirstSimHeadX.empty () && j < 8)
            {
                headX.push_back (simX[static_cast<size_t> (p)]);
                headY.push_back (y);
                headDe.push_back (simDe);
                headOut.push_back (s.outRaw[static_cast<size_t> ((cur + 1 + j) % F)]);
            }
        }
        // Keep the head of the FIRST ACTIVE hop: the silent startup hops carry no
        // signal and prove nothing. Once the seed is exact the deviation collapses
        // to float rounding, so a deviation threshold never fires - the head of the
        // first hop that carries signal is what shows the identity holding.
        if (isLead && diagFirstSimHeadX.empty () && chainEnergySim > 1.0e-6f)
        {
            diagFirstSimHeadInit.push_back (simDeStart);
            diagFirstSimHeadInit.push_back (s.outRaw[static_cast<size_t> (cur)]);
            diagFirstSimHeadX  = std::move (headX);
            diagFirstSimHeadY  = std::move (headY);
            diagFirstSimHeadDe = std::move (headDe);
            diagFirstSimHeadOut = std::move (headOut);
        }

        // The loudness scale from the LONG-window ratio, not from the 24 ms frame.
        // Measured why the frame is the wrong window: the chain's REAL sliding
        // energy over one frame swung 13 to 1357 between consecutive hops at r=1.5
        // (71x), so a scale read from a frame lagged by two hops landed 43x above
        // the global ideal of 0.0088 and the measured RMS ratio was 8.889. The
        // +-2% RMS criterion is defined on the whole stream, so the scale has to be
        // measured on the same window: the ratio of two 0.5 s exponential averages.
        // At r=1.0 the chain is the identity, both averages track the same signal,
        // and the scale is 1.
        // Seed on the FIRST ACTIVE hop. Starting the averages at zero made the ratio
        // guard return 1.0, so the first hops ran with scale 1.0 on a chain already
        // ~200x the input: that is where the explosions and the global RMS of 1.1196
        // came from, not from the steady region (measured final/in = 0.6172). The
        // frame means are the same quantity the per-sample updates feed, so the first
        // scale is the true ratio from hop one.
        const double frameIn    = juce::jmax (targetEnergyWin, 0.0f) / static_cast<double> (F);
        const double frameChain = juce::jmax (s.sumChain2, 0.0) / static_cast<double> (F);
        const double floorPower = static_cast<double> (hybridRmsFloor) * static_cast<double> (hybridRmsFloor);
        if (! s.emaSeeded && frameIn >= floorPower)
        {
            s.emaIn    = frameIn;
            s.emaChain = frameChain;
            s.emaOut   = frameIn; // start the AGC loop at unity gain, scale 1
            s.emaSeeded = true;
        }

        // Multiplicative AGC. The direct formula scale = sqrt(emaIn / emaChain) is a
        // one-shot estimate: on a hop where the filter rings, emaChain is 20x the
        // input power and the scale collapses to 0.04 for the following voiced hops
        // (measured rows: chain 692 -> scale 0.0396 -> outRms 0.026 against an input
        // RMS of 0.057). The loop form multiplies the PREVIOUS scale by the ratio of
        // the target power to the power actually delivered, so its fixed point is
        // emaOut = scale^2 * emaChain = emaIn, i.e. exact loudness, and a transient
        // in one hop moves the gain by one factor instead of jumping to a new value.
        const double agcRatio = (s.emaOut > 1.0e-12)
                    ? s.emaIn / s.emaOut
                    : 1.0;
        // Undamped step: the fixed point is exact (scale^2 * emaChain = emaIn, i.e.
        // RMS(out) = RMS(in)) and a transient moves the gain by one factor. Damping
        // the step to 0.3 was measured WORSE (RMS ratio 0.591/0.597 against
        // 0.656/0.663 undamped): the loop was under-converged over the 3 s test
        // signal, not oscillating, so the step must be full.
        float scale = s.scaleTo * std::sqrt (juce::jmax (0.0, agcRatio));

        const float outRms = static_cast<float> (std::sqrt (juce::jmax (0.0, s.sumOut2) / static_cast<double> (F)));

        // Peak clamp. The loudness scale matches the frame ENERGY, which does not
        // bound the output PEAK: the re-color filter rings, so a single transient
        // can put the output far above the reference peak. The bound is the peak of
        // the simulated chain over the analysis frame, the same simulation that
        // gave the energy, so it is scale-free and sees the onset inside the frame.
        //
        // The target is the reference's own peak: the output of a formant chain must
        // never be louder than the reference it matches. At ratio 1.0 with the
        // input equal to the reference, the simulated peak IS the reference peak,
        // peakScale is 1, and the clamp never binds.
        //
        // Two measured alternatives, both rejected:
        //
        //  - The convolution bound hL1 * max|e|: measured hL1 = 76 to 4554 and
        //    hL1 * max|e| = 7.6x the real filter peak, so the bound inflated to
        //    0.528 against a real peak of 0.069, peakScale fell to 0.4372, and the
        //    onset of every syllable was attenuated by more than half.
        //  - The filter's measured peak in the PREVIOUS hop. The filter is
        //    continuous, so that predicts a ringing that is already running, but at
        //    an onset the previous hop is silent, the measured peak is 0, and the
        //    clamp never binds - which is exactly where the ringing starts. Measured
        //    output jump 0.67086 against the reference's 0.10559.
        // The peak target is the INPUT's peak over the same frame, for the same
        // reason as the energy: the transposition must not change the loudness, so
        // the output peak has to stay at the level the plugin is fed, not at the
        // reference's level. At ratio 1.0 the two are the same signal.
        float refPeak = 0.0f;
        for (int j = 0; j < F; ++j)
            refPeak = juce::jmax (refPeak, std::abs (s.outRaw[static_cast<size_t> (j)]));

        // The bound has to cover the RAMP, not just the new coefficients. The
        // simulation runs the frame on this hop's fixed coefficients, but the chain
        // starts the hop on the PREVIOUS hop's coefficients (the ramp begins at
        // kFrom) and rings across the boundary. Measured: the clamp bound every hop
        // and the output peak still hit 4.22 against a reference peak of 0.82,
        // because the peak of the ramp's first samples is never simulated.
        // yPeakHop alone was rejected: at an onset the previous hop is silent, the
        // bound is 0 and the clamp never binds. The max of the two covers both.
        // The peak law as the TARGET scale was measured and rejected: bound =
        // max(peakSim, yPeakHop) underestimates the ramp's boundary ringing, so on a
        // silent hop the bound is 0.02 against an input peak of 0.30, peakScale
        // reaches 12.08, and the chain rings through it. Measured RMS(out/in) = 10.32
        // at r=1.5 with 765 explosions. The energy law stays the target; the peak
        // law only caps it.
        const float bound = juce::jmax (peakSim, s.yPeakHop);
        const float peakScale = (bound > 1.0e-4f)
                              ? refPeak / bound
                              : 1.0f;
        scale = juce::jmin (scale, peakScale);

        // The guard ceiling for the hop: twice the input's own peak, so a clipped
        // sample can never be louder than the signal the plugin is fed.
        s.peakLimit = juce::jmax (2.0f * refPeak, 1.0e-4f);
        const float diagYPeak = s.yPeakHop;
        const float diagEPeak = s.ePeakHop;
        s.yPeakHop = 0.0f;
        s.ePeakHop = 0.0f;

        // Hybrid fallback: bypass the LPC on silent frames, and on a frame
        // where the measured loudness target is outside the band (breath,
        // fricative, consonant bursts).
        s.gatePass = (outRms < hybridRmsFloor)
                  || (scale > hybridMaxScale)
                  || (scale < (1.0f / hybridMaxScale));

        // The loudness average is frozen on silent frames. The re-color filter rings
        // on after the excitation stops, and that ringing is gated out of the output,
        // so it must not enter the ratio the scale is built from: measured at r=1.5,
        // 99.99% of the region's chain energy sat on samples the gate had closed, and
        // the voiced output landed at 0.45x the input. Only the silence test drives
        // this; the scale-band passthrough is an explode guard, not a loudness state.
        s.emaActive = (outRms >= hybridRmsFloor);

        // Hard clamp: a single frame cannot clip (scale > 1) or mute
        // (scale < 1) the voice beyond this band. The scale is ramped across the
        // hop exactly like the coefficients: a hard scale step at the hop
        // boundary is an amplitude step the listener hears as a click.
        s.scaleFrom = s.scaleTo;   // the scale fully reached at the end of the previous hop
        s.scaleTo   = juce::jlimit (1.0f / hybridMaxScale, hybridMaxScale, scale);
        s.scale     = s.scaleTo;   // the hop's target, for the diagnostics getters

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
            lastErrRef = hopErrRef;
            lastErrIn = hopErrIn;
            lastEH = hopEH;
            lastRefEnergy = targetEnergyWin;
            minErrIn = juce::jmin (minErrIn, hopErrIn);
            maxErrIn = juce::jmax (maxErrIn, hopErrIn);
            minScaleApplied = juce::jmin (minScaleApplied, s.scaleTo);
            maxScaleApplied = juce::jmax (maxScaleApplied, s.scaleTo);
            sumScaleApplied += static_cast<double> (s.scaleTo);
            if (diagFirstRefEnergy.size () < 16)
            {
                diagFirstRefEnergy.push_back (targetEnergyWin);
                diagFirstChainEnergy.push_back (chainEnergySim);
                diagFirstScale.push_back (scale);
                diagFirstScaleApplied.push_back (s.scaleTo);
                diagFirstRefPeak.push_back (refPeak);
                diagFirstBound.push_back (bound);
                diagFirstPeakScale.push_back (peakScale);
                diagFirstEPeak.push_back (diagEPeak);
                diagFirstYPeak.push_back (diagYPeak);
                diagFirstSumOut2.push_back (static_cast<float> (s.sumOut2));
                diagFirstSumRef2.push_back (static_cast<float> (s.sumRef2));
                diagFirstSumChain2.push_back (static_cast<float> (s.sumChain2));
                diagFirstEmaIn.push_back (static_cast<float> (s.emaIn));
                diagFirstEmaChain.push_back (static_cast<float> (s.emaChain));
                diagFirstSimDeLast.push_back (simDe);
                diagFirstXRawLast.push_back (s.outRaw[static_cast<size_t> (cur)]);
                diagFirstSimErr2.push_back (simErr2);
                diagFirstSimMaxErr.push_back (simMaxErr);
                diagFirstSimYMaxErr.push_back (simYMaxErr);
                diagFirstSimHistErr.push_back (simHistErr);
            }
            diagRefEnergy.push_back (targetEnergyWin);
            diagChainEnergy.push_back (chainEnergySim);
            diagScale.push_back (scale);
            diagScaleApplied.push_back (s.scaleTo);
            diagLastEmaIn.push_back (static_cast<float> (s.emaIn));
            diagLastEmaChain.push_back (static_cast<float> (s.emaChain));
            diagLastOutRms.push_back (outRms);
            diagLastGateMix.push_back (s.gateMix);
            diagLastSumChain2.push_back (static_cast<float> (s.sumChain2));
            diagLastChainSim.push_back (chainEnergySim);
            if (diagRefEnergy.size () > 100)
            {
                diagRefEnergy.erase (diagRefEnergy.begin ());
                diagChainEnergy.erase (diagChainEnergy.begin ());
                diagScale.erase (diagScale.begin ());
                diagScaleApplied.erase (diagScaleApplied.begin ());
                diagLastEmaIn.erase (diagLastEmaIn.begin ());
                diagLastEmaChain.erase (diagLastEmaChain.begin ());
                diagLastOutRms.erase (diagLastOutRms.begin ());
                diagLastGateMix.erase (diagLastGateMix.begin ());
                diagLastSumChain2.erase (diagLastSumChain2.begin ());
                diagLastChainSim.erase (diagLastChainSim.begin ());
            }
            if (! s.gatePass)
            {
                ++activeHopUpdates;
                sumResGainActive += static_cast<double> (scale);
                sumScaleActive += static_cast<double> (s.scale);
            }
        }
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
            const bool hopBoundary = (s.hopCount >= hopSamples);
            if (hopBoundary)
            {
                s.hopCount = 0;
                updateHop (s, cur, mode, isLead);
            }

            // Per-sample coefficient ramp, in REFLECTION-coefficient space.
            //
            // The induction that makes the chain fold to the identity at ratio 1.0
            // (y_raw[n] = xpe[n]) requires the SAME coefficients for the current
            // sample and for the P history samples the two recurrences read. When
            // the coefficients step hard at the hop boundary, yRec holds values
            // filtered by the PREVIOUS hop's set, the induction fails for the P
            // samples straddling the boundary, and the filter rings: measured chain
            // gain 40 at ratio 1.0, not 1. That is the root cause of both the
            // loudness error at ratio != 1 and the transient ringing.
            //
            // Interpolating the DIRECT form sample by sample was tried and is
            // WRONG: a time-varying direct-form all-pole filter is not guaranteed
            // stable, and measured it made things worse, not better (maxJump at
            // r=1.5 from 1.2 to 7.0, mean LSD from 36.6 to 47.8). Reflection
            // coefficients are bounded by +-0.999 by construction, so a convex
            // combination of two sets stays inside the stability region. The direct
            // form is rebuilt from the interpolated set every sample: O(P^2/2),
            // 128 ops at P=16, twice per sample, about 1% of the block deadline
            // with 5 instances.
            const float alpha = (s.rampPos < hopSamples)
                              ? static_cast<float> (s.rampPos) / static_cast<float> (hopSamples)
                              : 1.0f;
            for (int k = 0; k < P; ++k)
            {
                const size_t ki = static_cast<size_t> (k);
                s.kCurOrig[ki]    = s.kFromOrig[ki]
                                  + alpha * (s.kToOrig[ki]    - s.kFromOrig[ki]);
                s.kCurShifted[ki] = s.kFromShifted[ki]
                                  + alpha * (s.kToShifted[ki] - s.kFromShifted[ki]);
            }
            float* aO = buildDirectFromRefl (s.kCurOrig.data(), P, s.bdA.data(), s.bdB.data());
            float* aS = buildDirectFromRefl (s.kCurShifted.data(), P, s.bdC.data(), s.bdD.data());
            float lambda = bwLambda;
            for (int k = 0; k < P; ++k)
            {
                aO[static_cast<size_t> (k)] *= lambda;
                aS[static_cast<size_t> (k)] *= lambda;
                lambda *= bwLambda;
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
            float e = xpe;
            for (int k = 1; k <= P; ++k)
                e -= aS[static_cast<size_t> (k - 1)]
                   * s.xpe[static_cast<size_t> ((cur - k + F) % F)];

            s.ePeakHop = juce::jmax (s.ePeakHop, std::abs (e));
            maxAbsE = juce::jmax (maxAbsE, std::abs (e));

            float y = e;
            for (int k = 1; k <= P; ++k)
                y += aO[static_cast<size_t> (k - 1)]
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

            // Direct test of the induction the chain relies on.
            maxInductionErr = juce::jmax (maxInductionErr, std::abs (y - xpe));

            // The hop's scale measurement, AFTER yRec[cur] is written: the P
            // history samples the simulation seeds must be the values the chain
            // will read when it runs the next sample, and yRec[cur] is only
            // current once this line has run.
            if (hopBoundary)
                measureScale (s, cur, mode, isLead);

            maxAbsYRec = juce::jmax (maxAbsYRec, std::abs (y));

            // The loudness scale is ramped across the hop with the same alpha as
            // the coefficients, so the gain has no step at the hop boundary.
            float scaleN = s.scaleFrom + alpha * (s.scaleTo - s.scaleFrom);

            // The de-emphasis runs on the RAW filter output, BEFORE the loudness
            // scale. Two reasons, both measured:
            //
            //  - The chain history must store a scale-free signal so the frame
            //    energy the next hop measures is the filter's own gain. Storing
            //    the SCALED output makes the scale a feedback loop with no
            //    restoring force at ratio 1.0 (every constant scale is a fixed
            //    point), measured the applied scale swinging 0.07 to 5.3.
            //  - Dividing the de-emphasized output back by the scale is NOT
            //    scale-free when the scale ramps: the integrator's feedback term
            //    picks up the ratio scale[n-1]/scale[n], so its pole becomes
            //    0.97 * that ratio. When the scale ramps down steeply (the gain
            //    at r=1.5 is 26.8, so the scale ramps 1 -> 0.037 over one hop),
            //    the ratio reaches 1.06 and the pole is 1.028 - the integrator
            //    diverges. Measured 8097 explosion restores and the scale
            //    swinging 0.001 to 57.7.
            //
            // Running the de-emphasis on the raw output keeps the pole at the
            // constant 0.97, keeps the stored history scale-free, and at ratio 1.0
            // the induction y_raw[n] = xpe[n] makes the de-emphasis give back the
            // raw input exactly, so the scale is 1 and the chain is the identity.
            if (pre)
            {
                y = y + preCoef * s.deEmphZ;
                s.deEmphZ = y;
            }

            // Record the integrator state in the F+1 ring: the next hop's
            // simulation seeds on the state this sample's successor reads, and the
            // frame it runs starts F samples back, which is one slot ahead here.
            s.deH[static_cast<size_t> (s.dePos)] = y;
            s.dePos = (s.dePos + 1) % (frameSamples + 1);

            // The peak the clamp has to bound is the peak of the SAME signal the
            // scale multiplies, which is the de-emphasized output. Recording the
            // raw filter output here measured a peak 33x smaller (the integrator's
            // DC gain is 33), so the bound was 33x too small, the clamp set a
            // scale 33x too large, and the output peak hit 3.07 against a
            // reference peak of 0.82.
            s.yPeakHop = juce::jmax (s.yPeakHop, std::abs (y));

            // The chain energy the scale law is built from is measured on the RAW
            // filter output, before the scale. Measuring the scaled sample makes the
            // law a cubic fixed point: emaChain = sum(scale^2 * y^2), so the target
            // sqrt(in / emaChain) converges to (in / sum y^2)^(1/3), not ^(1/2), and
            // landed 1.216x too quiet at r=1.5 (region voiced in = 129.84,
            // out = 87.89). Measuring the raw sample makes it exact: sum(out^2) =
            // scale^2 * sum(chain^2) = (sum in / sum chain^2) * sum chain^2 = sum in.
            const float chainRaw = y;

            // A per-sample limiter, not a clip. The energy law overshoots on the hops
            // where the chain rings, because the one-hop-delayed emaChain is stale:
            // idealOut 184.88 against in 129.84 at r=1.5. The +-peakLimit clip turned
            // that overshoot into distortion and ate the energy, measured
            // clampLoss = out / idealOut = 0.3729 (63% of the delivered energy gone).
            // Scaling by peakLimit / runningPeak keeps |out| <= peakLimit by
            // construction, and leaves every sample below the ceiling untouched.
            s.runPeak = juce::jmax (s.runPeak * 0.9995f, std::abs (y));
            const float headroom = (s.runPeak > 1.0e-4f)
                                  ? s.peakLimit / s.runPeak
                                  : 1.0f;
            scaleN = juce::jmin (scaleN, headroom);

            // The scale is applied to the OUTPUT sample only, after the
            // integrator, so it cannot move the integrator's pole.
            float yOut = scaleN * y;

            if (!std::isfinite (yOut))
            {
                yOut = 0.0f;
                ++explosionCount;
            }

            maxAbsY = juce::jmax (maxAbsY, std::abs (yOut));

            const float chainOut = yOut;

            const float leavingChain = s.chainH[static_cast<size_t> (nxt)];
            s.sumChain2 += static_cast<double> (chainRaw) * static_cast<double> (chainRaw)
                         - static_cast<double> (leavingChain) * static_cast<double> (leavingChain);
            s.chainH[static_cast<size_t> (cur)] = chainRaw;

            s.totalInEnergy    += static_cast<double> (xRaw) * static_cast<double> (xRaw);
            s.totalChainEnergy += static_cast<double> (chainRaw) * static_cast<double> (chainRaw);
            // Long-window loudness averages, the window the +-2% RMS criterion is
            // defined on. The 24 ms frame is the right window for the FORMANT
            // analysis and the wrong one for the loudness target.
            const double inEnergy  = static_cast<double> (xRaw) * static_cast<double> (xRaw);
            const double chainEnergy = static_cast<double> (chainRaw) * static_cast<double> (chainRaw);
            if (s.emaActive)
            {
                // Leaky, not cumulative. A running sum is poisoned by the startup:
                // the first hops run on an un-converged scale, the chain explodes and
                // is clamped at +-10, so a single hop adds ~1e6 to the sum while the
                // steady hops add ~9 each. Measured at r=2.0: emaChain 2.26e6 against
                // per-hop frame sums of 0.006..8.8, so the scale stayed 0.0085 where
                // the hop needed 0.03..0.12. A leaky average forgets the transient.
                //
                // Both sides are weighted by the gate, because the gate is what the
                // listener hears. Per-hop rows at r=1.5 showed REALsumChain2 = 692 on
                // a hop whose gateMix was 1.0 (the filter ringing after an onset), and
                // the unweighted average then drove the scale to 0.039 on the following
                // voiced hops, where the hop itself needed 0.28. Weighting both sides
                // keeps the identity exact over the samples that pass: sum (g*scale*y)^2
                // = scale^2 * sum g^2 y^2 = (sum g^2 in / sum g^2 y^2) * sum g^2 y^2
                // = sum g^2 in.
                const double g = static_cast<double> (s.gateMix);
                const double gw = g * g;
                s.emaIn    += (gw * inEnergy    - s.emaIn)    * static_cast<double> (emaBeta);
                s.emaChain += (gw * chainEnergy - s.emaChain) * static_cast<double> (emaBeta);
            }
            const float finalOut = s.gateMix * chainOut + (1.0f - s.gateMix) * xRaw;
            if (s.emaActive)
            {
                // The AGC feedback side: the power the listener actually receives.
                s.emaOut += (static_cast<double> (finalOut) * static_cast<double> (finalOut)
                             - s.emaOut) * static_cast<double> (emaBeta);
            }
            s.totalOutEnergy   += static_cast<double> (chainOut) * static_cast<double> (chainOut);
            s.totalRawOutEnergy += static_cast<double> (finalOut) * static_cast<double> (finalOut);
            if (s.totalSamples >= s.regionStart)
            {
                s.regionIn    += inEnergy;
                s.regionChain += chainEnergy;
                s.regionOut   += static_cast<double> (chainOut) * static_cast<double> (chainOut);
                s.regionFinal += static_cast<double> (finalOut) * static_cast<double> (finalOut);
                s.regionCross += static_cast<double> (chainOut) * static_cast<double> (xRaw);
                s.regionMix   += static_cast<double> (s.gateMix);
                const double g = static_cast<double> (s.gateMix);
                s.regionG2Out += g * g * static_cast<double> (chainOut) * static_cast<double> (chainOut);
                s.regionG1In  += (1.0 - g) * (1.0 - g) * inEnergy;
                s.regionGx    += g * (1.0 - g) * static_cast<double> (chainOut) * static_cast<double> (xRaw);
                if (s.emaActive)
                {
                    s.regionInActive    += inEnergy;
                    s.regionChainActive += chainEnergy;
                    s.regionOutActive   += static_cast<double> (chainOut) * static_cast<double> (chainOut);
                    const float ideal = scaleN * y;
                    if (std::isfinite (ideal))
                        s.regionIdealOut += static_cast<double> (ideal) * static_cast<double> (ideal);
                    ++s.regionActiveSamples;
                }
                ++s.regionSamples;
            }
            ++s.totalSamples;

            out[i] = finalOut;

            ++s.rampPos;
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
            initChannel (s);
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
