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
        // Limite basse 30 Hz pour couvrir les notes très graves comme F#1 (~46 Hz). 
        freqMinHz = 30.0f;
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
        lastValidPitch = 0.0f;
    }

    float PitchDetector::getMedianFiltered(float newValue)
    {
        history[historyIdx] = newValue;
        historyIdx = (historyIdx + 1) % MEDIAN_SIZE;

        float sorted[MEDIAN_SIZE];
        for (int i = 0; i < MEDIAN_SIZE; ++i) sorted[i] = history[i];

        for (int i = 0; i < MEDIAN_SIZE - 1; ++i) {
            for (int j = 0; j < MEDIAN_SIZE - i - 1; ++j) {
                if (sorted[j] > sorted[j + 1]) {
                    float temp = sorted[j];
                    sorted[j] = sorted[j + 1];
                    sorted[j + 1] = temp;
                }
            }
        }

        float median = sorted[MEDIAN_SIZE / 2];
        // Note : la correction d'octave est desormais geree dans detectPitch()
        // etape 3b. getMedianFiltered se contente de filtrer les outliers
        // passagers par mediane, sans correction d'octave supplementaire.
        return median;
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
        // Allow detection up to the largest possible lag that fits in the buffer
        const int maxTau = juce::jmin(maxLag, numSamples - 1);
        for (int tau = 1; tau <= maxTau; ++tau)
        {
            int len = std::min(numSamples - tau, halfSize); // effective comparison length
            float sum = 0.0f;
            const float* s1 = samples;
            const float* s2 = samples + tau;

            for (int j = 0; j < len; ++j)
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

        // Etape 3b (ANTI-OCTAVE-ERROR) : correction amelioree.
        // Principe : on evalue systematiquement les 2 alternatives d'octave.
        //
        // Cas A (le PLUS FREQUENT pour la voix) : YIN trouve la 2e harmonique.
        //   Ex: l'utilisateur chante F2 (87 Hz, tau=126) mais YIN trouve F3
        //   (174 Hz, tau=63) car la 2e harmonique a plus d'energie que le
        //   fondamental ("missing fundamental"). La periode detectee est la
        //   MOITIE de la vraie periode, donc il faut tester **tau * 2**
        //   (= periode double = frequence moitie = un octave en dessous).
        //
        // Cas B (moins frequent) : YIN trouve une sous-harmonique.
        //   La periode detectee est le DOUBLE de la vraie periode.
        //   Il faut tester **tau / 2** (= periode moitie = frequence double).
        //
        // Strategie de correction : on ne demande PAS que l'alternative ait
        // une meilleure clarte que la detection initiale. On demande juste
        // qu'elle soit "acceptable" (valeur de d' sous un seuil elargi),
        // puis on tranche par continuite d'octave.
        bool octaveCorrected = false;
        int bestTau = tauEstimate;
        float bestClarity = yinBuffer[tauEstimate];

        // Seuil elargi pour considerer une alternative comme "acceptable".
        // Une valeur d' < 0.25 est tres probablement un vrai pitch meme si
        // ce n'est pas le meilleur minimum local.
        constexpr float softThreshold = 0.25f;

        // --- Cas A : YIN a trouve la 2e harmonique -> on teste tau * 2 ---
        // (periode double = frequence moitie = un octave en dessous)
        if (tauEstimate * 2 < halfSize)
        {
            int tauDouble = tauEstimate * 2;
            while (tauDouble + 1 < halfSize && yinBuffer[tauDouble + 1] < yinBuffer[tauDouble])
                ++tauDouble;
            const float doubleClarity = yinBuffer[tauDouble];

            if (doubleClarity < bestClarity)
            {
                // L'alternative a strictement meilleure clarte -> on adopte
                bestTau = tauDouble;
                bestClarity = doubleClarity;
                octaveCorrected = true;
            }
            else if (doubleClarity < softThreshold)
            {
                // Alternative acceptable : on tranche par continuite d'octave
                if (lastValidPitch > 0.0f)
                {
                    const float doubleFreq = sampleRate / (float)tauDouble;
                    const float bestFreq = sampleRate / (float)tauEstimate;
                    const float doubleDist = std::abs(std::log2(doubleFreq / lastValidPitch));
                    const float bestDist = std::abs(std::log2(bestFreq / lastValidPitch));
                    if (doubleDist < bestDist)
                    {
                        bestTau = tauDouble;
                        bestClarity = doubleClarity;
                        octaveCorrected = true;
                    }
                }
                else
                {
                    // Pas de contexte de continuite : on prefere la frequence
                    // la plus basse (tau le plus grand) -> la fondamentale
                    bestTau = tauDouble;
                    bestClarity = doubleClarity;
                    octaveCorrected = true;
                }
            }
        }

        // --- Cas B : YIN a trouve une sous-harmonique -> on teste tau / 2 ---
        // (periode moitie = frequence double = un octave au-dessus)
        // ATTENTION : on n'utilise JAMAIS la continuite d'octave ici, car
        // elle empecherait les changements de registre legitimes.
        // Exemple : utilisateur chante C3 (lastValidPitch=131Hz), puis C2
        // (65Hz). YIN detecte 65Hz. tau/2 = 84 donne 131Hz (C3). La
        // continuite dirait "131Hz est plus proche de lastValidPitch que
        // 65Hz" et corromprait la detection. On ne corrige donc que si
        // l'alternative a strictement meilleure clarte.
        if (!octaveCorrected && tauEstimate % 2 == 0 && tauEstimate / 2 >= minLag)
        {
            int tauHalf = tauEstimate / 2;
            while (tauHalf + 1 < halfSize && yinBuffer[tauHalf + 1] < yinBuffer[tauHalf])
                ++tauHalf;
            const float halfClarity = yinBuffer[tauHalf];

            if (halfClarity < bestClarity)
            {
                // Alternative strictement meilleure -> on adopte
                bestTau = tauHalf;
                bestClarity = halfClarity;
                octaveCorrected = true;
            }
            // Sinon on garde la detection originale (conservateur)
        }

        if (octaveCorrected)
            tauEstimate = bestTau;

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
        float result = getMedianFiltered(f0);
        if (result > 0.0f)
            lastValidPitch = result;
        return result;
    }

    float PitchDetector::computeYin (const float* /*samples*/, int /*numSamples*/)
    {
        // Inutilise : la logique complete est in-place dans detectPitch().
        return 0.0f;
    }
}
