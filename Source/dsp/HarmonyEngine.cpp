#include "HarmonyEngine.h"
#include <cmath>
#include <vector>

#if JUCE_DEBUG
 #define OVT_LOG(msg) juce::Logger::writeToLog (msg)
#else
 #define OVT_LOG(msg) do { } while (false)
#endif

namespace ovtdsp
{

    // Helper: convert semitone offset to frequency relative to base
    static float freqFromSemitoneOffset(float baseFreq, int semitoneOffset)
    {
        return baseFreq * std::pow(2.0f, semitoneOffset / 12.0f);
    }

    void HarmonyEngine::prepare (double sampleRate)
    {
        currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    }

    void HarmonyEngine::setVoiceGate (bool on)
    {
        voiceGate = on;
    }

    /**
     * Determine the MIDI note number nearest to baseFreq.
     */
    static int midiFromFreq(float hz)
    {
        if (hz <= 0.0f) return 0;
        float semi = hzToSemitones(hz); // semitones relative to A4
        int midi = static_cast<int>(std::round(69.0f + semi));
        return midi;
    }

    juce::Array<float> HarmonyEngine::getHarmonyNotes (float baseFreq, const juce::Array<int>& scaleIntervals,
                                                       HarmonyType harmonyType) const
    {
        juce::Array<float> result;
        if (baseFreq <= 0.0f)
            return result;

        // If no scale intervals, fallback to simple semitone offsets
        if (scaleIntervals.size() == 0)
        {
            switch (harmonyType)
            {
                case HarmonyType::ThirdBelow:           result.add (freqFromSemitoneOffset (baseFreq, -3)); break;
                case HarmonyType::ThirdAbove:           result.add (freqFromSemitoneOffset (baseFreq,  3)); break;
                case HarmonyType::ThirdBelowAbove:      result.add (freqFromSemitoneOffset (baseFreq, -3)); result.add (freqFromSemitoneOffset (baseFreq, 3)); break;
                case HarmonyType::FourthBelow:          result.add (freqFromSemitoneOffset (baseFreq, -5)); break;
                case HarmonyType::FourthAbove:          result.add (freqFromSemitoneOffset (baseFreq,  5)); break;
                case HarmonyType::FourthBelowAbove:     result.add (freqFromSemitoneOffset (baseFreq, -5)); result.add (freqFromSemitoneOffset (baseFreq, 5)); break;
                case HarmonyType::FifthBelow:           result.add (freqFromSemitoneOffset (baseFreq, -7)); break;
                case HarmonyType::FifthAbove:           result.add (freqFromSemitoneOffset (baseFreq,  7)); break;
                case HarmonyType::FifthBelowAbove:      result.add (freqFromSemitoneOffset (baseFreq, -7)); result.add (freqFromSemitoneOffset (baseFreq, 7)); break;
                case HarmonyType::ThirdBelowFifthAbove: result.add (freqFromSemitoneOffset (baseFreq, -3)); result.add (freqFromSemitoneOffset (baseFreq, 7)); break;
                case HarmonyType::FifthBelowThirdAbove: result.add (freqFromSemitoneOffset (baseFreq, -7)); result.add (freqFromSemitoneOffset (baseFreq, 3)); break;
                case HarmonyType::OctaveBelow:          result.add (freqFromSemitoneOffset (baseFreq, -12)); break;
                case HarmonyType::OctaveAbove:          result.add (freqFromSemitoneOffset (baseFreq,  12)); break;
                case HarmonyType::OctaveBelowAbove:     result.add (freqFromSemitoneOffset (baseFreq, -12)); result.add (freqFromSemitoneOffset (baseFreq, 12)); break;
                case HarmonyType::VocalStack3:          result.add (freqFromSemitoneOffset (baseFreq,  3)); result.add (freqFromSemitoneOffset (baseFreq, 7)); result.add (freqFromSemitoneOffset (baseFreq, 12)); break;
                case HarmonyType::VocalStack4:          result.add (freqFromSemitoneOffset (baseFreq, -3)); result.add (freqFromSemitoneOffset (baseFreq, 3)); result.add (freqFromSemitoneOffset (baseFreq, 7)); result.add (freqFromSemitoneOffset (baseFreq, 12)); break;
                case HarmonyType::PowerChord:           result.add (freqFromSemitoneOffset (baseFreq,  7)); result.add (freqFromSemitoneOffset (baseFreq, 12)); break;
                case HarmonyType::ParallelThird:        result.add (freqFromSemitoneOffset (baseFreq,  3)); break;
                case HarmonyType::Drone:                result.add (baseFreq); break;
                case HarmonyType::Unison2:             result.add (baseFreq); result.add (baseFreq); break;
                case HarmonyType::UnisonOctaves4:      result.add (baseFreq); result.add (baseFreq); result.add (freqFromSemitoneOffset (baseFreq, -12)); result.add (freqFromSemitoneOffset (baseFreq, 12)); break;
                default: break;
            }
            return result;
        }

        // Map harmony types to degree offsets (in scale degrees)
        juce::Array<int> degreeOffsets;
        const int scaleSize = scaleIntervals.size();
        switch (harmonyType)
        {
            case HarmonyType::ThirdBelow:           degreeOffsets.add(-2); break;
            case HarmonyType::ThirdAbove:           degreeOffsets.add( 2); break;
            case HarmonyType::ThirdBelowAbove:      degreeOffsets.add(-2); degreeOffsets.add( 2); break;
            case HarmonyType::FourthBelow:          degreeOffsets.add(-3); break;
            case HarmonyType::FourthAbove:          degreeOffsets.add( 3); break;
            case HarmonyType::FourthBelowAbove:     degreeOffsets.add(-3); degreeOffsets.add( 3); break;
            case HarmonyType::FifthBelow:           degreeOffsets.add(-4); break;
            case HarmonyType::FifthAbove:           degreeOffsets.add( 4); break;
            case HarmonyType::FifthBelowAbove:      degreeOffsets.add(-4); degreeOffsets.add( 4); break;
            case HarmonyType::ThirdBelowFifthAbove: degreeOffsets.add(-2); degreeOffsets.add( 4); break;
            case HarmonyType::FifthBelowThirdAbove: degreeOffsets.add(-4); degreeOffsets.add( 2); break;
            case HarmonyType::OctaveBelow:          degreeOffsets.add(-(scaleSize)); break;
            case HarmonyType::OctaveAbove:          degreeOffsets.add(scaleSize); break;
            case HarmonyType::OctaveBelowAbove:     degreeOffsets.add(-(scaleSize)); degreeOffsets.add(scaleSize); break;
            case HarmonyType::VocalStack3:          degreeOffsets.add( 2); degreeOffsets.add(4); degreeOffsets.add(scaleSize); break;
            case HarmonyType::VocalStack4:          degreeOffsets.add(-2); degreeOffsets.add(2); degreeOffsets.add(4); degreeOffsets.add(scaleSize); break;
            case HarmonyType::PowerChord:           degreeOffsets.add( 4); degreeOffsets.add(scaleSize); break;
            case HarmonyType::ParallelThird:        degreeOffsets.add( 1); break;
            case HarmonyType::Drone:                degreeOffsets.add(-(scaleSize)); break;
            case HarmonyType::Unison2:             degreeOffsets.add(0); degreeOffsets.add(0); break;
            case HarmonyType::UnisonOctaves4:      degreeOffsets.add(0); degreeOffsets.add(0); degreeOffsets.add(-(scaleSize)); degreeOffsets.add(scaleSize); break;
            default: break;
        }

        // Find nearest scale degree to the base frequency
        const int baseMidi = midiFromFreq(baseFreq);
        const int baseOctave = baseMidi / 12;

        int bestDegree = 0;
        int bestMidi = baseMidi;
        int bestDist = INT_MAX;

        for (int d = 0; d < scaleSize; ++d)
        {
            int chroma = scaleIntervals[d]; // interval relative to C
            int noteInChromatic = chroma; // 0..11
            // candidate MIDI at same octave as base
            int candidate = baseOctave * 12 + noteInChromatic;
            // adjust octave to nearest
            while (candidate - baseMidi > 6) candidate -= 12;
            while (baseMidi - candidate > 6) candidate += 12;
            int dist = std::abs(candidate - baseMidi);
            if (dist < bestDist)
            {
                bestDist = dist;
                bestDegree = d;
                bestMidi = candidate;
            }
        }

        // For each required degree offset, compute target frequency
        for (int i = 0; i < degreeOffsets.size(); ++i)
        {
            int targetDegree = bestDegree + degreeOffsets[i];

            // compute octave shift and wrapped index
            int wrappedIndex = ((targetDegree % scaleSize) + scaleSize) % scaleSize;
            int octaveShift = (targetDegree - wrappedIndex) / scaleSize;

            int chroma = scaleIntervals[wrappedIndex];
            int candidateMidi = baseOctave * 12 + chroma + octaveShift * 12;
            // Prefer candidates above/below the base depending on degree offset sign
            if (degreeOffsets[i] > 0)
            {
                // ensure we pick a candidate at or above the base (prefer above)
                while (candidateMidi < baseMidi - 1) candidateMidi += 12;
            }
            else if (degreeOffsets[i] < 0)
            {
                // ensure we pick a candidate at or below the base (prefer below)
                while (candidateMidi > baseMidi + 1) candidateMidi -= 12;
            }
            else
            {
                // adjust to be near bestMidi
                while (candidateMidi - bestMidi > 6) candidateMidi -= 12;
                while (bestMidi - candidateMidi > 6) candidateMidi += 12;
            }

            float semisFromA4 = static_cast<float>(candidateMidi - 69);
            float freq = semitonesToHz (semisFromA4);
            result.add (freq);
        }

        return result;
    }

    int HarmonyEngine::getScaleDegree (float freq, int key, const juce::Array<int>& scaleIntervals) const
    {
        if (freq <= 0.0f || scaleIntervals.size() == 0)
            return 0;

        int midi = midiFromFreq(freq);
        int bestDegree = 0;
        int bestDist = INT_MAX;
        int baseOctave = midi / 12;

        for (int d = 0; d < scaleIntervals.size(); ++d)
        {
            int chroma = (key + scaleIntervals[d]) % 12;
            int candidate = baseOctave * 12 + chroma;
            while (candidate - midi > 6) candidate -= 12;
            while (midi - candidate > 6) candidate += 12;
            int dist = std::abs(candidate - midi);
            if (dist < bestDist)
            {
                bestDist = dist;
                bestDegree = d;
            }
        }
        return bestDegree;
    }

    float HarmonyEngine::degreeToFreq (int scaleDegree, float baseFreq, int numOctaves) const
    {
        int totalSemis = scaleDegree + numOctaves * 12;
        return baseFreq * std::pow (2.0f, totalSemis / 12.0f);
    }

    void HarmonyEngine::renderHarmonies (float inputFreq, const juce::Array<float>& harmonyFrequencies,
                                         float volume, double sampleRate, juce::AudioBuffer<float>& outputBuffer,
                                         int key, int scaleIndex, float blend, int toneMode, float toneColor,
                                         bool gateActive)
    {
        const int numSamples = outputBuffer.getNumSamples();
        const int numChannels = outputBuffer.getNumChannels();
        if (numSamples <= 0)
            return;

        // Ensure sample rate is set
        if (sampleRate > 0.0)
            currentSampleRate = sampleRate;

        const double twoPi = juce::MathConstants<double>::twoPi;

        const int numVoices = harmonyFrequencies.size();

        // Resize internal state
        phases.resize((size_t)numVoices);
        amplitudes.resize((size_t)numVoices);
        targetAmps.resize((size_t)numVoices);
        attackTotalSamples.resize((size_t)numVoices);
        attackStartAmp.resize((size_t)numVoices);
        voicePrevGate.resize((size_t)numVoices);

        // compute attack/release smoothing coefficients (simple one-pole)
        const double attackAlpha = 1.0 - std::exp(-1000.0 / (attackMs * currentSampleRate));
        const double releaseAlpha = 1.0 - std::exp(-1000.0 / (releaseMs * currentSampleRate));

        // Resize the linear fade-in counter to match the voice count
        attackSamplesRemaining.resize ((size_t) numVoices);

        // Effective attack length: when the Noise Gate is active the per-voice
        // attack is clamped to a short gate-follow time so the harmony voices
        // swell together WITH the gated dry signal. When the gate is off, the
        // user-configurable attackMs drives the per-voice fade-in.
        const float effectiveAttackMs = gateActive
            ? juce::jmin (attackMs, gateFollowMs)
            : attackMs;
        const int effectiveAttackSamples = (int) std::llround (effectiveAttackMs * 0.001 * currentSampleRate);

        // initialize amplitudes and targets if needed
        for (int v = 0; v < numVoices; ++v)
        {
            // Spread the starting phase across voices so that newly spawned
            // voices do not all begin at sin(0)=0 in lockstep. When several
            // voices appear at once (e.g. Power Chord: 3-4 voices) and all
            // start at phase 0, their summatial is constructively reinforced
            // at the attack transient, producing a louder "thud" on the first
            // block. A de-correlated starting phase keeps the steady-state
            // tone identical but removes the transient brightness boost.
            if (amplitudes[v] == 0.0f)
            {
                phases[v] = v * 0.5 * juce::MathConstants<double>::pi;
            }

            // Retrigger the progressive attack envelope on every gate-open
            // transition (and on a true restart from silence). Previously the
            // linear fade-in only fired when the voice had fully decayed to
            // zero, so a quick gate re-open (after a short close) skipped the
            // gentle ramp and used the abrupt exponential one-pole, producing a
            // click. We now capture the current amplitude as the ramp start and
            // (re)arm the smoothstep counter every time the gate opens.
            const bool gateJustOpened = (voiceGate && (voicePrevGate[v] == 0));
            const bool startedFromSilence = (amplitudes[v] == 0.0f);
            if (gateJustOpened || startedFromSilence)
            {
                attackStartAmp[v] = static_cast<float> (amplitudes[v]);
                attackTotalSamples[v] = effectiveAttackSamples;
                attackSamplesRemaining[v] = effectiveAttackSamples;
            }
            // targetAmps is 1.0 when gate open, 0 otherwise. Actual per-voice
            // amplitude is computed using the 'volume' parameter passed to renderHarmonies
            targetAmps[v] = voiceGate ? 1.0f : 0.0f;
            // Remember gate state for the next block's retrigger detection.
            voicePrevGate[v] = voiceGate ? 1 : 0;
        }

        // Clear output buffer area we will write to (we accumulate)
        for (int ch = 0; ch < numChannels; ++ch)
            outputBuffer.clear(ch, 0, numSamples);

        const float color = juce::jlimit (0.0f, 1.0f, toneColor);

        // For each voice synthesize a band-limited sine and apply envelope smoothing
        for (int v = 0; v < numVoices; ++v)
        {
            const double freq = harmonyFrequencies[v];
            if (freq <= 0.0 || freq > currentSampleRate * 0.49)
                continue; // skip inaudible or above Nyquist

            double phase = phases[v];
            const double phaseInc = twoPi * freq / currentSampleRate;

            double amp = amplitudes[v];
            double tgt = targetAmps[v] * (double)volume * (1.0 - (double)blend);
            // Per-voice progressive attack state (configured at block start,
            // already accounting for gateActive). Reference the member directly
            // so decrements persist across blocks.
            const int attackTotal = attackTotalSamples[v];
            int& fadeLeft = attackSamplesRemaining[v];

            // Stereo placement per voice:
            // 1st=full right, 2nd=full left, 3rd=centre-right, 4th=centre-left
            float pan = 0.0f;
            switch (v)
            {
                case 0: pan =  1.0f;  break; // full right
                case 1: pan = -1.0f;  break; // full left
                case 2: pan =  0.5f;  break; // centre-right
                case 3: pan = -0.5f;  break; // centre-left
                default: pan = 0.0f;  break;
            }

            float leftGain = 1.0f, rightGain = 1.0f;
            if (numChannels > 1)
            {
                const float angle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
                leftGain = std::cos (angle);
                rightGain = std::sin (angle);
            }

            for (int i = 0; i < numSamples; ++i)
            {
                // smooth amplitude toward target
                if (amp < tgt)
                {
                    // Progressive (smoothstep / raised-cosine) fade-in while a
                    // fresh attack is still ramping. A smoothstep has ZERO slope
                    // at both ends, so unlike a linear ramp (constant slope) or a
                    // one-pole (infinite slope at t=0) it has no abrupt onset and
                    // no "thud" when several voices start together. The ramp is
                    // always (re)armed on a gate-open / silence restart, so every
                    // harmony-voice attack is gently eased in.
                    if (fadeLeft > 0 && attackTotal > 0)
                    {
                        const double progress = (double) (attackTotal - fadeLeft) / (double) attackTotal;
                        const double smooth = progress * progress * (3.0 - 2.0 * progress); // smoothstep
                        amp = attackStartAmp[v] + (tgt - (double) attackStartAmp[v]) * smooth;
                        --fadeLeft;
                    }
                    else
                    {
                        amp += (tgt - amp) * attackAlpha;
                    }
                }
                else
                {
                    amp += (tgt - amp) * releaseAlpha;
                }

                // Tone variants:
                // 0=Choir, 1=Bright, 2=Synth Lead, 3=Strings, 4=Guitar, 5=Vocoder-like
                const double p = phase;
                const double base = std::sin (p);
                const double h2 = std::sin (2.0 * p);
                const double h3 = std::sin (3.0 * p);
                const double h4 = std::sin (4.0 * p);
                double s = base;

                switch (toneMode)
                {
                    case 0: // Choir: dual-detuned sine + 2nd harmonic
                    {
                        const double slowDetune = 0.006 + 0.008 * color;
                        const double b1 = std::sin (p * (1.0 - slowDetune));
                        const double b2 = std::sin (p * (1.0 + slowDetune));
                        s = 0.42 * b1 + 0.42 * b2 + 0.16 * h2;
                        break;
                    }

                    case 1: // Organ: carrier modulated by a slowly-moving formant
                    default:
                    {
                        if (slowPhase.size() <= static_cast<size_t>(v))
                            slowPhase.resize (std::max (static_cast<size_t>(v) + 1, slowPhase.size() + 8),
                                              v * 0.7 * juce::MathConstants<double>::pi);
                        const double fRate = 2.8 + 2.4 * color;
                        const double formant = std::sin (p * fRate + slowPhase[v]);
                        const double carrier = 0.70 * base + 0.30 * h2;
                        s = carrier * (0.55 + 0.45 * formant);
                        slowPhase[v] += phaseInc * 0.0015;
                        if (slowPhase[v] > twoPi) slowPhase[v] -= twoPi;
                        break;
                    }
                }

                float sample = static_cast<float>(s * amp);
                // Clamp sample to avoid occasional spikes
                if (!std::isfinite(sample)) sample = 0.0f;
                if (sample > 1.0f) sample = 1.0f;
                if (sample < -1.0f) sample = -1.0f;

                if (amp > 0.0001 && (freq < 20.0 || freq > currentSampleRate * 0.49))
                {
                    OVT_LOG ("HarmonyEngine: generated voice out-of-range freq=" + juce::String(freq));
                }

                if (numChannels > 1)
                {
                    outputBuffer.getWritePointer(0)[i] += sample * leftGain;
                    outputBuffer.getWritePointer(1)[i] += sample * rightGain;
                }
                else
                {
                    outputBuffer.getWritePointer(0)[i] += sample;
                }

                phase += phaseInc;
                if (phase >= twoPi) phase -= twoPi;
            }

            phases[v] = phase;
            amplitudes[v] = static_cast<float>(amp);
            attackSamplesRemaining[v] = fadeLeft;
        }

        // If amplitudes have decayed below a tiny threshold, zero them and reset phases
        double maxAmp = 0.0;
        for (float a : amplitudes) maxAmp = juce::jmax<double>(maxAmp, std::abs(a));
        if (maxAmp < 1e-5)
        {
            for (size_t i = 0; i < amplitudes.size(); ++i)
            {
                amplitudes[i] = 0.0f;
                targetAmps[i] = 0.0f;
                phases[i] = 0.0;
            }
        }
    }

    bool HarmonyEngine::isActive() const
    {
        const float threshold = 1e-4f;
        for (float a : amplitudes)
            if (std::abs(a) > threshold) return true;
        for (float t : targetAmps)
            if (std::abs(t) > threshold) return true;
        return false;
    }

} // namespace ovtdsp
