// YinPitchDetector.cpp
// YIN pitch detection algorithm implementation.
// Ported from PitchDetector.cpp with the same logic; only the
// class name and inheritance changed.

#include "YinPitchDetector.h"

namespace ovtdsp
{

YinPitchDetector::YinPitchDetector() = default;
YinPitchDetector::~YinPitchDetector() = default;

void YinPitchDetector::prepare (double sr, int blockSize)
{
    sampleRate = sr;
    freqMinHz = 30.0f;
    freqMaxHz = 1000.0f;

    int lagMax = static_cast<int> (sampleRate / freqMinHz);
    int lagMin = static_cast<int> (sampleRate / freqMaxHz);

    bufferSize = juce::jmax (blockSize, lagMax * 2);
    yinBuffer.allocate (bufferSize, true);
    maxLag = lagMax;

    juce::ignoreUnused (lagMin);
}

void YinPitchDetector::reset()
{
    if (yinBuffer.getData() != nullptr)
        std::memset (yinBuffer.getData(), 0, bufferSize * sizeof (float));

    for (int i = 0; i < MEDIAN_SIZE; ++i)
        history[i] = 0.0f;
    historyIdx = 0;
    lastValidPitch = 0.0f;
}

void YinPitchDetector::setFrequencyRange (float minHz, float maxHz)
{
    // Clamp to sane bounds. Anything below ~20Hz is sub-bass, anything above
    // ~2000Hz is a harmonic of the fundamental (or pure noise/sibilance).
    minHz = juce::jlimit (20.0f, 2000.0f, minHz);
    maxHz = juce::jmax (minHz + 10.0f, juce::jlimit (40.0f, 4000.0f, maxHz));

    freqMinHz = minHz;
    freqMaxHz = maxHz;

    if (sampleRate <= 0.0)
        return; // prepare() will apply the range when called

    const int newMaxLag = static_cast<int> (sampleRate / freqMinHz);
    maxLag = newMaxLag;

    // Grow the working buffer if the new range needs more samples. The
    // buffer must be at least 2*maxLag to evaluate d(tau) at the largest lag.
    const int neededSize = juce::jmax (bufferSize, newMaxLag * 2);
    if (neededSize > bufferSize)
    {
        bufferSize = neededSize;
        yinBuffer.allocate (bufferSize, true);
    }
}

float YinPitchDetector::getMedianFiltered (float newValue)
{
    history[historyIdx] = newValue;
    historyIdx = (historyIdx + 1) % MEDIAN_SIZE;

    float sorted[MEDIAN_SIZE];
    for (int i = 0; i < MEDIAN_SIZE; ++i)
        sorted[i] = history[i];

    for (int i = 0; i < MEDIAN_SIZE - 1; ++i)
        for (int j = 0; j < MEDIAN_SIZE - i - 1; ++j)
            if (sorted[j] > sorted[j + 1])
                std::swap (sorted[j], sorted[j + 1]);

    return sorted[MEDIAN_SIZE / 2];
}

float YinPitchDetector::detectPitch (const float* samples, int numSamples)
{
    if (samples == nullptr || numSamples < 2)
        return 0.0f;

    // Step 1+2: cumulative mean normalized difference.
    yinBuffer[0] = 1.0f;
    float cumSum = 0.0f;

    const int halfSize = numSamples / 2;
    const int maxTau = juce::jmin (maxLag, numSamples - 1);

    // Portee de recherche limitee par la taille du buffer disponible : on ne
    // peut pas evaluer une periode plus longue que numSamples/2. On recherche
    // donc jusqu'a searchMax = min(maxLag, numSamples/2) au lieu d'exiger
    // 2*maxLag echantillons. Cela rend YIN fonctionnel avec des buffers
    // realistes (ex. la fenetre decimee de 1024 echantillons utilisee en
    // temps reel) et couvre la plage [freqMax, sampleRate/searchMax]
    // physiquement disponible dans le buffer.
    const int minLag = juce::jmax (2, static_cast<int> (sampleRate / freqMaxHz));
    const int searchMax = juce::jmin (maxLag, halfSize);
    if (searchMax < minLag)
        return 0.0f;

    for (int tau = 1; tau <= maxTau; ++tau)
    {
        int len = std::min (numSamples - tau, halfSize);
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

    for (int tau = maxTau + 1; tau < searchMax; ++tau)
        yinBuffer[tau] = 1.0f;

    // Step 3: find first minimum below threshold.
    int tauEstimate = -1;
    for (int tau = minLag; tau < searchMax; ++tau)
    {
        if (yinBuffer[tau] < threshold)
        {
            while (tau + 1 < searchMax && yinBuffer[tau + 1] < yinBuffer[tau])
                ++tau;
            tauEstimate = tau;
            break;
        }
    }

    if (tauEstimate == -1)
    {
        getMedianFiltered (0.0f);
        return 0.0f;
    }

    // Step 3b: anti-octave-error correction.
    bool octaveCorrected = false;
    int bestTau = tauEstimate;
    float bestClarity = yinBuffer[tauEstimate];
    // Soft threshold for considering the fundamental even if the 2nd
    // harmonic has better clarity. The fundamental is almost always
    // detectable at d' < 0.30 for voiced speech.
    constexpr float fundamentalSoftThreshold = 0.30f;

    // Case A: YIN found 2nd harmonic -> test tau*2 (one octave down = fundamental).
    // Strategy: always prefer the fundamental (tau*2) as long as its clarity
    // is below fundamentalSoftThreshold. This is safer than continuity-based
    // selection which blocks genuine octave transitions.
    if (tauEstimate * 2 < searchMax)
    {
        int tauDouble = tauEstimate * 2;
        while (tauDouble + 1 < searchMax && yinBuffer[tauDouble + 1] < yinBuffer[tauDouble])
            ++tauDouble;
        const float doubleClarity = yinBuffer[tauDouble];

        // Prefer the fundamental if it has acceptable clarity, regardless
        // of whether the 2nd harmonic has better clarity.
        // The 2nd harmonic often has lower d' because of formant emphasis,
        // but the fundamental is almost always what we want.
        if (doubleClarity < fundamentalSoftThreshold)
        {
            bestTau = tauDouble;
            bestClarity = doubleClarity;
            octaveCorrected = true;
        }
    }

    // Case B: YIN found a sub-harmonic -> test tau/2 (one octave up).
    // Only adopt if it has strictly better clarity (conservative).
    if (!octaveCorrected && tauEstimate % 2 == 0 && tauEstimate / 2 >= minLag)
    {
        int tauHalf = tauEstimate / 2;
        while (tauHalf + 1 < searchMax && yinBuffer[tauHalf + 1] < yinBuffer[tauHalf])
            ++tauHalf;
        if (yinBuffer[tauHalf] < bestClarity)
        {
            bestTau = tauHalf;
            octaveCorrected = true;
        }
    }

    if (octaveCorrected)
        tauEstimate = bestTau;

    // Step 4: parabolic interpolation for sub-sample precision.
    float betterTau = (float)tauEstimate;
    if (tauEstimate > 0 && tauEstimate < searchMax - 1)
    {
        const float s0 = yinBuffer[tauEstimate - 1];
        const float s1 = yinBuffer[tauEstimate];
        const float s2 = yinBuffer[tauEstimate + 1];
        const float denom = 2.0f * (2.0f * s1 - s0 - s2);
        if (std::abs (denom) > 1e-12f)
            betterTau += (s2 - s0) / denom;
    }

    if (betterTau <= 0.0f)
    {
        getMedianFiltered (0.0f);
        return 0.0f;
    }

    float f0 = (float)(sampleRate / betterTau);
    float result = getMedianFiltered (f0);
    if (result > 0.0f)
        lastValidPitch = result;
    return result;
}

float YinPitchDetector::computeYin (const float*, int)
{
    // Logic is inlined in detectPitch().
    return 0.0f;
}

} // namespace ovtdsp