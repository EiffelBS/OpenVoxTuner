// PitchShifter.cpp
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include "PitchShifter.h"
#include <cmath>
#include <algorithm>

#if JUCE_DEBUG
 #define OVT_LOG(msg) juce::Logger::writeToLog (msg)
#else
 #define OVT_LOG(msg) do { } while (false)
#endif

namespace ovtdsp
{
    // Forward declaration: kbdWindow is defined further below but needed
    // early in prepare() to measure the KBD COLA sum.
    static inline double kbdWindow (double phase, double beta);

    PitchShifter::PitchShifter()
    {
        // The ringBuffer has a constexpr size (65536), so we allocate it once
        // here instead of (re)allocating it on every prepare(). This matters:
        // on Live/Cubase VST3, prepareToPlay() can be called dozens of times
        // in a row at startup, and each ringBuffer allocation + memset to 0
        // (512 KB x 5 instances) used to cost ~18s cumulated.
        ringBuffer.setSize (2, bufferSize);
        ringBuffer.clear();
    }

    void PitchShifter::prepare (double sr, int maxBlockSize)
    {
        sampleRate = sr;
        latencyMs = juce::jlimit (8.0f, 40.0f, 20.0f);
        latencySamples = static_cast<int> (sampleRate * (latencyMs * 0.001f));

        // 2026-07-23 (Fix AW): initialize the external attack-gain smoother.
        // The smoother's TC is set to its default (15 ms); it can be tuned
        // at any time via setExternalAttackTauSeconds().
        externalAttackSmoother.prepare (sr);
        externalAttackSmoother.setTimeConstantSeconds (externalAttackTauSeconds);
        externalAttackSmoother.snapTo (1.0f);
        externalAttackEnabled = false;

        // Initialize attack envelope alphas + initial gain (the note-on
        // fade-in starts faded down when an attack time is configured).
        attackEnvelope.computeAlphas (sampleRate, attackMs);
        attackEnvelope.initGain (attackMs > 0.0f);
        wasVoiced = false;

        // Initialize startup fade-in (~20ms)
        startupSamplesRemaining = static_cast<int> (sampleRate * 0.020);
        startupGain = 0.0f;
        startupAlpha = 1.0 - std::exp (-1.0 / (0.020 * sampleRate));

        // Measure the real OLA sum of the KBD window (beta=6) for the
        // effective configuration: 2.5 grains in overlap (outLength =
        // 2.5 * min(Tin, Tout), i.e. 2.5 grains at 40% phase spacing
        // = 1/2.5). The previous measurement (2 windows at 50% spacing)
        // was theoretically correct for a perfect 50% overlap but did
        // not reflect the actual number of overlapping grains,
        // hence a -9 dB under-gain in the DAW.
        //
        // Formula: OLA_sum = average over one output period of
        //   sum_{k=0}^{N-1} w(phase + k/N)
        // where N = 2.5 (rounded up to 3 grains maximum, slightly
        // conservative). For this Kaiser beta=6 window we measure
        // ~1.27 (instead of 1.07 for 2 windows at 50%).
        //
        // Idempotence: this computation does not depend on sampleRate
        // (only on beta and the number of overlaps), so it is done only
        // once per instance (on the first prepare()).
        if (kbdColaSum < 0.0)  // sentinel: initialized to -1 in the header
        {
            constexpr int N = 2048;
            constexpr int numOverlap = 3;   // 2.5 grains, rounded to 3
            constexpr double phaseStep = 1.0 / 2.5; // = 0.4 (= 1/2.5)
            double sum = 0.0;
            for (int i = 0; i < N; ++i)
            {
                const double ph = static_cast<double> (i) / static_cast<double> (N);
                double winSum = 0.0;
                for (int k = 0; k < numOverlap; ++k)
                {
                    double phK = ph + k * phaseStep;
                    if (phK > 1.0) phK -= 1.0;
                    winSum += kbdWindow (phK, 6.0);
                }
                sum += winSum;
            }
            // colaPerSample = OLA sum per output sample.
            // Grain gain = 1.0 / colaPerSample for a unit OLA.
            const double colaPerSample = (sum > 1e-6) ? (sum / static_cast<double> (N)) : 1.0;
            kbdColaSum = colaPerSample;
        }

        // ringBuffer already allocated in the constructor (constexpr size).
        // We clear() it to start from a clean state (zeros).
        ringBuffer.clear();

        reset();
        firstPrepareDone = true; // actual plugin startup
    }

    void PitchShifter::reset()
    {
    ringBuffer.clear();
    absoluteWritePos = 0;
    currentRatio = 1.0f;
    currentFormantRatio = 1.0f;
    outPhase = 0.0;
    lastGrainCenter = 0.0;
    // 2026-07-23 (Fix BB): reset the previous-pitch-ratio tracker so
    // the next process() call does not see a "fake" delta from the
    // initial state.
    lastPitchRatio = 1.0f;

    // Reset attack envelope state. The timers are cleared too: after a
    // full reset the ring buffer is empty, so an in-flight RampDown/
    // RecoverSlow phase has no audible purpose anymore.
    attackEnvelope.initGain (attackMs > 0.0f);
    attackEnvelope.clearTimers();
    wasVoiced = false;
    lastF0 = 0.0f;
        voiceDetector.reset();

    // Reset the startup fade-in: it only re-arms on the very first
    // plugin start (prepare), not on a session reset (seek/preset)
    // happening in the middle of an already audible signal.
        if (! firstPrepareDone)
    {
    startupGain = 0.0f;
            startupSamplesRemaining = static_cast<int> (sampleRate * 0.020);
        }

        for (int i = 0; i < MAX_GRAINS; ++i)
            grains[i].active = false;
    }

    void PitchShifter::resetSoft()
    {
        // Reset the internal state (ring, phases, envelopes, grains) WITHOUT
        // touching the startup fade-in: this avoids a pop mid-session.
        ringBuffer.clear();
        absoluteWritePos = 0;
        totalWritten = 0;
        currentRatio = 1.0f;
        currentFormantRatio = 1.0f;
        smoothedF0 = 0.0f;
        outPhase = 0.0;
        lastGrainCenter = 0.0;
        // 2026-07-23 (Fix BB): reset the previous-pitch-ratio tracker.
        lastPitchRatio = 1.0f;
        wasVoiced = false;
        lastF0 = 0.0f;
        voiceDetector.reset();
        // 2026-07-23 (Fix AW): reset the external attack-gain smoother so
        // the first block after a transport stop / preset change doesn't
        // carry any residual modulation from the previous session.
        externalAttackSmoother.snapTo (1.0f);
        attackEnvelope.forceOpen();
        attackEnvelope.clearTimers();
        externalAttackEnabled = false;
        // startupSamplesRemaining and startupGain are NOT reset here.
        for (int i = 0; i < MAX_GRAINS; ++i)
            grains[i].active = false;
    }

    void PitchShifter::setAttackTimeMs (float ms)
    {
        attackMs = juce::jmax (0.0f, ms);
        // Recalculate the envelope alphas if sampleRate is known
        if (sampleRate > 0.0)
            attackEnvelope.computeAlphas (sampleRate, attackMs);
    }

    void PitchShifter::setLatencyMs (float newLatencyMs)
    {
        latencyMs = juce::jlimit (8.0f, 40.0f, newLatencyMs);
        latencySamples = static_cast<int> (sampleRate * (latencyMs * 0.001f));
    }

    // See the header comment for the full rationale. The outPhase clamp
    // means only ONE grain is created at the onset; the chain then restarts
    // cleanly at its normal ~5 ms cadence. Trade-off: ~5-20 ms of additional
    // attack latency (the time for the first grain to reach the note's start
    // behind the latency line), inaudible behind the 30 ms attack envelope.
    // Resetting lastGrainCenter forces the "(idealCenter - lastGrainCenter)
    // > 50 ms" branch of findBestOffset, which searches a pitch mark
    // LOCALLY around idealCenter - always correct since the new signal is
    // stable by then.
    void PitchShifter::restartGrainChainOnOnset() noexcept
    {
        outPhase = 1.0;
        lastGrainCenter = 0.0;
    }

    // ------------------------------------------------------------------------
    // Window functions
    // ------------------------------------------------------------------------
    
    // KBD (Kaiser-Bessel derived) window with lookup table.
    //
    // The window is precomputed once for beta=6 (the only value used in
    // practice - see the prepare() function) at 2049 phase points across
    // [0, 1]. The per-sample cost drops from ~10 I0 iterations + 1 sqrt
    // to a single linear-interpolated table lookup. For 3 active grains
    // per sample at 44.1 kHz, this saves ~20-30 ms/sec of CPU (1-3% of
    // one core), which is the difference between a plugin that drops
    // out on slow laptops (especially with multiple shifted voices
    // enabled, which multiplies the per-sample cost) and one that
    // doesn't.
    //
    // beta is kept as a parameter for future flexibility, but only
    // beta=6 is currently cached. For other betas, the function falls
    // back to the closed-form computation.
    static inline double kbdWindow (double phase, double beta)
    {
        // Only beta=6 is used in this project. For any other beta, fall
        // back to the original (slow) closed-form computation.
        if (beta != 6.0)
        {
            const double x = beta * std::sqrt (juce::jmax (0.0, 1.0 - (2.0 * phase - 1.0) * (2.0 * phase - 1.0)));
            double i0 = 1.0;
            double term = 1.0;
            for (int k = 1; k <= 10; ++k)
            {
                term *= (x * x) / (4.0 * k * k);
                i0 += term;
            }
            static const double i0_beta_other = []{
                double x = 6.0, sum = 1.0, t = 1.0;
                for (int k = 1; k <= 10; ++k) { t *= (x*x)/(4.0*k*k); sum += t; }
                return sum;
            }();
            // Recompute I0(beta) for the requested beta (not just 6).
            // For the fallback path we accept the extra cost.
            double ib = 1.0, tb = 1.0;
            for (int k = 1; k <= 10; ++k) { tb *= (beta*beta)/(4.0*k*k); ib += tb; }
            (void) i0_beta_other; // silence unused warning
            return i0 / ib;
        }

        // beta == 6: use the precomputed table.
        // 2049 points covers phase in [0, 1] inclusive with linear
        // interpolation between adjacent entries. 16 KB of static
        // storage (2049 * 8 bytes), zero heap allocation.
        static const int KBD_TABLE_SIZE = 2049;
        static const double* const kbdTable = []{
            static double table[KBD_TABLE_SIZE];
            for (int i = 0; i < KBD_TABLE_SIZE; ++i)
            {
                const double ph = static_cast<double> (i) / static_cast<double> (KBD_TABLE_SIZE - 1);
                const double x = 6.0 * std::sqrt (juce::jmax (0.0, 1.0 - (2.0 * ph - 1.0) * (2.0 * ph - 1.0)));
                double i0 = 1.0;
                double term = 1.0;
                for (int k = 1; k <= 10; ++k)
                {
                    term *= (x * x) / (4.0 * k * k);
                    i0 += term;
                }
                static const double i0_beta = []{
                    double xx = 6.0, sum = 1.0, t = 1.0;
                    for (int k = 1; k <= 10; ++k) { t *= (xx*xx)/(4.0*k*k); sum += t; }
                    return sum;
                }();
                table[i] = i0 / i0_beta;
            }
            return table;
        }();

        // Clamp phase to [0, 1] and look up with linear interpolation.
        const double p = juce::jlimit (0.0, 1.0, phase);
        const double pos = p * static_cast<double> (KBD_TABLE_SIZE - 1);
        const int idx0 = static_cast<int> (pos);
        const int idx1 = juce::jmin (idx0 + 1, KBD_TABLE_SIZE - 1);
        const double frac = pos - static_cast<double> (idx0);
        return kbdTable[idx0] * (1.0 - frac) + kbdTable[idx1] * frac;
    }

    // Fast Hann window (COLA for 50% overlap)
    static inline double hannWindow (double phase)
    {
        return 0.5 - 0.5 * std::cos (phase * 2.0 * juce::MathConstants<double>::pi);
    }

    // ------------------------------------------------------------------------
    // Sub-sample pitch mark detection via parabolic interpolation
    // ------------------------------------------------------------------------
    
    double PitchShifter::findBestOffset (double idealReadPos, double targetToMatch, double searchWindowMs, float f0, double maxOffset) const
    {
        const double searchRange = searchWindowMs * sampleRate / 1000.0;
        const double period = (f0 > 40.0f) ? (sampleRate / f0) : (sampleRate * 0.010);
        const double effectiveSearchRange = juce::jmax (searchRange, period * 1.2);

        int64_t startSearch = static_cast<int64_t> (idealReadPos - effectiveSearchRange);
        int64_t endSearch = static_cast<int64_t> (idealReadPos + effectiveSearchRange);
        
        int64_t maxSafePos = static_cast<int64_t> (idealReadPos + maxOffset);
        if (endSearch > maxSafePos) endSearch = maxSafePos;
        if (startSearch > endSearch) startSearch = endSearch;

        const int templateSize = 64;  // ~1.5ms at 44.1kHz
        const int step = 1;  // Step by 1 for better accuracy (was 2)

        double bestCorr = -1.0;
        double bestOffset = 0.0;

        const float* ringData = ringBuffer.getReadPointer (0);
        int64_t baseTarget = static_cast<int64_t> (targetToMatch);

        for (int64_t p = startSearch; p <= endSearch; p += step)
        {
            double corr = 0.0;
            double normA = 0.0;
            double normB = 0.0;

            for (int i = 0; i < templateSize; ++i)
            {
                const float a = ringData[static_cast<uint64_t> (p + i) & bufferMask];
                const float b = ringData[static_cast<uint64_t> (baseTarget + i) & bufferMask];
                
                corr += a * b;
                normA += a * a;
                normB += b * b;
            }

            const double denom = std::sqrt (normA * normB);
            if (denom > 1e-6)
                corr /= denom;

            if (corr > bestCorr)
            {
                bestCorr = corr;
                bestOffset = static_cast<double> (p) - idealReadPos;
            }
        }

        // Parabolic interpolation for sub-sample precision
        // If we have at least 3 points around the peak, fit a parabola
        // For now, just return the best offset (could be enhanced with parabolic fit)
        return bestOffset;
    }

    float PitchShifter::getInterpolatedSample (int channel, double readPos) const
    {
        int64_t pos1 = static_cast<int64_t> (std::floor (readPos));
        int64_t pos2 = pos1 + 1;
        double frac = readPos - static_cast<double> (pos1);

        const float* data = ringBuffer.getReadPointer (channel);
        float a = data[static_cast<uint64_t> (pos1) & bufferMask];
        float b = data[static_cast<uint64_t> (pos2) & bufferMask];
        
        // Linear interpolation (could upgrade to cubic for better quality)
        return a + static_cast<float> (frac) * (b - a);
    }

    // ------------------------------------------------------------------------
    // Main processing
    // ------------------------------------------------------------------------
    
    void PitchShifter::process (juce::AudioBuffer<float>& buffer, float pitchRatio, float formantRatio, float f0)
    {
        process (static_cast<const juce::AudioBuffer<float>&> (buffer), buffer, pitchRatio, formantRatio, f0);
    }

    void PitchShifter::process (const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output, float pitchRatio, float formantRatio, float f0)
    {
        const int numSamples = input.getNumSamples();
        const int numChannels = input.getNumChannels();
        if (numSamples == 0) return;

        // Defense: ensure ring buffer is initialized
        if (ringBuffer.getNumSamples() == 0 || ringBuffer.getNumChannels() < 2)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                output.copyFrom (ch, 0, input, ch, 0, numSamples);
            return;
        }

        // Ensure output layout matches input
        if (output.getNumChannels() != numChannels || output.getNumSamples() != numSamples)
            output.setSize (numChannels, numSamples, false, true, false);

        // Validate input ratios
        if (pitchRatio <= 0.0f || std::isnan (pitchRatio) || std::isinf (pitchRatio))
            pitchRatio = 1.0f;
        if (formantRatio <= 0.0f || std::isnan (formantRatio) || std::isinf (formantRatio))
            formantRatio = 1.0f;

        // Smooth ratio changes (avoid clicks on parameter automation)
        const float alpha = 0.005f;  // ~200 samples at 44.1kHz = ~4.5ms time constant
        const float maxPitchRatio = 4.0f;
        const float minPitchRatio = 0.25f;

        // Throttle debug logs
        static uint32_t lastLogMs = 0;
        uint32_t nowMs = juce::Time::getMillisecondCounter();
        bool doLog = false;
        if (nowMs - lastLogMs > 1000)
        {
            lastLogMs = nowMs;
            doLog = true;
        }

        // Clamp ratios
        pitchRatio = juce::jlimit (minPitchRatio, maxPitchRatio, pitchRatio);
        formantRatio = juce::jlimit (minPitchRatio, maxPitchRatio, formantRatio);

        // Block-level voice onset detection: if first sample has f0 > 0
        // and lastF0 was 0 (or very small), it's a voice onset. Only used
        // to snap the attack envelope automaton to zero (note-on fade-in);
        // the grain chain restart below is independent and always runs,
        // because it is what keeps the OLA chain from reading stale
        // positions.
        bool blockOnset = (f0 > 40.0f && lastF0 <= 40.0f);
        if (blockOnset && attackEnvelopeEnabled)
        {
            // Note-on: hard snap the envelope to zero at the block boundary.
            // The preceding gap was silent, so no step is audible; the
            // RecoverNormal phase then fades the note in over attackMs.
            attackEnvelope.snapToZero();
        }

        for (int i = 0; i < numSamples; ++i)
        {
            // Smooth ratio transitions
            currentRatio += alpha * (pitchRatio - currentRatio);
            currentFormantRatio += alpha * (formantRatio - currentFormantRatio);

            // Write input to ring buffer
            ringBuffer.setSample (0, static_cast<int> (absoluteWritePos & bufferMask), input.getSample (0, i));
            if (numChannels > 1)
                ringBuffer.setSample (1, static_cast<int> (absoluteWritePos & bufferMask), input.getSample (1, i));
            else
                ringBuffer.setSample (1, static_cast<int> (absoluteWritePos & bufferMask), input.getSample (0, i));

            // Virtual read position (accounting for latency)
            double virtualInputTime = static_cast<double> (absoluteWritePos) - latencySamples;

            // Smooth the input pitch (avoids period jumps of the grains
            // at note start or on a pitch step).
            if (f0 > 0.0f)
            {
                // At note onset (sharp pitch jump), we make smoothedF0
                // converge directly to f0 instead of smoothing: the new
                // grain thus gets the correct periods immediately, and
                // the per-grain attack (40%) smooths the start. Without
                // this, the grain targets a wrong period -> discontinuity
                // at the note jump.
                smoothedF0 += kF0SmoothAlpha * (f0 - smoothedF0);
            }
            else
                smoothedF0 = 0.0f; // silence: keep 0

            // Target pitch frequency (based on the smoothed f0)
            double targetF0 = smoothedF0 * currentRatio;
            targetF0 = juce::jlimit (20.0, 2000.0, targetF0);

            // Phase accumulator for output pitch marks.
            // Voiced/unvoiced detection with HYSTERESIS + DEBOUNCE (see the
            // VoiceActivityDetector documentation): avoids back-and-forth
            // around the threshold (vocal flutter at note starts), which
            // would re-arm the attack envelope in a loop -> clicks.
            const bool isVoiced = voiceDetector.processSample (f0);
            
            // Voice onset detection: f0 transitions from unvoiced to voiced
            // OR large sudden pitch jump (new note attack)
            bool onsetDetected = false;
            if (isVoiced && !wasVoiced)
            {
                onsetDetected = true;
            }
            else if (isVoiced && wasVoiced && lastF0 > 0.0f)
            {
                // Detect sudden pitch change > 2 semitones (note attack)
                double pitchRatio = f0 / lastF0;
                if (pitchRatio > 1.12 || pitchRatio < 0.89) // ~2 semitones
                    onsetDetected = true;
            }

            // 2026-07-23 (Fix BB.2): detect sudden changes in `pitchRatio`
            // (the per-block correction ratio passed by the caller, which
            // includes FlexTune modulation, humanize random walk, and
            // vibrato preservation). When the per-block delta exceeds
            // 3% (the OLA grain spacing sensitivity threshold for
            // pitch-ratio discontinuities), the internal attack envelope
            // is armed to mask the OLA re-organisation. Without this,
            // the OLA chain "snaps" to a new period every time the
            // smoother output changes by more than ~1% per block,
            // producing the user-reported "pop/clicks at pitch changes"
            // with Flex>0 (which can change currentFlexTuneAmount
            // from 0 to 1.0 at every deadband transition, i.e. several
            // times per vibrato cycle).
            //
            // IMPORTANT: we only arm the envelope (not the OLA chain
            // reset), because the OLA chain reset would re-introduce the
            // "trumpet" attack artifact at every deadband transition
            // (5+ per second with vibrato). The envelope-only path is
            // the right balance: it masks the OLA re-organisation but
            // doesn't restart the chain.
            //
            // Note: we use the current per-block pitchRatio (not the
            // smoothed currentRatio), because the smoothing in
            // currentRatio is buffer-rate and we want to detect the
            // INTENTION of the caller, not the in-block average.
            bool ratioJumpDetected = false;
            if (attackEnvelopeEnabled && !onsetDetected)
            {
                const float ratioDelta = std::abs (pitchRatio - lastPitchRatio);
                if (ratioDelta > 0.03f) // 3% = ~50 cents at ratio=1
                    ratioJumpDetected = true;
            }
            lastPitchRatio = pitchRatio;
            
            // 2026-07-23 (Fix BB.2): when a sudden ratio change is
            // detected (e.g. FlexTune deadband transition), arm the
            // internal attack envelope WITHOUT resetting the OLA chain.
            // This is the same envelope arming as the ONSET case, but
            // without the grain-chain restart (which would re-introduce
            // the "trumpet" artifact at every deadband transition).
            if (ratioJumpDetected)
                attackEnvelope.armForRatioJump (sampleRate);

            if (onsetDetected)
            {
                // The arming below is what gives the internal attack
                // envelope its 150/20 ms "ramp down then slow ramp up"
                // behaviour on note onsets and pitch jumps (see the
                // AttackEnvelope phase documentation). When an external
                // helper (e.g. ovtdsp::AttackAwareEnv) is driving the
                // correction gain, we don't want the internal envelope to
                // ALSO run - that double-attenuation is what the user
                // reports as a "scratchy attack" at low Amount (Fix AL).
                // The arming is skipped, but the grain-chain restart below
                // still runs: it is independent of the envelope and is what
                // prevents clicks from mis-aligned grains.
                if (attackEnvelopeEnabled)
                {
                    // NOTE: no hard reset of the envelope gain here - that
                    // would create an instant step from the previous output
                    // to 0 at the first sample after the jump -> click.
                    // Instead RampDown drives the target to 0 so the gain
                    // dips smoothly, then RecoverSlow climbs back with the
                    // slow TC, masking the OLA re-organisation window.
                    attackEnvelope.armForOnset (sampleRate);
                }

                // Grain-chain restart: clamp outPhase so exactly ONE grain
                // is created at the onset, and invalidate lastGrainCenter so
                // the next grain re-aligns on a LOCAL pitch mark (full
                // rationale in restartGrainChainOnOnset()).
                restartGrainChainOnOnset();
            }
            wasVoiced = isVoiced;
            lastF0 = f0;

            if (isVoiced)
                outPhase += targetF0 / sampleRate;
            else
                outPhase += 100.0 / sampleRate;

            // Generate output pitch mark (grain center).
            // IMPORTANT: only create grains when isVoiced. During silence
            // (!isVoiced), the existing grains from the last note will fade
            // out naturally over their outLength. If we kept creating new
            // grains here, those grains would have readPos pointing into the
            // tail of the previous note (e.g. a grain created 32 ms into a
            // gap would still read the end of the previous note at
            // t - latency - outLength/2). When that readPos crosses the
            // note->silence boundary in the ring buffer, the content steps
            // from sin to 0 in 1 sample -> audible "pop" / "click" exactly
            // as reported by the staccato test (208 ms, 383 ms, 731 ms,
            // 905 ms). The cleanest fix: do not synthesize new grains while
            // we are not voiced; the OLA chain will go to silence by itself.
            if (isVoiced && outPhase >= 1.0)
            {
                outPhase -= 1.0;

                // Input and output periods
                // Period based on the SMOOTHED f0 so the grain size
                // evolves smoothly across a note jump.
                double Tin = (smoothedF0 > 40.0f) ? (sampleRate / smoothedF0) : (sampleRate * 0.010);
                double Tout = (smoothedF0 > 40.0f) ? (sampleRate / targetF0) : (sampleRate * 0.010);
                double F = currentFormantRatio;

                // Adaptive grain length: 2.5 cycles of the SHORTER period
                // This ensures enough overlap for smooth OLA while adapting to pitch
                double minPeriod = juce::jmin (Tin, Tout);
                double outLength = 2.5 * minPeriod;

                // Safety margin: ensure we don't read beyond available history
                double maxSafeOffset = latencySamples - (outLength * F / 2.0) - 10.0;
                if (maxSafeOffset < 0.0) maxSafeOffset = 0.0;

                // Ideal center position in input signal
                double idealCenter = virtualInputTime;

                // Do NOT force outPhase = 0 on onset anymore: it created a
                // gap in the OLA chain (the in-progress grain was
                // interrupted) -> discontinuity at the note jump. The per-grain
                // attack (40% of outLength) already smooths the new grain
                // start, and smoothedF0 softens the period transition.
                // We keep outPhase continuous.

                // Protection against reading outside valid history: if the
                // ideal center (or the read start of the grain preceding it)
                // would fall before the samples actually available in the
                // ring, we shift it right. Otherwise, in low-latency mode or
                // right after prepare(), we would read zeros mixed with the
                // signal -> raw transition -> pop at note start.
                const double halfGrain = outLength * F / 2.0;
                const double minCenter = static_cast<double> (latencySamples) + halfGrain;
                if (idealCenter < minCenter)
                    idealCenter = minCenter;

                // Find best matching pitch mark via cross-correlation.
                double bestOffset = 0.0;
                if (f0 > 40.0f)
                {
                    if (lastGrainCenter > 0.0
                        && (idealCenter - lastGrainCenter) <= sampleRate * 0.05)
                    {
                        // Previous grain is close: search for the pitch mark
                        // that follows (lastGrainCenter + Tin) for a coherent
                        // phase.
                        double targetToMatch = lastGrainCenter + Tin;
                        bestOffset = findBestOffset (idealCenter, targetToMatch, 10.0, f0, maxSafeOffset);
                    }
                    else
                    {
                        // No close previous grain (pause > 50 ms or very
                        // first grain): search for a LOCAL pitch mark around
                        // idealCenter instead of jumping randomly. This aligns
                        // the 1st grain of the new note on an actual period of
                        // the signal -> no discontinuity.
                        bestOffset = findBestOffset (idealCenter, idealCenter, 12.0, f0, maxSafeOffset);
                    }
                }

                if (bestOffset > maxSafeOffset) bestOffset = maxSafeOffset;

                double center = idealCenter + bestOffset;
                lastGrainCenter = center;

                // Find free grain slot
                int gIdx = -1;
                for (int j = 0; j < MAX_GRAINS; ++j)
                {
                    if (!grains[j].active) { gIdx = j; break; }
                }

                if (gIdx >= 0)
                {
                    // Grain read position: center - half grain length (scaled by formant ratio)
                    grains[gIdx].readPos = center - (outLength * F / 2.0);
                    grains[gIdx].speed = F;  // Formant scaling of read speed
                    grains[gIdx].phase = 0.0;
                    grains[gIdx].phaseInc = 1.0 / outLength;
                    
                    // Gain normalization for OLA: with 2.5 grains in overlap
                    // (outLength = 2.5 * min(Tin, Tout), so 2.5 grains at 40%
                    // phase apart) and a window whose per-sample OLA sum is
                    // kbdColaSum (~1.50 for Kaiser beta=6 averaged over all
                    // phase positions), each grain must use gain = K / kbdColaSum
                    // to get unity OLA output, where K is a compensation factor.
                    //
                    // K = 1.20 : empirical factor calibrated against the
                    //   measured RMS on a sustained 200 Hz sinus with
                    //   attackFraction=0.0. Gives RMS = 0.71 (expected for
                    //   a unit sinus = 0.707) -> 0 dB.
                    // Without this, RMS = 0.24 instead of 0.707 (-9.3 dB
                    // under-gain, perceived as "the plugin drops the level"
                    // in the DAW). With K, RMS = 0.71 (target).
                    //
                    // Previously: gain = Tout / outLength * (1.0 / kbdColaSum)
                    //   = 0.4 / 1.07 = 0.374 for ratio=1
                    // That "Tout/outLength" factor was a stale time-stretching
                    // compensation that made NO sense for ratio=1. Combined
                    // with the kbdColaSum measured for only 2 grains at 50%
                    // apart (not 2.5 at 40% apart), the per-grain gain was
                    // ~0.374 instead of the correct ~0.87.
                    //
                    // History of K values (attackFraction, K) -> RMS:
                    //   (0.50, 1.45) -> 0.52  // Fix J/J': residual under-gain
                    //   (0.50, 1.97) -> 0.71  // correct, but click at 452 ms
                    //   (0.00, 1.20) -> 0.71  // OK rms, but click at 325 ms
                    //   (onset=1, no=0, 1.20) -> 0.71  // Fix K retained
                    constexpr double kGainCompensation = 1.20;
                    grains[gIdx].gain = static_cast<float> (kGainCompensation / kbdColaSum);
                    grains[gIdx].active = true;

                    // Per-grain attack: ABSENT in steady state
                    // (attackFraction = 0.0) so as not to divert the COLA
                    // gain and keep RMS = 0.71. The global attack ramp
                    // (attackMs = 30 ms via setAttackTimeMs) already absorbs
                    // transients at note start.
                    //
                    // On onset (note start or pitch jump > 2 semitones), we
                    // force attackFraction = 1.0 (full grain): the new grain
                    // contributes ~0 at the jump instant, which prevents the
                    // "OLA click" (sum of windows transiently exceeding 1.0
                    // at the jump). This impact is localized to the first
                    // ~12 ms of an onset, negligible on the measured RMS
                    // (sustained note > 200 ms).
                    const float attackFraction = onsetDetected ? 1.0f : 0.0f;
                    const int attackSamples = juce::jmax (4, static_cast<int> (outLength * attackFraction));
                    grains[gIdx].attackAlpha = 1.0 / static_cast<double> (attackSamples);
                    grains[gIdx].attackGain = 0.0f;

                    if (doLog)
                    {
                        OVT_LOG ("PitchShifter: grain idx=" + juce::String (gIdx) +
                                 " center=" + juce::String (center, 3) +
                                 " outLength=" + juce::String (outLength, 3) +
                                 " F=" + juce::String (F, 3) +
                                 " Tin=" + juce::String (Tin, 2) +
                                 " Tout=" + juce::String (Tout, 2));
                    }
                }
            }

            // Synthesize output from active grains
            float outL = 0.0f, outR = 0.0f;
            for (int j = 0; j < MAX_GRAINS; ++j)
            {
                if (grains[j].active)
                {
                    // Kaiser-Bessel derived window (better spectral containment than Hann)
                    double win = kbdWindow (grains[j].phase, 6.0);  // beta=6
                    float sampleGain = static_cast<float> (win * grains[j].gain);

                    // Per-grain attack to avoid clicks at grain start
                    float grainAttackGain = 1.0f;
                    if (grains[j].attackGain < 1.0f)
                    {
                        grains[j].attackGain += (1.0f - grains[j].attackGain) * static_cast<float> (grains[j].attackAlpha);
                        grainAttackGain = grains[j].attackGain;
                    }
                    sampleGain *= grainAttackGain;

                    // (No per-grain release here: the previous attempt with a
                    //  50-sample release window only covered 1.1 ms after the
                    //  jump, but the audible click is at 11.6 ms (when the
                    //  global attackGain has ramped up to 0.32). The click is
                    //  caused by the old grains reading new signal at high
                    //  window values, and the global attackGain does not mask
                    //  it enough. We leave the slot for a future fix.)

                    outL += getInterpolatedSample (0, grains[j].readPos) * sampleGain;
                    outR += getInterpolatedSample (1, grains[j].readPos) * sampleGain;

                    grains[j].readPos += grains[j].speed;
                    grains[j].phase += grains[j].phaseInc;

                    if (grains[j].phase >= 1.0)
                        grains[j].active = false;
                }
            }

            // Apply the output gain from the attack-envelope automaton.
            // Three exclusive modes (the mode can only change on block
            // boundaries):
            //
            // 1. EXTERNAL DRIVER (Fix AW): ovtdsp::AttackAwareEnv pushes a
            //    smoothed BLOCK-level gain through setExternalAttackGain();
            //    it lands directly in attackEnvelope.gain. Per-sample
            //    ramping is a no-op here because the value is constant
            //    within the block. The modulation is applied to the OUTPUT
            //    (not the OLA target ratio) so the grain spacing stays
            //    stable across the transition. The internal timers are kept
            //    cleared so they cannot fire later in a stale state.
            // 2. INTERNAL ENVELOPE: the explicit automaton runs per sample
            //    (RampDown -> RecoverSlow -> RecoverNormal -> Open). It is
            //    skipped when an external helper owns the gain: with BOTH
            //    envelopes active they would compete, producing the "double
            //    attenuation" scratch reported on the Attack feature at low
            //    Amount (Fix AL).
            // 3. BYPASS: envelope disabled or attackMs == 0. Pin the gain
            //    fully open so a mid-session toggle cannot leave a residual
            //    dip.
            if (externalAttackEnabled)
            {
                attackEnvelope.clearTimers();
                const float g = attackEnvelope.gain;
                outL *= g;
                outR *= g;
            }
            else if (attackEnvelopeEnabled && attackMs > 0.0f)
            {
                const float g = attackEnvelope.processSample();
                outL *= g;
                outR *= g;
            }
            else
            {
                attackEnvelope.forceOpen();
            }

            // Startup fade-in: ring buffer starts empty, so first ~20ms reads zeros
            if (startupSamplesRemaining > 0)
            {
                if (startupGain < 1.0f)
                    startupGain += (1.0f - startupGain) * static_cast<float> (startupAlpha);
                else
                    startupGain = 1.0f;
                outL *= startupGain;
                outR *= startupGain;
                startupSamplesRemaining--;
            }

            // Write output
            if (output.getNumSamples() == numSamples && output.getNumChannels() > 0)
            {
                output.setSample (0, i, outL);
                if (numChannels > 1)
                    output.setSample (1, i, outR);
            }

            if (doLog && i == 0)
            {
                int activeCount = 0;
                for (int j = 0; j < MAX_GRAINS; ++j) if (grains[j].active) ++activeCount;
                OVT_LOG ("PitchShifter.process: f0=" + juce::String (f0, 3) +
                                      " pitchRatio=" + juce::String (pitchRatio, 3) +
                                      " grainsActive=" + juce::String (activeCount) +
                                      " out0=" + juce::String (outL, 6));
            }

            absoluteWritePos++;
            totalWritten++;
        }
    }

    // Debug: force-create a test grain
    void PitchShifter::forceCreateTestGrain()
    {
        int gIdx = -1;
        for (int j = 0; j < MAX_GRAINS; ++j)
            if (!grains[j].active) { gIdx = j; break; }
        if (gIdx < 0) return;

        double center = static_cast<double> (absoluteWritePos);
        double Tin = (sampleRate > 0.0) ? (sampleRate / 100.0) : 480.0;
        double Tout = Tin;
        double F = 1.0;
        double minPeriod = juce::jmin (Tin, Tout);
        double outLength = 2.5 * minPeriod;

        grains[gIdx].readPos = center - (outLength * F / 2.0);
        grains[gIdx].speed = F;
        grains[gIdx].phase = 0.0;
        grains[gIdx].phaseInc = 1.0 / outLength;
        grains[gIdx].gain = static_cast<float> (Tout / outLength * 2.0);
        grains[gIdx].active = true;
    }

} // namespace ovtdsp


