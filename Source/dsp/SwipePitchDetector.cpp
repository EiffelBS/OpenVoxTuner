// SwipePitchDetector.cpp
// SWIPE'-inspired spectral pitch detection implementation.
//
// Reference: Camacho & Harris, "A Sawtooth Waveform Inspired Pitch
//   Estimator for Speech and Music", J. Acoust. Soc. Am. 124(3), 2008.
//
// Algorithm overview:
//   1) Accumulate samples in a ring buffer until FFT size reached
//   2) Apply Hann window, compute FFT, extract magnitude spectrum
//   3) For each candidate pitch, compute correlation between the
//      magnitude spectrum and a sawtooth kernel at that pitch
//   4) Select candidate with highest correlation
//   5) Parabolic interpolation for sub-bin precision
//   6) Stability smoothing (rejects spurious outliers)

#include "SwipePitchDetector.h"
#include <cmath>
#include <algorithm>

namespace atdsp
{

SwipePitchDetector::SwipePitchDetector() = default;
SwipePitchDetector::~SwipePitchDetector() = default;

void SwipePitchDetector::prepare (double sr, int blockSize)
{
    sampleRate = sr;

    // Choose FFT size: at least 2x block size for overlap, power of 2.
    // At 44.1 kHz: 1024 -> ~23ms latency, good for voice.
    fftSize = 1024;
    while (fftSize < blockSize * 2)
        fftSize *= 2;

    fftOrder = static_cast<int> (std::log2 ((double)fftSize));
    fft = std::make_unique<juce::dsp::FFT> (fftOrder);

    // Allocate FFT buffers (HeapBlock guarantees 16-byte SIMD alignment).
    // performRealOnlyForwardTransform requires 2 * fftSize floats (packed format + scratch).
    fftWindow.allocate (fftSize, true);
    fftRealBuffer.allocate (2 * fftSize, true);
    spectrumMag.allocate (fftSize / 2 + 1, true);

    // Build Hann window.
    for (int i = 0; i < fftSize; ++i)
        fftWindow[i] = 0.5f * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1)));

    // Ring buffer for frame accumulation.
    ringSize = fftSize + blockSize;
    ringBuffer.allocate (ringSize, true);
    ringWritePos = 0;

    // Build candidate pitch list: logarithmic spacing over the voice range.
    // ~48 candidates per octave (fine enough for < 1 semitone).
    const float freqMin = 30.0f;
    const float freqMax = 1000.0f;
    numCandidates = 0;
    for (float f = freqMin; f <= freqMax && numCandidates < 300; f *= std::pow (2.0f, 1.0f / 48.0f))
    {
        candidateFreqs[numCandidates] = f;
        ++numCandidates;
    }

    // Pre-build kernels in the frequency domain.
    // Each kernel is the ideal sawtooth response at that pitch.
    const int halfSize = fftSize / 2 + 1;
    kernelCache.allocate (numCandidates * halfSize, true);
    for (int c = 0; c < numCandidates; ++c)
    {
        kernelCachePtrs[c] = kernelCache.getData() + c * halfSize;
        buildKernel (candidateFreqs[c], kernelCachePtrs[c], fftSize);
    }

    lastPitch = 0.0f;
    stableCount = 0;
}

void SwipePitchDetector::reset()
{
    if (ringBuffer.getData() != nullptr)
        std::memset (ringBuffer.getData(), 0, (size_t)ringSize * sizeof (float));
    ringWritePos = 0;
    lastPitch = 0.0f;
    stableCount = 0;
}

void SwipePitchDetector::buildKernel (float freq, float* kernel, int fftSz)
{
    if (freq <= 0.0f || fftSz <= 0)
        return;

    const int halfSize = fftSz / 2 + 1;
    const float fundamentalBin = (float)(freq * fftSz / sampleRate);

    // Build a sawtooth-wave kernel in the frequency domain.
    // A sawtooth has harmonics at k * f0 with amplitude 1/k.
    // We compute the correlation mask up to Nyquist (halfSize bins).
    for (int bin = 0; bin < halfSize; ++bin)
    {
        float binFreq = (float)(bin * sampleRate / fftSz);

        // Find the nearest harmonic of the candidate pitch.
        float harmIdx = binFreq / freq;
        if (harmIdx < 0.5f)
        {
            // Below the fundamental: no energy expected.
            kernel[bin] = 0.0f;
            continue;
        }

        int nearest = std::max (1, (int)std::round (harmIdx));
        float harmFreq = freq * nearest;
        float binDistance = std::abs (binFreq - harmFreq) * fftSz / sampleRate;

        // Gaussian-like weight around each harmonic.
        // Width scales with harmonic number (wider at higher harmonics).
        float spread = 0.5f + 0.25f * (float)nearest;
        float weight = std::exp (-binDistance * binDistance / (2.0f * spread * spread));

        // Amplitude = 1 / harmonic number (sawtooth envelope).
        float amplitude = 1.0f / (float)nearest;

        kernel[bin] = weight * amplitude;
    }

    // Normalize kernel to unit energy.
    float energy = 0.0f;
    for (int bin = 0; bin < halfSize; ++bin)
        energy += kernel[bin] * kernel[bin];
    if (energy > 1e-10f)
    {
        float invNorm = 1.0f / std::sqrt (energy);
        for (int bin = 0; bin < halfSize; ++bin)
            kernel[bin] *= invNorm;
    }
}

float SwipePitchDetector::computeCorrelation (const float* spectrum,
                                               const float* kernel,
                                               int halfSize)
{
    // Pearson correlation between spectrum magnitude and kernel.
    float sumS = 0.0f, sumK = 0.0f;
    float sumSS = 0.0f, sumKK = 0.0f, sumSK = 0.0f;
    int count = 0;

    for (int bin = 1; bin < halfSize; ++bin)
    {
        float s = spectrum[bin];
        float k = kernel[bin];

        // Only evaluate bins where kernel has meaningful energy.
        if (k < 0.01f) continue;

        sumS  += s;     sumK  += k;
        sumSS += s * s; sumKK += k * k;
        sumSK += s * k;
        ++count;
    }

    if (count < 3)
        return 0.0f;

    float n = (float)count;
    float varS = sumSS - sumS * sumS / n;
    float varK = sumKK - sumK * sumK / n;
    float cov  = sumSK - sumS * sumK / n;

    float denom = std::sqrt (varS * varK);
    if (denom < 1e-10f) return 0.0f;

    return cov / denom;
}

float SwipePitchDetector::interpolatePeak (float y1, float y2, float y3)
{
    float denom = 2.0f * (2.0f * y2 - y1 - y3);
    if (std::abs (denom) < 1e-12f) return 0.0f;
    return (y3 - y1) / denom;
}

float SwipePitchDetector::detectPitch (const float* samples, int numSamples)
{
    if (samples == nullptr || numSamples < 1)
        return lastPitch;

    // Accumulate into ring buffer.
    for (int i = 0; i < numSamples; ++i)
    {
        ringBuffer[ringWritePos] = samples[i];
        ringWritePos = (ringWritePos + 1) % ringSize;
    }

    // Need at least fftSize samples accumulated.
    // If the ring hasn't wrapped yet, check fill count.
    // Simple approach: try to read fftSize consecutive samples
    // ending at ringWritePos-1 (circular).
    int readPos = (ringWritePos - fftSize + ringSize) % ringSize;

    // Read FFT frame (windowed) into real-only buffer.
    for (int i = 0; i < fftSize; ++i)
    {
        int pos = (readPos + i) % ringSize;
        fftRealBuffer[i] = ringBuffer[pos] * fftWindow[i];
    }

    // Compute real-only forward FFT (Juce 8 API).
    fft->performRealOnlyForwardTransform (fftRealBuffer.getData());

    // Extract magnitude spectrum from packed output.
    // Packed format: data[0]=DC, data[1]=Nyquist, data[2k]=re[k], data[2k+1]=im[k].
    // data[2k+1] is only valid for 1 <= k < fftSize/2.
    // DO NOT read beyond fftSize-1 (data[4096] when fftSize=4096)!
    const int halfSize = fftSize / 2 + 1;
    spectrumMag[0] = std::abs (fftRealBuffer[0]); // DC
    for (int bin = 1; bin < fftSize / 2; ++bin)
    {
        float re = fftRealBuffer[bin * 2];
        float im = fftRealBuffer[bin * 2 + 1];
        spectrumMag[bin] = std::sqrt (re * re + im * im);
    }
    spectrumMag[fftSize / 2] = std::abs (fftRealBuffer[1]); // Nyquist

    // Compute signal energy once (independent of candidate).
    float signalEnergy = 0.0f;
    for (int bin = 1; bin < halfSize; ++bin)
        signalEnergy += spectrumMag[bin] * spectrumMag[bin];
    const float energyFactor = juce::jmin (1.0f, signalEnergy / (float)fftSize);

    // Search over candidate pitches for best correlation.
    int bestIdx = -1;
    float bestCorr = -1.0f;
    for (int c = 0; c < numCandidates; ++c)
    {
        float corr = computeCorrelation (spectrumMag.getData(),
                                         kernelCachePtrs[c],
                                         halfSize);
        // Weight by signal energy coherence: penalize very low energy.
        corr *= energyFactor;

        // Bias toward lower frequencies: fundamental is preferred over harmonics.
        // Weight = 1/sqrt(freq/100). At 100Hz -> 1.0, at 400Hz (4th harmonic) -> 0.5.
        const float freqBias = 1.0f / std::sqrt (candidateFreqs[c] / 100.0f);
        corr *= freqBias;

        if (corr > bestCorr && corr > threshold * 0.5f)
        {
            bestCorr = corr;
            bestIdx = c;
        }
    }

    // No pitch detected (best correlation below threshold).
    if (bestIdx < 0 || bestCorr < threshold)
    {
        stableCount = 0;
        lastPitch = 0.0f;
        return 0.0f;
    }

    float pitch = candidateFreqs[bestIdx];

    // Refine with parabolic interpolation if neighbours exist.
    if (bestIdx > 0 && bestIdx < numCandidates - 1)
    {
        const int idxL = bestIdx - 1;
        const int idxR = bestIdx + 1;

        // Compute correlation for neighbours (reuse cached kernel).
        float rL = computeCorrelation (spectrumMag.getData(),
                                       kernelCachePtrs[idxL], halfSize);
        float rM = bestCorr;
        float rR = computeCorrelation (spectrumMag.getData(),
                                       kernelCachePtrs[idxR], halfSize);

        // Apply the same energy weighting.
        rL *= energyFactor;
        rM *= energyFactor;
        rR *= energyFactor;

        const float delta = interpolatePeak (rL, rM, rR);
        if (delta != 0.0f)
        {
            // Convert from candidate index delta to frequency delta.
            // Candidates are logarithmically spaced: f * 2^(1/48) per step.
            pitch *= std::pow (2.0f, delta / 48.0f);
        }
    }

    // Stability: require N consecutive detections near same pitch.
    float pitchDiff = (lastPitch > 0.0f)
        ? std::abs (std::log2 (pitch / lastPitch))
        : 1.0f;

    if (pitchDiff < 0.05f) // within ~5 cents
    {
        ++stableCount;
        if (stableCount >= STABLE_THRESHOLD)
        {
            // Smooth with EMA.
            pitch = lastPitch * 0.4f + pitch * 0.6f;
            lastPitch = pitch;
            return pitch;
        }
    }
    else
    {
        stableCount = 0;
        lastPitch = pitch;
    }

    // First detection or unstable: still report but keep lastPitch updated.
    lastPitch = pitch;
    return pitch;
}

} // namespace atdsp