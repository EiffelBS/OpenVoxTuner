// FormantPreserver.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace ovtdsp
{
    /**
     * Shifts the formants of a signal in the direction opposite to a pitch
     * transposition. Must be called BEFORE the PSOLA.
     */
    class FormantPreserver
    {
    public:
        enum class Mode
    {
        Legacy,       // Single peaking EQ at 500Hz (original)
        MultiFormant, // F1-F4 peaking-EQ formant preservation
        Allpass       // F1-F4 allpass-cascade formant shifting (P3)
    };

        // Strategy selects the compensation law (see formant_strategy APVTS param):
        //   Current = partial 1/sqrt(r) compensation with fixed male-default centers.
        //   P0      = full 1/r compensation with voice-type-aware formant centers.
        // P1/P2 (LPC cross-synthesis) are handled by LpcFormantPreserver, not here.
        enum class Strategy
        {
            Current = 0,
            P0 = 1
        };

        FormantPreserver();
        ~FormantPreserver();

        void prepare (double sampleRate, int blockSize);
        void reset();

        /// Applies the formant shifting to the buffer.
        /// @param ratio transposition ratio (1.0 = passthrough)
        void process (juce::AudioBuffer<float>& buffer, float ratio);

        void setEnabled (bool b) { enabled = b; }
        bool isEnabled() const   { return enabled; }

        /// Sets the manual formant shift in semitones
        void setFormantShift (float semitones) { shiftSemitones = semitones; }

        /// Selects the formant preservation mode
        void setMode (Mode m) { mode = m; }
        Mode getMode() const { return mode; }

        /// Selects the compensation strategy (Current or P0).
        void setStrategy (Strategy s) { strategy = s; }
        Strategy getStrategy() const { return strategy; }

        /// Q multiplier applied to the formant resonance (1.0 = original Q,
        /// 1.3 = 30% sharper peaks). Used to make P1/P2 audibly distinct
        /// from P0/Current without changing the overall gain level.
        void setQMultiplier (float q) { qMultiplier = juce::jlimit (0.1f, 5.0f, q); }
        float getQMultiplier() const { return qMultiplier; }

        /// Biquad smoothing alpha (0..1). Larger = faster tracking. P2 uses
        /// a higher value than P0/Current to react more quickly to pitch
        /// changes.
        void setSmoothingAlpha (float a) { biquadSmoothAlpha = juce::jlimit (0.001f, 1.0f, a); }
        float getSmoothingAlpha() const { return biquadSmoothAlpha; }

        /// Sets the voice type (0=Universal,1=Bass,2=Baritone,3=Tenor,4=Alto,5=Soprano)
        /// used to pick the formant centers in P0 mode.
        void setVoiceType (int vt) { voiceType = juce::jlimit (0, 5, vt); }

        /// Configures a specific formant (MultiFormant mode only)
        /// @param index 0=F1, 1=F2, 2=F3, 3=F4
        /// @param freqHz formant frequency in Hz
        /// @param q quality (resonance)
        /// @param gainDb gain in dB
        void setFormant (int index, float freqHz, float q, float gainDb);

    private:
        // Voice-type-aware formant center frequencies (Hz) for F1-F4.
        // Index = voice type (0=Universal..5=Soprano); sub-index = formant 0..3.
        // Used in P0 strategy to place the pre-warp peaking-EQs on the real
        // formants instead of the fixed male-default centers (fixes gap G1/G6).
        static constexpr float voiceTypeTable[6][4] = {
            { 550.0f, 1700.0f, 2550.0f, 3500.0f }, // Universal (neutral blend)
            { 430.0f, 1150.0f, 2200.0f, 3200.0f }, // Bass
            { 480.0f, 1350.0f, 2400.0f, 3400.0f }, // Baritone
            { 520.0f, 1500.0f, 2500.0f, 3500.0f }, // Tenor
            { 600.0f, 1700.0f, 2700.0f, 3700.0f }, // Alto
            { 650.0f, 1900.0f, 2900.0f, 3900.0f }  // Soprano
        };

        // Returns the formant center frequency (Hz) for formant f, honoring the
        // active strategy (voice-type table for P0, user config for Current).
        float getFormantFreqHz (int f) const
        {
            if (strategy == Strategy::P0)
                return voiceTypeTable[voiceType][f];
            return formantConfigs[f].freqHz;
        }
        double sampleRate = 44100.0;
        int blockSize = 512;
        
        float shiftSemitones = 0.0f;

        // IIR filter coefficients (one biquad per formant per channel).
        struct ChannelState
        {
            // One biquad per formant (max 4).
            // `formants` stores the TARGET coefficients (recomputed every
            // block from the ratio). `smooth` stores the coefficients
            // APPLIED to the signal along with their own delay states
            // (fixes pops/clicks caused by coefficient/state mismatch).
            struct BiquadState
            {
                float a1 = 0.0f, a2 = 0.0f;
                float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
                float z1 = 0.0f, z2 = 0.0f;
            };
            struct BiquadSmooth
            {
                float a1 = 0.0f, a2 = 0.0f;
                float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
                float z1 = 0.0f, z2 = 0.0f; // delay states OWNED by smoothed coefficients
            };
            BiquadState  formants[4];   // target coefficients (recomputed per block)
            BiquadSmooth smooth[4];     // coefficients + states applied to the signal
        };
        juce::Array<ChannelState> channels;

        // Biquad smoothing coefficient (one step per block, applied to every
        // sample of the block to stay buffer-size independent).
        //
        // 2026-07-23 (Fix AZ): increased from 0.002 (~2.9s TC at 256/44100,
        // which produced a 5Hz warble of formant frequencies when the input
        // pitch had any vibrato modulation) to 0.05 (~115ms TC). At 115ms
        // the smoother is fast enough to track the typical 5Hz vibrato
        // (|H(5Hz)| ~ 0.42, so the biquad response moves with ~half the
        // vibrato amplitude, which is the right perceptual balance:
        // "formants follow pitch" without being completely static), but
        // still smooths out per-block YIN jitter and OLA retarget steps.
        // Without this fix, the FormantPreserver acted as a 5Hz bandpass
        // on its own compensation ratio (1/sqrt(targetRatio)) and produced
        // an audible "warble" that the user reports as a "scratch" with
        // Flex>0 + Speed=0. The previous 2.9s TC was an unintended
        // side-effect of an old buffer-size-dependent formula.
        float biquadSmoothAlpha = 0.05f;

        // Default formant configuration (typical male voice F1-F4)
        struct FormantConfig
        {
            float freqHz = 500.0f;
            float q = 2.0f;
            float gainDb = 8.0f;
        };
        FormantConfig formantConfigs[4];

        Mode mode = Mode::Legacy;

        // Active compensation strategy (Current or P0). Selected by the
        // formant_strategy APVTS parameter in PluginProcessor.
        Strategy strategy = Strategy::Current;

        // Voice type index (0..5) used by the P0 strategy for formant centers.
        int voiceType = 0;

        // Q multiplier applied to the formant resonance. Different strategies
        // use different Q multipliers to be audibly distinct without changing
        // overall gain level. Default 1.0 = original Q.
        float qMultiplier = 1.0f;

        // Nyquist safety frequency.
        static constexpr float maxCutoffHz = 8000.0f;

        // Enables / disables the module.
        bool enabled = false;

        // Recomputes the biquad coefficients for the requested cutoff frequency.
        void updateBiquadCoefficients (ChannelState::BiquadState& s, float freqHz, float q, float gainDb);

        // Allpass biquad coefficients (RBJ cookbook). Magnitude is unity; the
        // phase wraps around `freqHz`, which is what physically shifts the
        // formant without colouring the spectral envelope.
        void updateAllpassCoefficients (ChannelState::BiquadState& s, float freqHz, float q);

        // Updates all formants for a channel according to the ratio
        void updateAllFormants (ChannelState& s, float compensationRatio, float shiftRatio);

        // Processes one channel through all biquads in series
        void processChannel (float* data, int numSamples, ChannelState& s);
    };
}



