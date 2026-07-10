#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace atdsp
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
            attackCoeff  = 1.0f - std::exp (-1.0f / (float) (0.005 * sr));
            releaseCoeff = 1.0f - std::exp (-1.0f / (float) (0.050 * sr));
        }

        void setEnabled (bool e) { enabled = e; }
        void setThresholdDb (float db)
        {
            thresholdLinear = std::pow (10.0f, db / 20.0f);
            // Hysteresis: open threshold is 6 dB above close threshold
            openThreshold = thresholdLinear * 2.0f; // +6 dB
        }
        bool isEnabled() const { return enabled; }

        void process (juce::AudioBuffer<float>& buffer)
        {
            if (! enabled) return;

            const int N = buffer.getNumSamples();
            const int ch = buffer.getNumChannels();
            if (N == 0 || ch == 0) return;

            // Mono RMS detection
            float sumSq = 0.0f;
            for (int c = 0; c < ch; ++c)
            {
                const float* d = buffer.getReadPointer (c);
                for (int i = 0; i < N; ++i) sumSq += d[i] * d[i];
            }
            const float rms = std::sqrt (sumSq / (float) (N * ch));

            // Hysteresis: use different thresholds for opening vs closing
            float target;
            if (gateOpen)
            {
                // Gate is open: close only if signal drops below close threshold
                target = (rms >= thresholdLinear) ? 1.0f : 0.0f;
                if (rms < thresholdLinear) gateOpen = false;
            }
            else
            {
                // Gate is closed: open only if signal rises above open threshold
                target = (rms >= openThreshold) ? 1.0f : 0.0f;
                if (rms >= openThreshold) gateOpen = true;
            }

            // Per-sample gain smoothing (state persists across buffers)
            float g = currentGain;
            const float coeff = (target > g) ? attackCoeff : releaseCoeff;
            for (int i = 0; i < N; ++i)
                g += (target - g) * coeff;

            // Apply smoothed gain to all samples
            for (int c = 0; c < ch; ++c)
            {
                float* d = buffer.getWritePointer (c);
                for (int i = 0; i < N; ++i)
                    d[i] *= g;
            }

            currentGain = g;
        }

    private:
        bool enabled = false;
        bool gateOpen = true;
        float thresholdLinear = 0.01f;
        float openThreshold = 0.02f;
        float currentGain = 1.0f;
        float attackCoeff = 0.0f;
        float releaseCoeff = 0.0f;
        double sampleRate = 44100.0;
    };
}
