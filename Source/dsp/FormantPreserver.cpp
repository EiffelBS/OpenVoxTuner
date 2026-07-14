// FormantPreserver.cpp
// Implementation du deplaceur de formants par biquad passe-bas.

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

        // 2 canaux par defaut.
        for (int i = 0; i < 2; ++i)
        {
            ChannelState s;
            updateCoefficients (s, maxCutoffHz); // passthrough au demarrage
            channels.add (s);
        }
    }

    void FormantPreserver::reset()
    {
        for (auto& s : channels)
        {
            s.z1 = 0.0f;
            s.z2 = 0.0f;
        }
    }

    void FormantPreserver::updateCoefficients (ChannelState& s, float cutoffHz)
    {
        // Filtre Peaking (EQ) pour simuler un Formant (Resonance)
        const float w0 = 2.0f * juce::MathConstants<float>::pi * cutoffHz / static_cast<float> (sampleRate);
        const float cosw0 = std::cos (w0);
        const float sinw0 = std::sin (w0);
        
        // Q = 2.0 (assez resonnant pour entendre le formant)
        const float alpha = sinw0 / (2.0f * 2.0f);
        
        // Gain = +8dB pour le formant
        const float A = std::pow(10.0f, 8.0f / 40.0f);

        // Coefficients normalises pour un Peaking EQ.
        const float a0 = 1.0f + alpha / A;
        const float b0n = (1.0f + alpha * A) / a0;
        const float b1n = (-2.0f * cosw0) / a0;
        const float b2n = (1.0f - alpha * A) / a0;
        const float a1n = (-2.0f * cosw0) / a0;
        const float a2n = (1.0f - alpha / A) / a0;

        s.b0 = b0n;
        s.b1 = b1n;
        s.b2 = b2n;
        s.a1 = a1n;
        s.a2 = a2n;
    }

    void FormantPreserver::processChannel (float* data, int numSamples, ChannelState& s)
    {
        // Forme directe transposed II (numeriquement stable).
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = data[i];
            const float y = s.b0 * x + s.z1;
            s.z1 = s.b1 * x - s.a1 * y + s.z2;
            s.z2 = s.b2 * x - s.a2 * y;
            data[i] = y;
        }
    }

    void FormantPreserver::process (juce::AudioBuffer<float>& buffer, float ratio)
    {
        // On desactive si le module est off et qu'il n'y a ni pitch shifting ni formant shift.
        if (!enabled || (std::abs (ratio - 1.0f) < 1e-6f && std::abs(shiftSemitones) < 1e-6f))
            return;

        // Calcul de la freq de coupure : on deplace les formants en sens
        // inverse du ratio (compensation). Si ratio > 1 (on va monter le
        // pitch via PSOLA), on baisse ici les formants de (1/ratio).
        // Note : c'est une approximation tres grossiere, mais elle preserve
        // deja bien mieux le timbre qu'un PSOLA pur.
        const float r = juce::jlimit (0.25f, 4.0f, ratio);
        const float compensationRatio = 1.0f / std::sqrt (r); // sqrt = compromis
        
        // Ajout du decalage de formants manuel
        const float shiftRatio = std::pow (2.0f, shiftSemitones / 12.0f);
        
        const float cutoff = juce::jlimit (100.0f, maxCutoffHz,
                                           referenceFormantHz * compensationRatio * shiftRatio);

        const int numChannels = juce::jmin (2, buffer.getNumChannels());
        for (int ch = 0; ch < numChannels; ++ch)
        {
            updateCoefficients (channels.getReference (ch), cutoff);
            processChannel (buffer.getWritePointer (ch),
                            buffer.getNumSamples(),
                            channels.getReference (ch));
        }
    }
}
