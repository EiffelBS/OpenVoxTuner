// FormantPreserver.cpp
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include "FormantPreserver.h"
#include <cmath>

namespace ovtdsp
{
    FormantPreserver::FormantPreserver() = default;
    FormantPreserver::~FormantPreserver() = default;

    void FormantPreserver::prepare (double sr, int bs)
    {
        sampleRate = sr;
        blockSize = bs;
        channels.clear();

        // Default formant config (male voice typical F1-F4)
        formantConfigs[0] = { 500.0f, 2.5f, 6.0f };   // F1
        formantConfigs[1] = { 1500.0f, 2.0f, 5.0f };  // F2
        formantConfigs[2] = { 2500.0f, 1.8f, 4.0f };  // F3
        formantConfigs[3] = { 3500.0f, 1.5f, 3.0f };  // F4

        // 2 channels by default
        for (int i = 0; i < 2; ++i)
        {
            ChannelState s;
            // Initialize all 4 formants as passthrough
            for (int f = 0; f < 4; ++f)
            {
                updateBiquadCoefficients (s.formants[f], maxCutoffHz, 0.707f, 0.0f);
                // Smooth coefficients start as passthrough
                s.smooth[f].b0 = 1.0f; s.smooth[f].b1 = 0.0f; s.smooth[f].b2 = 0.0f;
                s.smooth[f].a1 = 0.0f; s.smooth[f].a2 = 0.0f;
            }
            channels.add (s);
        }
    }

    void FormantPreserver::reset()
    {
        for (auto& s : channels)
        {
            for (int f = 0; f < 4; ++f)
            {
                // Reset target filter states
                s.formants[f].z1 = 0.0f;
                s.formants[f].z2 = 0.0f;
                // Reset smoothed filter states AND coefficients to passthrough
                s.smooth[f].b0 = 1.0f; s.smooth[f].b1 = 0.0f; s.smooth[f].b2 = 0.0f;
                s.smooth[f].a1 = 0.0f; s.smooth[f].a2 = 0.0f;
                s.smooth[f].z1 = 0.0f; s.smooth[f].z2 = 0.0f;
            }
        }
    }

    void FormantPreserver::setFormant (int index, float freqHz, float q, float gainDb)
    {
        if (index < 0 || index >= 4) return;
        formantConfigs[index] = { juce::jlimit (20.0f, maxCutoffHz, freqHz),
                                   juce::jlimit (0.1f, 20.0f, q),
                                   gainDb };
        // Will be recalculated on next process() call
    }

    void FormantPreserver::updateBiquadCoefficients (ChannelState::BiquadState& s, float freqHz, float q, float gainDb)
    {
        // Peaking EQ biquad (RBJ cookbook)
        // Handles gain = 0dB as passthrough (b0=1, others=0)
        float clampedFreq = juce::jlimit (20.0f, maxCutoffHz, freqHz);
        float A = std::pow (10.0f, gainDb / 40.0f);
        float w0 = 2.0f * juce::MathConstants<float>::pi * clampedFreq / static_cast<float> (sampleRate);
        float cosw0 = std::cos (w0);
        float sinw0 = std::sin (w0);
        float alpha = sinw0 / (2.0f * q);

        if (std::abs (gainDb) < 0.01f)
        {
            // Passthrough
            s.b0 = 1.0f; s.b1 = 0.0f; s.b2 = 0.0f;
            s.a1 = 0.0f; s.a2 = 0.0f;
            return;
        }

        float a0 = 1.0f + alpha / A;
        s.b0 = (1.0f + alpha * A) / a0;
        s.b1 = (-2.0f * cosw0) / a0;
        s.b2 = (1.0f - alpha * A) / a0;
        s.a1 = (-2.0f * cosw0) / a0;
        s.a2 = (1.0f - alpha / A) / a0;
    }

    void FormantPreserver::updateAllpassCoefficients (ChannelState::BiquadState& s, float freqHz, float q)
    {
        // Allpass biquad (RBJ cookbook). |H(z)| = 1 everywhere; the phase
        // response wraps around `freqHz`, which physically shifts the
        // formant without colouring the spectral envelope. This is the
        // building block of the P3 (allpass cascade) strategy.
        const float clampedFreq = juce::jlimit (20.0f, maxCutoffHz, freqHz);
        const float w0 = 2.0f * juce::MathConstants<float>::pi * clampedFreq / static_cast<float> (sampleRate);
        const float cosw0 = std::cos (w0);
        const float alpha = std::sin (w0) / (2.0f * juce::jmax (0.1f, q));

        const float a0 = 1.0f + alpha;
        s.b0 = (1.0f - alpha) / a0;
        s.b1 = (-2.0f * cosw0) / a0;
        s.b2 = 1.0f;
        s.a1 = (-2.0f * cosw0) / a0;
        s.a2 = (1.0f - alpha) / a0;
    }

    void FormantPreserver::updateAllFormants (ChannelState& s, float compensationRatio, float shiftRatio)
    {
        for (int f = 0; f < 4; ++f)
        {
            // Compensation: formants move opposite to pitch shift
            // If pitch goes up (ratio > 1), formants need to go down before PSOLA
            // The compensationRatio moves them partially (Current) or fully (P0)
            // shiftRatio applies user formant shift on top
            float targetFreq = getFormantFreqHz (f) * compensationRatio * shiftRatio;
            targetFreq = juce::jlimit (20.0f, maxCutoffHz, targetFreq);
            updateBiquadCoefficients (s.formants[f], targetFreq,
                                      formantConfigs[f].q * qMultiplier,
                                      formantConfigs[f].gainDb);
        }
    }

    void FormantPreserver::processChannel (float* data, int numSamples, ChannelState& s)
    {
        // Smooths the target coefficients toward the applied coefficients
        // (one step per block, but applied at every sample of the block to
        // stay buffer-size independent). This prevents any discontinuity at
        // the output when the pitch ratio changes abruptly (start of a sung
        // note).
        for (int f = 0; f < 4; ++f)
        {
            s.smooth[f].b0 += biquadSmoothAlpha * (s.formants[f].b0 - s.smooth[f].b0);
            s.smooth[f].b1 += biquadSmoothAlpha * (s.formants[f].b1 - s.smooth[f].b1);
            s.smooth[f].b2 += biquadSmoothAlpha * (s.formants[f].b2 - s.smooth[f].b2);
            s.smooth[f].a1 += biquadSmoothAlpha * (s.formants[f].a1 - s.smooth[f].a1);
            s.smooth[f].a2 += biquadSmoothAlpha * (s.formants[f].a2 - s.smooth[f].a2);
        }

        // Process through all 4 formants in series, using the SMOOTHED
        // coefficients AND THEIR OWN(delay states s.smooth[f].z1/z2) to
        // avoid clicks caused by coefficient/state mismatch.
        for (int i = 0; i < numSamples; ++i)
        {
            float x = data[i];

            // F1
            float y = s.smooth[0].b0 * x + s.smooth[0].z1;
            s.smooth[0].z1 = s.smooth[0].b1 * x - s.smooth[0].a1 * y + s.smooth[0].z2;
            s.smooth[0].z2 = s.smooth[0].b2 * x - s.smooth[0].a2 * y;
            x = y;

            // F2
            y = s.smooth[1].b0 * x + s.smooth[1].z1;
            s.smooth[1].z1 = s.smooth[1].b1 * x - s.smooth[1].a1 * y + s.smooth[1].z2;
            s.smooth[1].z2 = s.smooth[1].b2 * x - s.smooth[1].a2 * y;
            x = y;

            // F3
            y = s.smooth[2].b0 * x + s.smooth[2].z1;
            s.smooth[2].z1 = s.smooth[2].b1 * x - s.smooth[2].a1 * y + s.smooth[2].z2;
            s.smooth[2].z2 = s.smooth[2].b2 * x - s.smooth[2].a2 * y;
            x = y;

            // F4
            y = s.smooth[3].b0 * x + s.smooth[3].z1;
            s.smooth[3].z1 = s.smooth[3].b1 * x - s.smooth[3].a1 * y + s.smooth[3].z2;
            s.smooth[3].z2 = s.smooth[3].b2 * x - s.smooth[3].a2 * y;

            data[i] = y;
        }
    }

    void FormantPreserver::process (juce::AudioBuffer<float>& buffer, float ratio)
    {
        // Disable if the module is off and there is neither pitch shifting nor formant shift.
        if (!enabled || (std::abs (ratio - 1.0f) < 1e-6f && std::abs(shiftSemitones) < 1e-6f))
            return;

        // Compensation computation: move the formants in the direction
        // opposite to the ratio (compensation). If ratio > 1 (the pitch will
        // be raised via PSOLA), lower the formants here by (1/ratio).
        // Note: this is an approximation, but it already preserves the
        // timbre much better than a pure PSOLA.
        const float r = juce::jlimit (0.25f, 4.0f, ratio);

        // Compensation ratio. P0 strategy uses full 1/r compensation (moves
        // formants all the way back to their natural place); Current uses the
        // partial 1/sqrt(r) compromise (Moulines & Charpentier).
        const float compensationRatio = (strategy == Strategy::P0)
                                            ? (1.0f / r)
                                            : (1.0f / std::sqrt (r));

        // Apply the manual formant shift on top
        const float shiftRatio = std::pow (2.0f, shiftSemitones / 12.0f);

        const int numChannels = juce::jmin (2, buffer.getNumChannels());

        // Ensure we have enough channel states
        while (channels.size() < numChannels)
        {
            ChannelState s;
            for (int f = 0; f < 4; ++f)
            {
                updateBiquadCoefficients (s.formants[f], maxCutoffHz, 0.707f, 0.0f);
                s.smooth[f].z1 = 0.0f;
                s.smooth[f].z2 = 0.0f;
            }
            channels.add (s);
        }

        if (mode == Mode::Legacy)
        {
            // Legacy mode: single peaking EQ at reference frequency (backward compatible)
            const float cutoff = juce::jlimit (100.0f, maxCutoffHz,
                                               getFormantFreqHz (0) * compensationRatio * shiftRatio);
            for (int ch = 0; ch < numChannels; ++ch)
            {
                // Use first formant slot for legacy
                updateBiquadCoefficients (channels.getReference(ch).formants[0], cutoff, 2.0f, 8.0f);
                processChannel (buffer.getWritePointer (ch), buffer.getNumSamples(),
                                channels.getReference(ch));
            }
        }
        else if (mode == Mode::MultiFormant)
        {
            // MultiFormant mode: F1-F4 peaking-EQ cascade
            for (int ch = 0; ch < numChannels; ++ch)
            {
                updateAllFormants (channels.getReference(ch), compensationRatio, shiftRatio);
                processChannel (buffer.getWritePointer (ch), buffer.getNumSamples(),
                                channels.getReference(ch));
            }
        }
        else
        {
            // Allpass mode (P3): F1-F4 allpass biquad cascade. Magnitude
            // is unity at every frequency; the phase response shifts the
            // formants without colouring the spectral envelope. More
            // transparent than the peaking-EQ cascade used in MultiFormant.
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto& sch = channels.getReference(ch);
                for (int f = 0; f < 4; ++f)
                {
                    float targetFreq = juce::jlimit (20.0f, maxCutoffHz,
                                                      getFormantFreqHz (f) * compensationRatio * shiftRatio);
                    updateAllpassCoefficients (sch.formants[f], targetFreq,
                                              formantConfigs[f].q * qMultiplier);
                }
                processChannel (buffer.getWritePointer (ch), buffer.getNumSamples(), sch);
            }
        }
    }
}



