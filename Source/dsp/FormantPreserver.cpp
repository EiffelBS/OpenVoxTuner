// FormantPreserver.cpp
// Implementation du deplaceur de formants multi-formants.

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
                updateBiquadCoefficients (s.formants[f], maxCutoffHz, 0.707f, 0.0f);
            channels.add (s);
        }
    }

    void FormantPreserver::reset()
    {
        for (auto& s : channels)
        {
            for (int f = 0; f < 4; ++f)
            {
                s.formants[f].z1 = 0.0f;
                s.formants[f].z2 = 0.0f;
                // Reinitialise aussi les coefficients lisses a l'etat
                // transparent (passthrough) pour eviter un saut au reset.
                s.smooth[f].b0 = 1.0f; s.smooth[f].b1 = 0.0f; s.smooth[f].b2 = 0.0f;
                s.smooth[f].a1 = 0.0f; s.smooth[f].a2 = 0.0f;
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

    void FormantPreserver::updateAllFormants (ChannelState& s, float compensationRatio, float shiftRatio)
    {
        for (int f = 0; f < 4; ++f)
        {
            // Compensation: formants move opposite to pitch shift
            // If pitch goes up (ratio > 1), formants need to go down before PSOLA
            // The compensationRatio = 1/sqrt(ratio) moves them partially
            // shiftRatio applies user formant shift on top
            float targetFreq = formantConfigs[f].freqHz * compensationRatio * shiftRatio;
            targetFreq = juce::jlimit (20.0f, maxCutoffHz, targetFreq);
            updateBiquadCoefficients (s.formants[f], targetFreq, formantConfigs[f].q, formantConfigs[f].gainDb);
        }
    }

    void FormantPreserver::processChannel (float* data, int numSamples, ChannelState& s)
    {
        // Lisse les coefficients cibles vers les coefficients appliques (un
        // seul pas par bloc, mais applique a chaque echantillon du bloc pour
        // rester independant de la taille de buffer). Cela empeche toute
        // discontinuite a la sortie quand le ratio de pitch change
        // brutalement (demarrage d'une note chantee).
        for (int f = 0; f < 4; ++f)
        {
            s.smooth[f].b0 += biquadSmoothAlpha * (s.formants[f].b0 - s.smooth[f].b0);
            s.smooth[f].b1 += biquadSmoothAlpha * (s.formants[f].b1 - s.smooth[f].b1);
            s.smooth[f].b2 += biquadSmoothAlpha * (s.formants[f].b2 - s.smooth[f].b2);
            s.smooth[f].a1 += biquadSmoothAlpha * (s.formants[f].a1 - s.smooth[f].a1);
            s.smooth[f].a2 += biquadSmoothAlpha * (s.formants[f].a2 - s.smooth[f].a2);
        }

        // Process through all 4 formants in series, using the SMOOTHED
        // coefficients (not the raw per-block targets) to avoid clicks.
        for (int i = 0; i < numSamples; ++i)
        {
            float x = data[i];
            
            // F1
            float y = s.smooth[0].b0 * x + s.formants[0].z1;
            s.formants[0].z1 = s.smooth[0].b1 * x - s.smooth[0].a1 * y + s.formants[0].z2;
            s.formants[0].z2 = s.smooth[0].b2 * x - s.smooth[0].a2 * y;
            x = y;

            // F2
            y = s.smooth[1].b0 * x + s.formants[1].z1;
            s.formants[1].z1 = s.smooth[1].b1 * x - s.smooth[1].a1 * y + s.formants[1].z2;
            s.formants[1].z2 = s.smooth[1].b2 * x - s.smooth[1].a2 * y;
            x = y;

            // F3
            y = s.smooth[2].b0 * x + s.formants[2].z1;
            s.formants[2].z1 = s.smooth[2].b1 * x - s.smooth[2].a1 * y + s.formants[2].z2;
            s.formants[2].z2 = s.smooth[2].b2 * x - s.smooth[2].a2 * y;
            x = y;

            // F4
            y = s.smooth[3].b0 * x + s.formants[3].z1;
            s.formants[3].z1 = s.smooth[3].b1 * x - s.smooth[3].a1 * y + s.formants[3].z2;
            s.formants[3].z2 = s.smooth[3].b2 * x - s.smooth[3].a2 * y;
            
            data[i] = y;
        }
    }

    void FormantPreserver::process (juce::AudioBuffer<float>& buffer, float ratio)
    {
        // On desactive si le module est off et qu'il n'y a ni pitch shifting ni formant shift.
        if (!enabled || (std::abs (ratio - 1.0f) < 1e-6f && std::abs(shiftSemitones) < 1e-6f))
            return;

        // Calcul de la compensation : on deplace les formants en sens
        // inverse du ratio (compensation). Si ratio > 1 (on va monter le
        // pitch via PSOLA), on baisse ici les formants de (1/ratio).
        // Note : c'est une approximation, mais elle preserve
        // deja bien mieux le timbre qu'un PSOLA pur.
        const float r = juce::jlimit (0.25f, 4.0f, ratio);
        
        // Compensation ratio: inverse sqrt gives partial compensation
        // Full compensation would be 1/r, but that overcorrects
        // sqrt is a good compromise (Moulines & Charpentier)
        const float compensationRatio = 1.0f / std::sqrt (r);
        
        // Ajout du decalage de formants manuel
        const float shiftRatio = std::pow (2.0f, shiftSemitones / 12.0f);

        const int numChannels = juce::jmin (2, buffer.getNumChannels());
        
        // Ensure we have enough channel states
        while (channels.size() < numChannels)
        {
            ChannelState s;
            for (int f = 0; f < 4; ++f)
                updateBiquadCoefficients (s.formants[f], maxCutoffHz, 0.707f, 0.0f);
            channels.add (s);
        }

        if (mode == Mode::Legacy)
        {
            // Legacy mode: single peaking EQ at reference frequency (backward compatible)
            const float cutoff = juce::jlimit (100.0f, maxCutoffHz,
                                               500.0f * compensationRatio * shiftRatio);
            for (int ch = 0; ch < numChannels; ++ch)
            {
                // Use first formant slot for legacy
                updateBiquadCoefficients (channels.getReference(ch).formants[0], cutoff, 2.0f, 8.0f);
                processChannel (buffer.getWritePointer (ch), buffer.getNumSamples(),
                                channels.getReference(ch));
            }
        }
        else
        {
            // MultiFormant mode: F1-F4
            for (int ch = 0; ch < numChannels; ++ch)
            {
                updateAllFormants (channels.getReference(ch), compensationRatio, shiftRatio);
                processChannel (buffer.getWritePointer (ch), buffer.getNumSamples(),
                                channels.getReference(ch));
            }
        }
    }
}