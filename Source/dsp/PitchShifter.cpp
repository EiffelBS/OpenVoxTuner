#include "PitchShifter.h"
#include <cmath>

#if JUCE_DEBUG
 #define OVT_LOG(msg) juce::Logger::writeToLog (msg)
#else
 #define OVT_LOG(msg) do { } while (false)
#endif

// Definition of global debug counter
std::atomic<int> gPitchShifterGrainEvents { 0 };

namespace atdsp
{
    PitchShifter::PitchShifter() = default;

    void PitchShifter::prepare (double sr, int bs)
    {
        sampleRate = sr;
        latencySamples = static_cast<int>(sampleRate * (latencyMs * 0.001f));

        ringBuffer.setSize(2, bufferSize);
        ringBuffer.clear();

        reset();
    }

    void PitchShifter::reset()
    {
        ringBuffer.clear();
        absoluteWritePos = 0;
        currentRatio = 1.0f;
        currentFormantRatio = 1.0f;
        outPhase = 0.0;
        lastGrainCenter = 0.0;

        for (int i = 0; i < MAX_GRAINS; ++i) {
            grains[i].active = false;
        }
    }

    void PitchShifter::setLatencyMs (float newLatencyMs)
    {
        latencyMs = juce::jlimit (8.0f, 40.0f, newLatencyMs);
        latencySamples = static_cast<int>(sampleRate * (latencyMs * 0.001f));
    }

    double PitchShifter::findBestOffset (double idealReadPos, double targetToMatch, double searchWindowMs, float f0, double maxOffset) const
    {
        double searchRange = searchWindowMs * sampleRate / 1000.0;
        if (f0 > 40.0f) {
            double period = sampleRate / f0;
            searchRange = juce::jmax(searchRange, period * 1.2);
        }

        int64_t startSearch = static_cast<int64_t>(idealReadPos - searchRange);
        int64_t endSearch = static_cast<int64_t>(idealReadPos + searchRange);
        
        int64_t maxSafePos = static_cast<int64_t>(idealReadPos + maxOffset);
        if (endSearch > maxSafePos) endSearch = maxSafePos;
        if (startSearch > endSearch) startSearch = endSearch;
        
        const int templateSize = 64; 
        
        float maxCorr = -1.0f;
        double bestOffset = 0.0;
        
        const float* ringData = ringBuffer.getReadPointer(0);
        int64_t baseTarget = static_cast<int64_t>(targetToMatch);
        
        for (int64_t p = startSearch; p <= endSearch; p += 2) // Step 2 for speed
        {
            float corr = 0.0f;
            float normA = 0.0f;
            float normB = 0.0f;
            
            for (int i = 0; i < templateSize; i += 2)
            {
                float a = ringData[static_cast<uint64_t>(p + i) & bufferMask];
                float b = ringData[static_cast<uint64_t>(baseTarget + i) & bufferMask]; 
                
                corr += a * b;
                normA += a * a;
                normB += b * b;
            }
            
            float denom = std::sqrt(normA * normB);
            if (denom > 1e-6f) {
                corr /= denom;
            }
            
            if (corr > maxCorr) {
                maxCorr = corr;
                bestOffset = static_cast<double>(p) - idealReadPos;
            }
        }
        
        return bestOffset;
    }

    float PitchShifter::getInterpolatedSample(int channel, double readPos) const
    {
        int64_t pos1 = static_cast<int64_t>(std::floor(readPos));
        int64_t pos2 = pos1 + 1;
        float frac = static_cast<float>(readPos - static_cast<double>(pos1));
        
        const float* data = ringBuffer.getReadPointer(channel);
        float a = data[static_cast<uint64_t>(pos1) & bufferMask];
        float b = data[static_cast<uint64_t>(pos2) & bufferMask];
        return a + frac * (b - a);
    }

    void PitchShifter::process (juce::AudioBuffer<float>& buffer, float pitchRatio, float formantRatio, float f0)
    {
        process (static_cast<const juce::AudioBuffer<float>&> (buffer), buffer, pitchRatio, formantRatio, f0);
    }

    void PitchShifter::process (const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output, float pitchRatio, float formantRatio, float f0)
    {
        const int numSamples = input.getNumSamples();
        const int numChannels = input.getNumChannels();
        if (numSamples == 0) return;

        // Defense: ensure ring buffer is properly initialized before touching it.
        // ringBuffer.setSize(2, bufferSize) is called in prepare(), but may have
        // been constructed yet (e.g. if prepare() wasn't called, or bufferSize == 0).
        if (ringBuffer.getNumSamples() == 0 || ringBuffer.getNumChannels() < 2)
        {
            // Fallback: copy input to output silently without processing.
            for (int ch = 0; ch < numChannels; ++ch)
                output.copyFrom (ch, 0, input, ch, 0, numSamples);
            return;
        }

        // Ensure output layout matches input
        // NOTE: avoidReallocating must be false for the first call (buffer is
        // default-constructed with null data); true would skip the allocation!
        if (output.getNumChannels() != numChannels || output.getNumSamples() != numSamples)
            output.setSize (numChannels, numSamples, false, true, false);

        // Defense en profondeur : valider les ratios d'entree pour eviter
        // de propager NaN, Inf ou ratio <= 0 au pipeline de synthese.
        if (pitchRatio <= 0.0f || std::isnan(pitchRatio) || std::isinf(pitchRatio))
        {
            pitchRatio = 1.0f;  // ratio invalide -> pass-through neutre
        }
        if (formantRatio <= 0.0f || std::isnan(formantRatio) || std::isinf(formantRatio))
        {
            formantRatio = 1.0f;
        }

        // Throttle verbose debug logs to once per second
        static uint32_t lastLogMs = 0;
        uint32_t nowMs = juce::Time::getMillisecondCounter();
        bool doLog = false;
        if (nowMs - lastLogMs > 1000)
        {
            lastLogMs = nowMs;
            doLog = true;
        }

        pitchRatio = juce::jlimit(0.25f, 4.0f, pitchRatio);
        formantRatio = juce::jlimit(0.25f, 4.0f, formantRatio);

        const float alpha = 0.005f;

        for (int i = 0; i < numSamples; ++i)
        {
            currentRatio += alpha * (pitchRatio - currentRatio);
            currentFormantRatio += alpha * (formantRatio - currentFormantRatio);

            ringBuffer.setSample(0, static_cast<int>(absoluteWritePos & bufferMask), input.getSample(0, i));
            if (numChannels > 1)
                ringBuffer.setSample(1, static_cast<int>(absoluteWritePos & bufferMask), input.getSample(1, i));
            else
                ringBuffer.setSample(1, static_cast<int>(absoluteWritePos & bufferMask), input.getSample(0, i));

            double virtualInputTime = static_cast<double>(absoluteWritePos) - latencySamples;

            double target_f0 = f0 * currentRatio;
            target_f0 = juce::jlimit(20.0, 2000.0, target_f0);

            if (f0 > 40.0f) {
                outPhase += target_f0 / sampleRate;
            } else {
                outPhase += 100.0 / sampleRate;
            }

            if (outPhase >= 1.0) {
                outPhase -= 1.0;

                double Tin = (f0 > 40.0f) ? (sampleRate / f0) : (sampleRate * 0.010);
                double Tout = (f0 > 40.0f) ? (sampleRate / target_f0) : (sampleRate * 0.010);
                double F = currentFormantRatio;

                double outLength = std::max(2.0 * Tin / F, 2.0 * Tout);

                double maxSafeOffset = latencySamples - (outLength * F / 2.0) - 10.0;
                if (maxSafeOffset < 0.0) maxSafeOffset = 0.0;

                double idealCenter = virtualInputTime;
                double bestOffset = 0.0;

                if (f0 > 40.0f) {
                    if (idealCenter - lastGrainCenter > sampleRate * 0.05) {
                        lastGrainCenter = 0.0;
                    } else if (lastGrainCenter > 0.0) {
                        double targetToMatch = lastGrainCenter + Tin;
                        bestOffset = findBestOffset(idealCenter, targetToMatch, 10.0, f0, maxSafeOffset);
                    }
                }

                if (bestOffset > maxSafeOffset) bestOffset = maxSafeOffset;

                double center = idealCenter + bestOffset;
                lastGrainCenter = center;

                int gIdx = -1;
                for(int j=0; j<MAX_GRAINS; ++j) {
                    if (!grains[j].active) { gIdx = j; break; }
                }

                if (gIdx >= 0) {
                    grains[gIdx].readPos = center - (outLength * F / 2.0);
                    grains[gIdx].speed = F;
                    grains[gIdx].phase = 0.0;
                    grains[gIdx].phaseInc = 1.0 / outLength;
                    grains[gIdx].gain = Tout / (0.5 * outLength);
                    grains[gIdx].active = true;
                    gPitchShifterGrainEvents.fetch_add (1, std::memory_order_relaxed);
                    static uint32_t lastGrainLogMs = 0;
                    uint32_t nowG = juce::Time::getMillisecondCounter();
                    if (nowG - lastGrainLogMs > 1000)
                    {
                        lastGrainLogMs = nowG;
                        OVT_LOG ("PitchShifter: created grain idx=" + juce::String(gIdx) +
                                                  " center=" + juce::String(center, 3) +
                                                  " outLength=" + juce::String(outLength,3) +
                                                  " F=" + juce::String(F,3));
                    }
                }
            }

            float outL = 0.0f, outR = 0.0f;
            for (int j = 0; j < MAX_GRAINS; ++j) {
                if (grains[j].active) {
                    double win = 0.5 - 0.5 * std::cos(grains[j].phase * 2.0 * juce::MathConstants<double>::pi);
                    float sampleGain = static_cast<float>(win * grains[j].gain);

                    outL += getInterpolatedSample(0, grains[j].readPos) * sampleGain;
                    outR += getInterpolatedSample(1, grains[j].readPos) * sampleGain;

                    grains[j].readPos += grains[j].speed;
                    grains[j].phase += grains[j].phaseInc;

                    if (grains[j].phase >= 1.0) {
                        grains[j].active = false;
                    }
                }
            }

            // Defensive write: guard against output buffer corruption
            // (0xFFFFFFFFFFFFFFFF crash). Check if output has valid data.
            if (output.getNumSamples() == numSamples && output.getNumChannels() > 0)
            {
                output.setSample(0, i, outL);
                if (numChannels > 1)
                    output.setSample(1, i, outR);
            }

            if (doLog && i == 0)
            {
                int activeCount = 0;
                for (int j = 0; j < MAX_GRAINS; ++j) if (grains[j].active) ++activeCount;
                OVT_LOG ("PitchShifter.process: f0=" + juce::String(f0,3) +
                                          " pitchRatio=" + juce::String(pitchRatio,3) +
                                          " grainsActive=" + juce::String(activeCount) +
                                          " out0=" + juce::String(outL,6));
            }

            absoluteWritePos++;
        }

    }

    // end of process

    // Debug: force-create a test grain (calls from host/UI)
void atdsp::PitchShifter::forceCreateTestGrain()
{
    int gIdx = -1;
    for (int j = 0; j < MAX_GRAINS; ++j)
        if (! grains[j].active) { gIdx = j; break; }
    if (gIdx < 0) return;

    double center = static_cast<double>(absoluteWritePos);
    double Tin = (sampleRate > 0.0) ? (sampleRate / 100.0) : 480.0;
    double Tout = Tin;
    double F = 1.0;
    double outLength = std::max(2.0 * Tin / F, 2.0 * Tout);

    grains[gIdx].readPos = center - (outLength * F / 2.0);
    grains[gIdx].speed = F;
    grains[gIdx].phase = 0.0;
    grains[gIdx].phaseInc = 1.0 / outLength;
    grains[gIdx].gain = Tout / (0.5 * outLength);
    grains[gIdx].active = true;

    gPitchShifterGrainEvents.fetch_add (1, std::memory_order_relaxed);
}

}
