#include "HarmonyEngine.h"
#include <cmath>
#include <vector>

#if JUCE_DEBUG
 #define OVT_LOG(msg) juce::Logger::writeToLog (msg)
#else
 #define OVT_LOG(msg) do { } while (false)
#endif

namespace atdsp
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
                case HarmonyType::ThirdBelow:      result.add (freqFromSemitoneOffset (baseFreq, -3)); break;
                case HarmonyType::ThirdAbove:      result.add (freqFromSemitoneOffset (baseFreq,  3)); break;
                case HarmonyType::ThirdBelowAbove: result.add (freqFromSemitoneOffset (baseFreq, -3)); result.add (freqFromSemitoneOffset (baseFreq, 3)); break;
                case HarmonyType::FifthBelow:      result.add (freqFromSemitoneOffset (baseFreq, -7)); break;
                case HarmonyType::FifthAbove:      result.add (freqFromSemitoneOffset (baseFreq,  7)); break;
                case HarmonyType::FifthBelowAbove: result.add (freqFromSemitoneOffset (baseFreq, -7)); result.add (freqFromSemitoneOffset (baseFreq, 7)); break;
                case HarmonyType::VocalStack3:     result.add (freqFromSemitoneOffset (baseFreq,  3)); result.add (freqFromSemitoneOffset (baseFreq, 7)); result.add (freqFromSemitoneOffset (baseFreq, 12)); break;
                case HarmonyType::VocalStack4:     result.add (freqFromSemitoneOffset (baseFreq, -3)); result.add (freqFromSemitoneOffset (baseFreq, 3)); result.add (freqFromSemitoneOffset (baseFreq, 7)); result.add (freqFromSemitoneOffset (baseFreq, 12)); break;
                case HarmonyType::PowerChord:      result.add (freqFromSemitoneOffset (baseFreq,  7)); result.add (freqFromSemitoneOffset (baseFreq, 12)); break;
                case HarmonyType::ParallelThird:   result.add (freqFromSemitoneOffset (baseFreq,  3)); break;
                case HarmonyType::Drone:           result.add (baseFreq); break;
                default: break;
            }
            return result;
        }

        // Map harmony types to degree offsets (in scale degrees)
        juce::Array<int> degreeOffsets;
        const int scaleSize = scaleIntervals.size();
        switch (harmonyType)
        {
            case HarmonyType::ThirdBelow:      degreeOffsets.add(-2); break;
            case HarmonyType::ThirdAbove:      degreeOffsets.add( 2); break;
            case HarmonyType::ThirdBelowAbove: degreeOffsets.add(-2); degreeOffsets.add( 2); break;
            case HarmonyType::FifthBelow:      degreeOffsets.add(-4); break;
            case HarmonyType::FifthAbove:      degreeOffsets.add( 4); break;
            case HarmonyType::FifthBelowAbove: degreeOffsets.add(-4); degreeOffsets.add( 4); break;
            case HarmonyType::VocalStack3:     degreeOffsets.add( 2); degreeOffsets.add(4); degreeOffsets.add(scaleSize); break;
            case HarmonyType::VocalStack4:     degreeOffsets.add(-2); degreeOffsets.add(2); degreeOffsets.add(4); degreeOffsets.add(scaleSize); break;
            case HarmonyType::PowerChord:      degreeOffsets.add( 4); degreeOffsets.add(scaleSize); break;
            case HarmonyType::ParallelThird:   degreeOffsets.add( 1); break;
            case HarmonyType::Drone:           degreeOffsets.add(-(scaleSize)); break;
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
                                         int key, int scaleIndex, float blend, int toneMode, float toneColor)
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

        // compute attack/release smoothing coefficients (simple one-pole)
        const double attackAlpha = 1.0 - std::exp(-1000.0 / (attackMs * currentSampleRate));
        const double releaseAlpha = 1.0 - std::exp(-1000.0 / (releaseMs * currentSampleRate));

        // initialize amplitudes and targets if needed
        for (int v = 0; v < numVoices; ++v)
        {
            if (amplitudes[v] == 0.0f)
                phases[v] = 0.0; // start phase at zero for new voices
            // targetAmps is 1.0 when gate open, 0 otherwise. Actual per-voice
            // amplitude is computed using the 'volume' parameter passed to renderHarmonies
            targetAmps[v] = voiceGate ? 1.0f : 0.0f;
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
                    amp += (tgt - amp) * attackAlpha;
                else
                    amp += (tgt - amp) * releaseAlpha;

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
                    case 0: // Choir
                    {
                        const double slowDetune = 0.006 + 0.008 * color;
                        const double b1 = std::sin (p * (1.0 - slowDetune));
                        const double b2 = std::sin (p * (1.0 + slowDetune));
                        s = 0.42 * b1 + 0.42 * b2 + 0.16 * h2;
                        break;
                    }

                    case 1: // Bright
                        s = (0.78 - 0.10 * color) * base
                          + (0.17 + 0.10 * color) * h2
                          + (0.05 + 0.06 * color) * h3;
                        break;

                    case 2: // Synth Lead
                    {
                        const double lead = 0.72 * base + 0.20 * h2 + 0.08 * h3;
                        const double sat = std::tanh ((1.2 + 1.0 * color) * lead);
                        s = sat;
                        break;
                    }

                    case 3: // Strings
                        s = (0.66 - 0.08 * color) * base
                          + (0.19 + 0.06 * color) * h2
                          + (0.10 + 0.05 * color) * h3
                          + (0.05 + 0.04 * color) * h4;
                        break;

                    case 4: // Guitar
                    {
                        const double cyclePos = p / twoPi; // 0..1 in cycle
                        const double pickEnv = std::exp (-(3.2 + 2.5 * color) * cyclePos);
                        const double body = 0.70 * base + 0.20 * h2 + 0.10 * h3;
                        s = body * (0.68 + 0.32 * pickEnv);
                        break;
                    }

                    case 5: // Vocoder-like
                    default:
                    {
                        const double formant = std::sin (p * (2.8 + 2.4 * color));
                        const double carrier = 0.70 * base + 0.30 * h2;
                        s = carrier * (0.55 + 0.45 * formant);
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

} // namespace atdsp
