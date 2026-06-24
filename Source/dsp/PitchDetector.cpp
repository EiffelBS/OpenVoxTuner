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

        float median = sorted[MEDIAN_SIZE / 2];

        // Anti-octave error par continuite d'octave : on examine les 5
        // valeurs de l'historique pour detecter un saut d'octave. Si la
        // majorite (>= 3) des valeurs valides indique le meme saut, on
        // le corrige. Ce consensus evite les corrections intempestives
        // sur un seul echantillon parasite.
        if (median > 0.0f && median >= 30.0f && median <= 1200.0f)
        {
            int votesUp = 0;   // median est une octave trop haute -> corriger vers le bas
            int votesDown = 0; // median est une octave trop basse -> corriger vers le haut
            int validCount = 0;

            for (int i = 0; i < MEDIAN_SIZE; ++i)
            {
                const float lastPitch = sorted[i];
                if (lastPitch <= 0.0f) continue;
                
                ++validCount;
                const float ratio = median / lastPitch;
                
                // Octave au-dessus (median ~ 2 * lastPitch) : YIN a saute
                // une octave trop haut -> on divise par 2
                if (ratio > 1.7f && ratio < 2.3f)
                    ++votesUp;
                // Octave en-dessous (median ~ 0.5 * lastPitch) : YIN a saute
                // une octave trop bas -> on multiplie par 2
                else if (ratio > 0.45f && ratio < 0.55f)
                    ++votesDown;
            }

            // Applique la correction seulement si consensus clair
            // (> 50% des valeurs valides indiquent la meme direction)
            const int threshold = juce::jmax(3, (validCount / 2) + 1);
            float candidate = median;

            if (votesUp >= threshold && votesDown == 0)
                candidate = median * 0.5f;
            else if (votesDown >= threshold && votesUp == 0)
                candidate = median * 2.0f;

            if (candidate >= 30.0f && candidate <= 1200.0f)
                median = candidate;
        }

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
        // On evalue systematiquement les alternatives a l'octave superieure
        // (tau/2, qui donne 2*f0) et inferieure (2*tau, qui donne f0/2).
        // On choisit la meilleure selon :
        //   (a) la valeur de d' (plus basse = meilleure clarte)
        //   (b) la continuite d'octave avec le dernier pitch valide
        //
        // Note : YIN trouve souvent la 2e harmonique comme premier minimum
        // sous le seuil (voix feminines aigues, ou formant F1 eleve). Dans
        // ce cas, tau/2 est la bonne fondamentale et d'(tau/2) < d'(tau).
        // A l'inverse, sur les voix graves avec sous-harmoniques, 2*tau
        // est la bonne fondamentale.
        bool octaveCorrected = false;
        int bestTau = tauEstimate;
        float bestClarity = yinBuffer[tauEstimate];
        const float bestFreq = sampleRate / (float)tauEstimate;

        // Verifie l'alternative a l'octave superieure : tau/2 -> 2*f0
        // (corrige le cas ou YIN a trouve la 2e harmonique au lieu du fondamental)
        if (tauEstimate % 2 == 0 && tauEstimate / 2 >= minLag)
        {
            int tauHalf = tauEstimate / 2;
            while (tauHalf + 1 < halfSize && yinBuffer[tauHalf + 1] < yinBuffer[tauHalf])
                ++tauHalf;
            const float halfClarity = yinBuffer[tauHalf];
            const float halfFreq = sampleRate / (float)tauHalf;
            
            if (halfClarity < bestClarity)
            {
                bestTau = tauHalf;
                bestClarity = halfClarity;
            }
            else if (lastValidPitch > 0.0f && halfClarity < bestClarity * 1.20f)
            {
                // Clarte similaire (< 20% de difference) : on utilise la
                // continuite d'octave pour trancher. On prefere l'octave
                // la plus proche du dernier pitch valide.
                const float halfDist = std::abs(std::log2(halfFreq / lastValidPitch));
                const float bestDist = std::abs(std::log2(bestFreq / lastValidPitch));
                if (halfDist < bestDist)
                {
                    bestTau = tauHalf;
                    bestClarity = halfClarity;
                }
            }
        }

        // Verifie l'alternative a l'octave inferieure : 2*tau -> f0/2
        // (corrige le cas ou YIN a trouve une sous-harmonique)
        if (tauEstimate * 2 < halfSize)
        {
            int tauDouble = tauEstimate * 2;
            while (tauDouble + 1 < halfSize && yinBuffer[tauDouble + 1] < yinBuffer[tauDouble])
                ++tauDouble;
            const float doubleClarity = yinBuffer[tauDouble];
            const float doubleFreq = sampleRate / (float)tauDouble;
            
            if (doubleClarity < bestClarity)
            {
                bestTau = tauDouble;
                bestClarity = doubleClarity;
            }
            else if (lastValidPitch > 0.0f && doubleClarity < bestClarity * 1.20f)
            {
                const double doubleDist = std::abs(std::log2(doubleFreq / lastValidPitch));
                const double bestDist = std::abs(std::log2(bestFreq / lastValidPitch));
                if (doubleDist < bestDist)
                {
                    bestTau = tauDouble;
                    bestClarity = doubleClarity;
                }
            }
        }

        if (bestTau != tauEstimate)
        {
            tauEstimate = bestTau;
            octaveCorrected = true;
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
