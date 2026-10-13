// LpcFormantPreserver.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace ovtdsp
{
    class LpcFormantPreserver
    {
    public:
        enum class Mode
        {
            C0 = 0,       // P1: plain LPC cross-synthesis
            C1Hybrid = 1  // P2: pre-emphasis + LPC interpolation + hybrid fallback
        };

        explicit LpcFormantPreserver (int order = 16);
        ~LpcFormantPreserver();

        void prepare (double sampleRate, int maxBlockSize);
        void reset();
        void setOrder (int order);

        /// Analysis hop in milliseconds. Default 10 ms (100 coefficient
        /// resolutions per second). Harmony voices can double it to 20 ms to
        /// halve the analysis cost — the perceived timbre is dominated by the
        /// lead. Takes effect on the next prepare().
        void setHopMs (double hopMs);

        /// Fixed analysis frame length (24 ms) in samples, after prepare().
        int getFrameSamples() const { return frameSamples; }
        int getHopSamples() const { return hopSamples; }

        /// The framing is causal: coefficients are computed from the frame
        /// ending at the CURRENT sample, and applied sample-by-sample from that
        /// sample on. The module adds no latency, so the plugin's
        /// setLatencySamples contract is untouched.
        int getLatencySamples() const { return 0; }

        /// Diagnostics of the last coefficient update, for tests and the bench:
        /// the raw residual ratio gRef/gIn the gate was tested against, the
        /// clamped loudness scale actually applied, and how many hop updates
        /// the instance has performed.
        float getLastResGain() const { return lastResGain; }
        float getLastScale() const { return lastScale; }
        int getHopUpdates() const { return hopUpdates; }
        bool isGatePassive() const { return gatePassive; }

        /// Aggregates over the LEAD-channel hops where the LPC path was active
        /// (the gate did not take the passthrough branch): the mean residual
        /// ratio and the mean applied scale.
        int getActiveHopUpdates() const { return activeHopUpdates; }
        float getMeanResGainActive() const
        {
            return activeHopUpdates > 0 ? static_cast<float> (sumResGainActive / activeHopUpdates) : 0.0f;
        }
        float getMeanScaleActive() const
        {
            return activeHopUpdates > 0 ? static_cast<float> (sumScaleActive / activeHopUpdates) : 0.0f;
        }

        /// Spread of the residual ratio over ALL lead-channel hops: the
        /// hop-to-hop stability of the normalization, which is what the hybrid
        /// gate has to absorb.
        float getMinResGain() const { return minResGain; }
        float getMaxResGain() const { return maxResGain; }
        int getExplosionCount() const { return explosionCount; }

        /// The two prediction-error energies of the last hop, in the
        /// pre-emphasized analysis domain: gRef for the reference frame, gIn
        /// for the whitening-input frame. The scale is built from gIn, so these
        /// two numbers say whether the loudness target is stable.
        float getLastErrRef() const { return lastErrRef; }
        float getLastErrIn() const { return lastErrIn; }
        float getMinErrIn() const { return minErrIn; }
        float getMaxErrIn() const { return maxErrIn; }

        /// Squared impulse-response energy of the re-color filter 1/A_orig(z)
        /// at the last hop, and the raw reference frame energy it is normalized
        /// against.
        float getLastEH() const { return lastEH; }
        float getLastRefEnergy() const { return lastRefEnergy; }

        /// Peak |y| the chain produced before the guard, peak |e| of the whitened
        /// signal, and peak |yRec| of the re-color history. These three say
        /// whether the scale clamp is the binding constraint or the recurrence
        /// itself is ringing.
        float getMaxAbsY() const { return maxAbsY; }
        float getMaxAbsE() const { return maxAbsE; }
        float getMaxAbsYRec() const { return maxAbsYRec; }

        /// The largest |y_raw[n] - xpe[n]| the chain produced, where y_raw is the
        /// re-color output BEFORE the loudness scale. At ratio 1.0 the whitening
        /// and the re-coloring run on the same coefficients, so the induction
        /// y_raw[n] = xpe[n] must hold exactly and this number must be zero. It
        /// is the direct test of the induction the whole chain relies on: the
        /// re-color recurrence reads P history samples, and the induction only
        /// holds if the coefficients applied to those samples match the ones
        /// applied to the current one.
        float getMaxInductionErr() const { return maxInductionErr; }
        /// Set the bandwidth-expansion factor before prepare(). It is the Q of the
        /// re-color filter, so the bench sweeps it to find the numerical floor.
        void setBwLambda (float lambda) { bwLambda = lambda; }

        /// Per-hop diagnostic ring: the raw reference frame energy, the scale-free
        /// chain frame energy, the scale the law asked for and the scale applied,
        /// for the last 16 lead-channel hops. The scale is the ratio of the first
        /// two, so a hop where the scale is wrong is a hop where one of the two
        /// energies is not what the induction says it should be.
        const std::vector<float>& getDiagRefEnergy() const { return diagRefEnergy; }
        const std::vector<float>& getDiagChainEnergy() const { return diagChainEnergy; }
        const std::vector<float>& getDiagScale() const { return diagScale; }
        const std::vector<float>& getDiagScaleApplied() const { return diagScaleApplied; }

        /// The FIRST 16 lead-channel hops, same four columns: the onset is where
        /// the measured scale goes wrong, because the de-emphasis integrator
        /// (DC gain 33) starts from zero state and the analysis frame is mostly
        /// silence.
        const std::vector<float>& getDiagFirstRefEnergy() const { return diagFirstRefEnergy; }
        const std::vector<float>& getDiagFirstChainEnergy() const { return diagFirstChainEnergy; }
        const std::vector<float>& getDiagFirstScale() const { return diagFirstScale; }
        const std::vector<float>& getDiagFirstScaleApplied() const { return diagFirstScaleApplied; }
        const std::vector<float>& getDiagFirstRefPeak() const { return diagFirstRefPeak; }
        const std::vector<float>& getDiagFirstBound() const { return diagFirstBound; }
        const std::vector<float>& getDiagFirstPeakScale() const { return diagFirstPeakScale; }
        const std::vector<float>& getDiagFirstEPeak() const { return diagFirstEPeak; }
        const std::vector<float>& getDiagFirstYPeak() const { return diagFirstYPeak; }
        const std::vector<float>& getDiagFirstSumOut2() const { return diagFirstSumOut2; }
        const std::vector<float>& getDiagFirstSumRef2() const { return diagFirstSumRef2; }
        /// The REAL sliding chain energy over the frame at the hop boundary, next to
        /// the simulated one. The simulation runs one coefficient set over the whole
        /// frame, the chain ramps and updates every hop, so at r != 1 the two differ
        /// and the measured scale is built on the simulation.
        const std::vector<float>& getDiagFirstSumChain2() const { return diagFirstSumChain2; }
        const std::vector<float>& getDiagFirstEmaIn() const { return diagFirstEmaIn; }
        const std::vector<float>& getDiagFirstEmaChain() const { return diagFirstEmaChain; }
        /// The last 16 hop measurements, the steady region the +-2% RMS criterion
        /// is judged on once the long-window averages have converged.
        const std::vector<float>& getDiagLastScale() const { return diagLastScale; }
        const std::vector<float>& getDiagLastScaleApplied() const { return diagLastScaleApplied; }
        const std::vector<float>& getDiagLastEmaIn() const { return diagLastEmaIn; }
        const std::vector<float>& getDiagLastEmaChain() const { return diagLastEmaChain; }
        const std::vector<float>& getDiagLastOutRms() const { return diagLastOutRms; }
        const std::vector<float>& getDiagLastGateMix() const { return diagLastGateMix; }
        const std::vector<float>& getDiagLastSumChain2() const { return diagLastSumChain2; }
        const std::vector<float>& getDiagLastChainSim() const { return diagLastChainSim; }
        const std::vector<float>& getDiagFirstSimDeLast() const { return diagFirstSimDeLast; }
        const std::vector<float>& getDiagFirstXRawLast() const { return diagFirstXRawLast; }
        const std::vector<float>& getDiagFirstSimErr2() const { return diagFirstSimErr2; }
        const std::vector<float>& getDiagFirstSimMaxErr() const { return diagFirstSimMaxErr; }
        const std::vector<float>& getDiagFirstSimYMaxErr() const { return diagFirstSimYMaxErr; }
        const std::vector<float>& getDiagFirstSimHistErr() const { return diagFirstSimHistErr; }
        const std::vector<float>& getDiagFirstSimHeadX() const { return diagFirstSimHeadX; }
        const std::vector<float>& getDiagFirstSimHeadY() const { return diagFirstSimHeadY; }
        const std::vector<float>& getDiagFirstSimHeadDe() const { return diagFirstSimHeadDe; }
        const std::vector<float>& getDiagFirstSimHeadOut() const { return diagFirstSimHeadOut; }
        const std::vector<float>& getDiagFirstSimHeadInit() const { return diagFirstSimHeadInit; }

        /// Statistics of the scale ACTUALLY applied at each hop, after the peak
        /// clamp and the hard clamp, over all lead-channel hops. The pre-clamp
        /// scale (lastResGain) says what the loudness law asked for; these say
        /// what the chain ran with, and the two diverge when the peak clamp
        /// binds - which is what makes the measured-scale loop oscillate.
        float getMinScaleApplied() const { return minScaleApplied; }
        float getMaxScaleApplied() const { return maxScaleApplied; }
        float getMeanScaleApplied() const
        {
            return hopUpdates > 0 ? static_cast<float> (sumScaleApplied / hopUpdates) : 0.0f;
        }

        /// Applies LPC formant preservation IN PLACE on `out`.
        /// @param out       post-pitch-shift signal to be re-colored (modified in place).
        /// @param reference pre-pitch-shift signal carrying the TARGET formants.
        /// @param ratio     transposition ratio (1.0 = passthrough, used only for
        ///                  optional logging / future adaptive behaviour).
        /// @param mode      C0 (P1) or C1Hybrid (P2).
        void process (juce::AudioBuffer<float>& out,
                      const juce::AudioBuffer<float>& reference,
                      float ratio,
                      Mode mode);

    private:
        struct ChannelState
        {
            // Circular analysis history, sized to frameSamples: the last
            // frameSamples samples of the raw output, of the (optionally
            // pre-emphasized) whitening input, of the pre-de-emphasis
            // re-color output, of the post-de-emphasis chain output, and of the
            // reference.
            std::vector<float> outRaw;
            std::vector<float> xpe;
            std::vector<float> yRec;
            std::vector<float> chainH;   // chain output, what the measured scale is matched to
            std::vector<float> refH;
            std::vector<float> refRawH;  // raw reference, for the sliding RMS only

            int pos = 0;          // write index into the circular buffers
            int hopCount = 0;     // samples since the last coefficient update
            int rampPos = 0;      // samples since the last hop boundary, drives the coefficient ramp
            double sumOut2 = 0.0; // sliding sum of outRaw^2 over the frame
            double sumRef2 = 0.0; // sliding sum of refRawH^2 over the frame
            double sumOut2Pre = 0.0; // sliding sum of xpe^2 (the chain domain)
            double sumRef2Pre = 0.0; // sliding sum of refH^2 (the chain domain)
            double sumChain2 = 0.0;  // sliding sum of chainH^2 (the measured chain output)

            bool gatePass = true; // hybrid fallback: passthrough branch active
            float scale = 1.0f;   // per-sample loudness scale, ramped across the hop
            float scaleFrom = 1.0f; // scale at the start of the hop ramp
            float scaleTo = 1.0f;   // scale at the end of the hop ramp

            // Coefficient ramp endpoints, in REFLECTION-coefficient space. The
            // coefficients applied at sample n are the convex combination
            // (1 - alpha) kFrom + alpha kTo with alpha = rampPos / hopSamples,
            // rebuilt to direct form every sample. Reflection coefficients are
            // bounded by +-0.999 by construction, so a convex combination of two
            // sets stays inside the stability region: this is the ONLY space in
            // which a time-varying all-pole interpolation is guaranteed stable.
            std::vector<float> kFromOrig, kToOrig;       // reference filter ramp
            std::vector<float> kFromShifted, kToShifted; // whitening filter ramp
            std::vector<float> kCurOrig, kCurShifted;    // interpolated, per sample
            std::vector<float> bdA, bdB, bdC, bdD;       // Levinson ping-pong scratch

            float preEmphZ = 0.0f;           // pre-emphasis one-pole state (out)
            float refPreZ = 0.0f;            // pre-emphasis one-pole state (reference)
            float deEmphZ = 0.0f;            // de-emphasis one-pole state
            // The de-emphasis state ring, F+1 slots. The analysis frame covers
            // exactly F slots, so the state the frame's OLDEST sample reads lives
            // in the same ring slot as the newest sample and gets overwritten.
            // One extra slot keeps it: after sample n is written, slot dePos holds
            // d[n-F], the seed a simulation of the frame n-F+1..n has to start on.
            // Seeding with d[n] instead made the simulated integrator carry a
            // constant offset that decays as 0.97^j over the whole frame, so the
            // measured scale sat at 0.963 at ratio 1.0 where the induction makes
            // the chain the identity and the scale must be exactly 1.
            std::vector<float> deH;
            int dePos = 0;

            // Per-hop ceiling for the scaled output, set to twice the input's own
            // peak over the analysis frame. The explosion guard used a fixed +-10,
            // which is 33x the signal level (input peak 0.3000): a clamped sample
            // carried the whole RMS ratio, measured peak out = 10.0000 and
            // RMS(out/in) = 5.42 in the first second of the converged window at
            // r=1.5. Clipping at the level the plugin is fed bounds the energy the
            // guard can inject.
            float peakLimit = 0.3f;

            // Slowly decaying running peak of the de-emphasized filter output, the
            // input to the per-sample headroom limiter.
            float runPeak = 0.0f;
            // Monotonic energy totals, so the RMS deficit can be attributed to one
            // stage instead of guessed at.
            double totalInEnergy = 0.0;       // sum xRaw^2
            double totalChainEnergy = 0.0;    // sum (raw filter output)^2, before the scale
            double totalOutEnergy = 0.0;      // sum (scale * raw output)^2, after the clamps
            double totalRawOutEnergy = 0.0;   // sum gateMix*chainOut + (1-gateMix)*xRaw squared
            int totalSamples = 0;
            // Long-window energy averages for the loudness scale. The 24 ms frame is
            // the right window for the FORMANT analysis and the wrong one for the
            // loudness target: the chain's REAL energy over one frame swung 13 to
            // 1357 between consecutive hops at r=1.5 (71x), so a scale read from a
            // frame lagged by two hops landed 43x above the global ideal of 0.0088
            // and the measured RMS ratio was 8.889. The scale is the ratio of two
            // exponential moving averages with a 0.5 s time constant, the window
            // the +-2% RMS criterion is defined on.
            double emaIn = 0.0;      // exponential average of xRaw^2
            double emaChain = 0.0;   // exponential average of (raw filter output)^2

            // Energy of the DELIVERED output, the feedback side of the AGC loop. The
            // direct formula scale = sqrt(emaIn / emaChain) has a cubic fixed point
            // when the measured side is the scaled sample; the multiplicative update
            // scale(k+1) = scale(k) * sqrt(emaIn / emaOut) converges to the exact
            // match, because its fixed point is emaOut = scale^2 * emaChain = emaIn.
            double emaOut = 0.0;
            // The averages must not start at zero. With emaChain = 0 the ratio guard
            // returns 1.0, so the first hops run with scale 1.0 on a chain already
            // ~200x the input: that is where the explosions and the global RMS of
            // 1.1196 came from, not from the steady region (which measured final/in
            // = 0.6172). The first ACTIVE hop seeds both averages from its own frame
            // sums so the very first scale is the true ratio.
            bool emaSeeded = false;
            // False while the input frame is below hybridRmsFloor. The re-color filter
            // keeps RINGING after the excitation stops: measured at r=1.5, 99.99% of
            // the chain's energy over the last second sat on samples the gate had
            // already closed (region g^2*chainOut^2 = 27.60 of region chainOut^2 =
            // 1160.45). Averaging that ringing into emaChain drove the scale to
            // 0.0084, 250x below the ratio the voiced samples need, and the voiced
            // output landed at 0.45x the input. The loudness average is only
            // meaningful on driven samples, so it is frozen while the input is silent.
            bool emaActive = true;
            // Totals restricted to samples at or after regionStart, so the steady
            // region can be read apart from the startup transient.
            int regionStart = 0;
            double regionIn = 0.0;
            double regionChain = 0.0;
            double regionOut = 0.0;
            double regionFinal = 0.0;
            double regionCross = 0.0;
            double regionMix = 0.0;
            // The same totals restricted to the samples the gate keeps active, i.e.
            // the voiced signal alone, which is what the +-2% RMS criterion measures.
            double regionInActive = 0.0;
            double regionChainActive = 0.0;
            double regionOutActive = 0.0;
            // Energy the scale law WOULD deliver if the peak clamp never fired:
            // sum (scale * raw chain)^2 over voiced samples. Comparing it with
            // regionOutActive measures exactly how much the +-peakLimit clamp eats.
            double regionIdealOut = 0.0;
            int regionActiveSamples = 0;
            double regionG2Out = 0.0;
            double regionG1In = 0.0;
            double regionGx = 0.0;
            int regionSamples = 0;
            float ePeakHop = 0.0f;           // peak |e| of the whitened signal since the last hop
            float yPeakHop = 0.0f;           // peak |y| of the raw filter output in the PREVIOUS hop
            float gateMix = 0.0f;            // cross-fade 0 = raw input, 1 = chain output
        };

        std::vector<ChannelState> channels;

    public:
        /// Monotonic totals over the whole stream. The scale is measured as
        /// sqrt(input frame energy / chain frame energy) over the 1058-sample
        /// frame, applied to the 441-sample hop, and ramped across that hop. The
        /// RMS ratio sits at 0.72-0.75 at r=1.5 for every bwLambda, so the deficit
        /// is not the filter Q. Comparing the totals says whether the loss is the
        /// scale law, the ramp, the floor, or the peak clamp.
        double getTotalInEnergy() const { return channels.empty () ? 0.0 : channels[0].totalInEnergy; }
        double getTotalChainEnergy() const { return channels.empty () ? 0.0 : channels[0].totalChainEnergy; }
        double getTotalOutEnergy() const { return channels.empty () ? 0.0 : channels[0].totalOutEnergy; }
        double getTotalRawOutEnergy() const { return channels.empty () ? 0.0 : channels[0].totalRawOutEnergy; }
        int getTotalSamples() const { return channels.empty () ? 0 : channels[0].totalSamples; }
        double getEmaIn() const { return channels.empty () ? 0.0 : channels[0].emaIn; }
        double getEmaChain() const { return channels.empty () ? 0.0 : channels[0].emaChain; }
        /// Restrict the region totals to samples at or after this index, so the
        /// steady region is measured apart from the startup transient.
        void setRegionStart (int sampleIndex)
        {
            for (auto& s : channels)
                s.regionStart = sampleIndex;
        }
        double getRegionIn() const { return channels.empty () ? 0.0 : channels[0].regionIn; }
        double getRegionChain() const { return channels.empty () ? 0.0 : channels[0].regionChain; }
        double getRegionOut() const { return channels.empty () ? 0.0 : channels[0].regionOut; }
        double getRegionFinal() const { return channels.empty () ? 0.0 : channels[0].regionFinal; }
        /// Sum of chainOut * xRaw over the region. The gate crossfades these two
        /// signals, so this is the cross term of the final energy: a negative value
        /// means the chain output is anti-phase with the input and the fade cancels
        /// level instead of blending it.
        double getRegionCross() const { return channels.empty () ? 0.0 : channels[0].regionCross; }
        /// Sum of the gate mix over the region, so the mean mix is readable: the
        /// energy equation final = g^2*out + (1-g)^2*in + 2g(1-g)*cross only closes
        /// with the g the samples actually used.
        double getRegionMix() const { return channels.empty () ? 0.0 : channels[0].regionMix; }
        /// The exact decomposition of the region's final energy:
        /// final = sum g^2*chainOut^2 + sum (1-g)^2*xRaw^2 + 2*sum g(1-g)*chainOut*xRaw.
        /// A mean g does NOT close this identity, because g is weighted per sample.
        double getRegionG2Out() const { return channels.empty () ? 0.0 : channels[0].regionG2Out; }
        double getRegionG1In() const { return channels.empty () ? 0.0 : channels[0].regionG1In; }
        double getRegionGx() const { return channels.empty () ? 0.0 : channels[0].regionGx; }
        /// Voiced-only totals: input, chain and scaled chain over the samples the
        /// gate keeps active. The ideal loudness scale is sqrt(in/chain) restricted
        /// to these samples, not to the whole region.
        double getRegionInActive() const { return channels.empty () ? 0.0 : channels[0].regionInActive; }
        double getRegionChainActive() const { return channels.empty () ? 0.0 : channels[0].regionChainActive; }
        double getRegionOutActive() const { return channels.empty () ? 0.0 : channels[0].regionOutActive; }
        double getRegionIdealOut() const { return channels.empty () ? 0.0 : channels[0].regionIdealOut; }
        int getRegionActiveSamples() const { return channels.empty () ? 0 : channels[0].regionActiveSamples; }
        int getRegionSamples() const { return channels.empty () ? 0 : channels[0].regionSamples; }

    private:
        int order = 16;
        double sampleRate = 44100.0;

        // Fixed framing: 24 ms analysis frame, 10 ms hop. The analysis quality
        // is a property of these constants, NOT of the DAW block size.
        static constexpr double frameMs = 24.0;
        double hopMs = 10.0;
        int frameSamples = 1058;
        int hopSamples = 441;

        // Per-sample decay of the long-window loudness averages (0.5 s time
        // constant), set in prepare() from the sample rate.
        float emaBeta = 0.0f;

        std::vector<float> hann;     // precomputed window, size frameSamples
        std::vector<float> frameRef; // windowed reference frame scratch
        std::vector<float> frameX;   // windowed whitening-input frame scratch

        // Impulse response of the re-color filter 1/A_orig(z), used to measure
        // the energy gain the cross-synthesis applies to the whitened residual.
        std::vector<float> impulse;
        /// Scratch for the per-hop chain simulation that measures the gain the
        /// loudness scale has to cancel. Sized to frameSamples + order in
        /// prepare(): the first `order` entries hold the chain's real history,
        /// the rest the frame.
        std::vector<float> simX;
        std::vector<float> simY;
        static constexpr int impulseLength = 128;

        // Pre-emphasis coefficient (P2). Boosts high frequencies before LPC so
        // the all-pole model captures the spectral tilt more accurately.
        float preCoef = 0.97f;

        // Bandwidth expansion for LPC stability: a_k *= lambda^k.
        //
        // It is also the Q of the re-color filter, and the Q is what sets the
        // numerical floor of the chain. The whitening cancels the signal down to
        // ~1e-3, so the residual carries ~1e-6 of float rounding; the re-color
        // filter amplifies that rounding by its impulse-response energy sqrt(e_h),
        // measured 53.6 at lambda 0.90, and the de-emphasis integrator (DC gain
        // 33) integrates it again. That is the ringing measured at ratio 1.0,
        // where the chain is supposed to be the identity: max |y_raw - xpe| =
        // 4.2e-5, and the simulated de-emphasis output deviating from the raw
        // input by up to 0.23 against a signal peak of 0.29.
        //
        // Settable so the bench can sweep it; the default is the shipped value.
        float bwLambda = 0.60f;

        // Hybrid fallback thresholds (applied to BOTH P1 and P2 for robustness
        // on real-world signals: fricatives, breath, consonants make the LPC
        // residual energy wildly variable, so a too-permissive gain can mute
        // the output or push it into clipping).
        //
        // The band is on the MEASURED loudness target (the scale that makes the
        // output frame energy equal the reference frame energy). That scale is
        // NOT centered on 1: the re-color filter 1/A_orig(z) has a large energy
        // gain, so the loudness-matching scale sits far below 1. Measured over the
        // whole stream at r=1.5 (block 256): input energy 427.37, raw chain output
        // energy 5 489 408, so the chain is 113.3x the input and the ideal scale is
        // 0.0088. At r=2.0 the cross-synthesis gain is 2322, so the ideal scale is
        // 4.3e-4. A band of 20 CLAMPED both scales up to 0.05, which made the
        // output 5.7x too loud AND tripped the `scale < 1/20` gate, so the gate
        // ran the passthrough branch and the output was the raw input times 0.726.
        // The band must cover the cross-synthesis gain, not the unity band.
        static constexpr float hybridRmsFloor = 5.0e-3f; // below this -> passthrough
        static constexpr float hybridMaxScale = 4000.0f;  // gain explode -> passthrough

        // Cross-fade length for the hybrid gate. The gate switches between the
        // raw pitch-shifted input and the re-colored signal, and the two differ
        // by far more than the signal's own sample-to-sample jump, so a hard
        // switch is a step the listener hears as a click. 5 ms is short enough to
        // keep the formant correction tight and long enough to be inaudible.
        // The gate crossfade fades the chain against the raw input. The two signals
        // are UNCORRELATED (measured corr 0.035), so a fade CANCELS level: at r=2.0
        // the region measured g^2*chainOut^2 = 2.45 out of chainOut^2 = 206.6, i.e.
        // 98% of the chain energy landed on samples where the gate was ramping. The
        // loudest chain samples are the syllable attacks, exactly where the gate is
        // opening, so the fade has to be short against the 10 ms attack.
        static constexpr double gateFadeMs = 1.0;
        int gateFadeSamples = 220; // set in prepare() from gateFadeMs

        // Diagnostics snapshot of the last hop update (channel 0).
        float lastResGain = 1.0f;
        float lastScale = 1.0f;
        int hopUpdates = 0;
        bool gatePassive = true;
        int activeHopUpdates = 0;
        double sumResGainActive = 0.0;
        double sumScaleActive = 0.0;
        float minResGain = 1.0e9f;
        float maxResGain = 0.0f;
        int explosionCount = 0;
        float lastErrRef = 0.0f;
        float lastErrIn = 0.0f;
        float minErrIn = 1.0e9f;
        float maxErrIn = 0.0f;
        float lastEH = 0.0f;
        /// The hop's LPC residual and impulse-response energy, handed from
        /// updateHop to measureScale for the diagnostics.
        float hopErrRef = 0.0f;
        float hopErrIn  = 0.0f;
        float hopEH     = 0.0f;
        float lastRefEnergy = 0.0f;
        float maxAbsY = 0.0f;
        float maxAbsE = 0.0f;
        float maxAbsYRec = 0.0f;
        float maxInductionErr = 0.0f;
        std::vector<float> diagRefEnergy;
        std::vector<float> diagChainEnergy;
        std::vector<float> diagScale;
        std::vector<float> diagScaleApplied;
        std::vector<float> diagFirstRefEnergy;
        std::vector<float> diagFirstChainEnergy;
        std::vector<float> diagFirstScale;
        std::vector<float> diagFirstScaleApplied;
        std::vector<float> diagFirstRefPeak;
        std::vector<float> diagFirstBound;
        std::vector<float> diagFirstPeakScale;
        std::vector<float> diagFirstEPeak;
        std::vector<float> diagFirstYPeak;
        /// The running raw input energy over the frame, recorded next to the
        /// simulated chain energy so the identity at ratio 1.0 can be checked:
        /// the simulation must land on the raw input energy there.
        std::vector<float> diagFirstSumOut2;
        std::vector<float> diagFirstSumRef2;
        std::vector<float> diagFirstSumChain2;
        std::vector<float> diagFirstEmaIn;
        std::vector<float> diagFirstEmaChain;
        std::vector<float> diagLastScale;
        std::vector<float> diagLastScaleApplied;
        std::vector<float> diagLastEmaIn;
        std::vector<float> diagLastEmaChain;
        std::vector<float> diagLastOutRms;
        std::vector<float> diagLastGateMix;
        std::vector<float> diagLastSumChain2;
        std::vector<float> diagLastChainSim;
        /// The simulated de-emphasis output at the newest sample of the frame,
        /// next to the real raw input there, so the identity can be checked at a
        /// single sample rather than as an energy.
        std::vector<float> diagFirstSimDeLast;
        std::vector<float> diagFirstXRawLast;
        /// Sum over the frame of (simulated de-emphasis output - raw input)^2,
        /// the per-hop deviation of the simulation from the identity at ratio 1.0.
        std::vector<float> diagFirstSimErr2;
        /// Largest |simulated de-emphasis output - raw input| over the frame.
        std::vector<float> diagFirstSimMaxErr;
        /// Largest |simulated raw filter output - xpe| over the frame, the
        /// simulation's own induction error.
        std::vector<float> diagFirstSimYMaxErr;
        /// Largest |yRec - xpe| over the P history samples the simulation starts on.
        std::vector<float> diagFirstSimHistErr;
        // Per-sample head of the first simulated hop: simX, y, simDe, outRaw.
        std::vector<float> diagFirstSimHeadX;
        std::vector<float> diagFirstSimHeadY;
        std::vector<float> diagFirstSimHeadDe;
        std::vector<float> diagFirstSimHeadOut;
        std::vector<float> diagFirstSimHeadInit; // deEmphZ, xRaw[cur]
        float minScaleApplied = 1.0e30f;
        float maxScaleApplied = 0.0f;
        double sumScaleApplied = 0.0;

        // Per-channel scratch buffers reused across process() calls.
        std::vector<float> autocorrRef;
        std::vector<float> autocorrIn;
        std::vector<float> aOrig;
        std::vector<float> aShifted;
        std::vector<float> kOrig;    // reflection coefficients of the reference frame
        std::vector<float> kShifted; // reflection coefficients of the input frame

        // xBackup holds the raw input at processChannel entry so the block
        // can be restored verbatim if the LPC chain explodes (instability
        // guard). Sized to the DAW block size.
        std::vector<float> xBackup;

        // Reset one channel's circular history, ramp state and sliding sums.
        // Shared by prepare() and reset() so the two paths cannot drift apart.
        void initChannel (ChannelState& s);

        // Computes LPC coefficients a[1..P] (size P) for frame x[0..n) via the
        // autocorrelation method + Levinson-Durbin. Fills K[1..P] with the
        // reflection coefficients (bounded by +-0.999, the stable interpolation
        // space). Returns the prediction error energy.
        static float computeLpc (const float* x, int n, int P,
                                  std::vector<float>& a, std::vector<float>& R,
                                  std::vector<float>& K);

        // Allocation-free version of the same recursion, ping-ponging between two
        // preallocated buffers, for the per-sample coefficient ramp. Returns the
        // pointer that holds the result (one of the two buffers). Cost O(P^2/2),
        // 128 ops at P=16, twice per sample.
        static float* buildDirectFromRefl (const float* K, int P,
                                           float* bufA, float* bufB);

        // Recompute the coefficients at a hop boundary from the circular
        // history ending at `cur`, and evaluate the hybrid gate. `isLead`
        // marks channel 0, whose hop is the one the diagnostics getters report.
        void updateHop (ChannelState& s, int cur, Mode mode, bool isLead);
        /// The per-hop chain simulation that yields the loudness scale. Runs AFTER
        /// the current sample's `yRec` is written, so the P history samples the
        /// simulation seeds are the values the chain will actually read when it
        /// runs the next sample. Seeding them inside updateHop read the value
        /// written F samples earlier, one full frame stale. `mode` is needed to
        /// rebuild the de-emphasis state of the current sample: this runs before
        /// that sample is de-emphasized, so `s.deEmphZ` is one integrator step
        /// behind the value the frame's first sample will read.
        void measureScale (ChannelState& s, int cur, Mode mode, bool isLead);

        void processChannel (float* out, const float* ref, int n,
                             ChannelState& s, Mode mode, bool isLead);
    };
}


