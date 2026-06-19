// PitchDetector.cpp
// Implementation YIN complete.
// Algorithme en 4 etapes :
//   1) Difference function    d(tau) = sum (x[j] - x[j+tau])^2
//   2) Cumulative mean normalized difference d'(tau)
//   3) Recherche du premier minimum sous le seuil
//   4) Interpolation parabolique pour la precision sub-sample

#include "PitchDetector.h"

namespace atdsp
{
    PitchDetector::PitchDetector() = default;
    PitchDetector::~PitchDetector() = default;

    void PitchDetector::prepare (double sr, int blockSize)
    {
        sampleRate = sr;
        // Limite basse 50 Hz (homme basse), limite haute 1000 Hz (enfant/femme aigu).
        freqMinHz = 50.0f;
        freqMaxHz = 1000.0f;

        // Le lag max correspond a la periode la plus longue (frequence min).
        int lagMax = static_cast<int> (sampleRate / freqMinHz);
        int lagMin = static_cast<int> (sampleRate / freqMaxHz);

        // Le buffer d'entree doit permettre le plus long lag recherche.
        bufferSize = juce::jmax (blockSize, lagMax * 2);
        yinBuffer.allocate (bufferSize, true);
        maxLag = lagMax;

        // Stocke lagMin dans une variable "morte" pour eviter un warning
        // (utile si on veut affiner la recherche plus tard).
        juce::ignoreUnused (lagMin);
    }

    void PitchDetector::reset()
    {
        if (yinBuffer.getData() != nullptr)
            std::memset (yinBuffer.getData(), 0, bufferSize * sizeof (float));
            
        for (int i = 0; i < MEDIAN_SIZE; ++i) {
            history[i] = 0.0f;
        }
        historyIdx = 0;
    }

    float PitchDetector::getMedianFiltered(float newValue)
    {
        history[historyIdx] = newValue;
        historyIdx = (historyIdx + 1) % MEDIAN_SIZE;

        float sorted[MEDIAN_SIZE];
        for (int i = 0; i < MEDIAN_SIZE; ++i) sorted[i] = history[i];

        // Bubble sort (size is very small)
        for (int i = 0; i < MEDIAN_SIZE - 1; ++i) {
            for (int j = 0; j < MEDIAN_SIZE - i - 1; ++j) {
                if (sorted[j] > sorted[j + 1]) {
                    float temp = sorted[j];
                    sorted[j] = sorted[j + 1];
                    sorted[j + 1] = temp;
                }
            }
        }

        // Return median
        return sorted[MEDIAN_SIZE / 2];
    }

    float PitchDetector::detectPitch (const float* samples, int numSamples)
    {
        if (samples == nullptr || numSamples < 2)
            return 0.0f;

        // Verifie que le buffer est assez grand pour la plage de frequences.
        if (numSamples < maxLag * 2)
            return 0.0f;

        // Etape 1+2 : difference function cumulee normalisee.
        // d(tau) = sum_{j=0..W-1} (x[j] - x[j+tau])^2
        // d'(tau) = d(tau) / ((1/tau) * sum_{j=1..tau} d(j))  si tau > 0
        // d'(0) = 1
        yinBuffer[0] = 1.0f;
        float cumSum = 0.0f;

        const int halfSize = numSamples / 2;
        const int maxTau = juce::jmin(halfSize, maxLag);
        for (int tau = 1; tau <= maxTau; ++tau)
        {
            float sum = 0.0f;
            const float* s1 = samples;
            const float* s2 = samples + tau;

            // Auto-vectorizable loop
            for (int j = 0; j < halfSize; ++j)
            {
                const float delta = s1[j] - s2[j];
                sum += delta * delta;
            }
            cumSum += sum;
            yinBuffer[tau] = (cumSum > 0.0f) ? (sum * tau / cumSum) : 1.0f;
        }

        // Fill the rest with 1.0f just in case
        for (int tau = maxTau + 1; tau < halfSize; ++tau)
            yinBuffer[tau] = 1.0f;

        // Etape 3 : recherche du premier minimum sous le seuil.
        // On cherche le premier tau ou d'(tau) < threshold, puis on descend au minimum local.
        int tauEstimate = -1;
        const int minLag = juce::jmax (2, static_cast<int> (sampleRate / freqMaxHz));
        for (int tau = minLag; tau < halfSize; ++tau)
        {
            if (yinBuffer[tau] < threshold)
            {
                // Descend jusqu'au minimum local.
                while (tau + 1 < halfSize && yinBuffer[tau + 1] < yinBuffer[tau])
                    ++tau;
                tauEstimate = tau;
                break;
            }
        }

        if (tauEstimate == -1) {
            getMedianFiltered(0.0f); // Feed history
            return 0.0f; // Pas de pitch detecte.
        }

        // Etape 3b (ANTI-OCTAVE-ERROR) : si 2*tau est aussi sous le seuil,
        // c'est que tau est probablement une sous-harmonique (le vrai
        // fondamental est a 2*tau). C'est le cas classique "YIN detecte
        // 2*f0 au lieu de f0" sur les signaux avec une forte 2e harmonique.
        // Reference : de Cheveigne & Kawahara, "YIN, a fundamental frequency
        // estimator for speech and music", J. Acoust. Soc. Am. 2002.
        {
            const int tau2 = tauEstimate * 2;
            if (tau2 > tauEstimate && tau2 < halfSize
                && yinBuffer[tau2] < threshold)
            {
                // tauEstimate est une sous-harmonique : on prend 2*tau a la place.
                tauEstimate = tau2;
                // Redescend au minimum local autour de 2*tau.
                while (tauEstimate + 1 < halfSize
                       && yinBuffer[tauEstimate + 1] < yinBuffer[tauEstimate])
                    ++tauEstimate;
            }
        }

        // Etape 4 : interpolation parabolique pour la precision sub-sample.
        // On ajuste tau autour du minimum grace a 3 points.
        float betterTau = static_cast<float> (tauEstimate);
        if (tauEstimate > 0 && tauEstimate < halfSize - 1)
        {
            const float s0 = yinBuffer[tauEstimate - 1];
            const float s1 = yinBuffer[tauEstimate];
            const float s2 = yinBuffer[tauEstimate + 1];
            const float denom = 2.0f * (2.0f * s1 - s0 - s2);
            if (std::abs (denom) > 1e-12f)
                betterTau += (s2 - s0) / denom;
        }

        if (betterTau <= 0.0f) {
            getMedianFiltered(0.0f); // Feed 0 to history
            return 0.0f;
        }

        float f0 = static_cast<float> (sampleRate / betterTau);
        return getMedianFiltered(f0);
    }

    float PitchDetector::computeYin (const float* /*samples*/, int /*numSamples*/)
    {
        // Inutilise : la logique complete est in-place dans detectPitch().
        return 0.0f;
    }
}
