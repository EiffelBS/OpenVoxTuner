#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace ovtdsp
{
    /**
     * Simple noise gate applied to the input audio before pitch detection.
     * Uses RMS-based level detection with smooth attack/release envelopes
     * and hysteresis to prevent chattering near the threshold.
     */
    class NoiseGate
    {
    public:
        NoiseGate() = default;

        void prepare (double sr)
    {
        sampleRate = sr;
        // Slower, smoother attack (15 ms) so the gate opening front does not
        // produce a steep transient that the downstream harmony engine would
        // then amplify. Release stays at 50 ms.
        attackCoeff  = 1.0f - std::exp (-1.0f / (float) (0.015 * sr));
        releaseCoeff = 1.0f - std::exp (-1.0f / (float) (0.050 * sr));
        rmsState = 0.0f;
    }

        void setEnabled (bool e) { enabled = e; }
        void setThresholdDb (float db)
        {
            thresholdLinear = std::pow (10.0f, db / 20.0f);
            // Hysteresis: open threshold is 6 dB above close threshold
            openThreshold = thresholdLinear * 2.0f; // +6 dB
        }
        bool isEnabled() const { return enabled; }
        float getCurrentGain() const { return currentGain; }

        void process (juce::AudioBuffer<float>& buffer)
        {
            if (! enabled) return;

            const int N = buffer.getNumSamples();
            const int ch = buffer.getNumChannels();
            if (N == 0 || ch == 0) return;

            // Per-sample RMS detection (smoothed). A block-wide RMS would
            // already be high on a block where the voice starts mid-block, so
            // the gate would slam open for the whole block and create a steep
            // front. Sample-accurate RMS lets the gate track the real onset.
            const float rmsCoeff = 1.0f - std::exp (-1.0f / (float) (0.010 * sampleRate));

            float g = currentGain;
            for (int i = 0; i < N; ++i)
            {
                float sumSq = 0.0f;
                for (int c = 0; c < ch; ++c)
                    sumSq += buffer.getSample (c, i) * buffer.getSample (c, i);
                const float inst = std::sqrt (sumSq / (float) ch);
                rmsState += (inst - rmsState) * rmsCoeff;
                const float rms = rmsState;

                // Hysteresis: use different thresholds for opening vs closing
                if (gateOpen)
                {
                    // Gate is open: close only if signal drops below close threshold
                    if (rms < thresholdLinear) gateOpen = false;
                }
                else
                {
                    // Gate is closed: open only if signal rises above open threshold
                    if (rms >= openThreshold) gateOpen = true;
                }
                const float target = gateOpen ? 1.0f : 0.0f;

                // Per-sample gain smoothing (state persists across buffers)
                const float coeff = (target > g) ? attackCoeff : releaseCoeff;
                g += (target - g) * coeff;

                for (int c = 0; c < ch; ++c)
                    buffer.setSample (c, i, buffer.getSample (c, i) * g);
            }

            currentGain = g;
        }

    private:
        bool enabled = false;
        bool gateOpen = true;
        float thresholdLinear = 0.01f;
        float openThreshold = 0.02f;
        float currentGain = 1.0f;
        float rmsState = 0.0f;      // smoothed per-sample RMS level for detection
        float attackCoeff = 0.0f;
        float releaseCoeff = 0.0f;
        double sampleRate = 44100.0;
    };
}
