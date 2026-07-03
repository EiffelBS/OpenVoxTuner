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

    // Zero out the kernel.
    std::memset (kernel, 0, (size_t)halfSize * sizeof (float));

    // SWIPE' kernel: place sawtooth weights (1/h) at the EXACT harmonic
    // positions. No Gaussian spread. The kernel is just a set of discrete
    // weights at bin positions corresponding to h*f for h=1..H.
    // This is critical: spreading kernel energy over multiple bins with
    // Gaussians lets a wrong candidate's harmonics "catch" energy from
    // nearby voice harmonics, artificially inflating the correlation.
    for (int h = 1; h <= MAX_KERNEL_HARMONICS; ++h)
    {
        float harmFreq = freq * (float)h;
        int bin = (int)std::round (harmFreq * fftSz / sampleRate);
        if (bin >= halfSize) break;

        // Sawtooth amplitude: 1/h
        kernel[bin] = 1.0f / (float)h;
    }

    // Normalize kernel to unit energy (L2 norm = 1).
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
                                               int halfSize,
                                               float signalEnergy)
{
    // SWIPE' spectral correlation: dot product of spectrum(k) * kernel(k)
    // normalized by total spectrum energy. The kernel is unit-normalized
    // (L2=1) and zero everywhere except at exact harmonic positions.
    //
    // The numerator sum(s[k] * kernel[k]) captures energy at the candidate's
    // harmonic positions. The denominator sqrt(signalEnergy) normalizes by
    // the TOTAL signal energy across ALL bins. This means a candidate whose
    // harmonics align with strong voice energy scores high, while one whose
    // harmonics miss the voice energy scores low — even if the few bins it
    // does hit have some residual energy from formants or spectral leakage.
    float sumSK = 0.0f;

    for (int bin = 1; bin < halfSize; ++bin)
    {
        const float k = kernel[bin];
        if (k == 0.0f) continue;
        sumSK += spectrum[bin] * k;
    }

    if (signalEnergy < 1e-10f)
        return 0.0f;

    const float denom = std::sqrt (signalEnergy);
    if (denom < 1e-10f || sumSK < 1e-10f)
        return 0.0f;

    // Normalize by total spectrum energy.
    // At full voice level, signalEnergy ~ 500-5000, giving score in 0..~0.8.
    // The energy scale penalty suppresses near-silence noise floors.
    const float energyScale = juce::jmin (1.0f, signalEnergy / 100.0f);

    return (sumSK / denom) * energyScale;
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

    // Search over candidate pitches for best correlation.
    int bestIdx = -1;
    float bestCorr = -1.0f;
    for (int c = 0; c < numCandidates; ++c)
    {
        // computeCorrelation returns 0-1 SWIPE'-style spectral correlation.
        float corr = computeCorrelation (spectrumMag.getData(),
                                         kernelCachePtrs[c],
                                         halfSize,
                                         signalEnergy);

        // Very mild freq bias: at 1000Hz -> *0.95, at 30Hz -> *1.0.
        // The spectral correlation is already naturally fair, this just
        // gives a slight nudge toward lower frequencies to break ties.
        const float freqBias = 1.0f - 0.05f * candidateFreqs[c] / 1000.0f;
        corr *= freqBias;

        if (corr > bestCorr && corr > threshold)
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

        // computeCorrelation returns 0-1 SWIPE'-style spectral correlation.
        float rL = computeCorrelation (spectrumMag.getData(),
                                       kernelCachePtrs[idxL], halfSize, signalEnergy);
        float rM = bestCorr;
        float rR = computeCorrelation (spectrumMag.getData(),
                                       kernelCachePtrs[idxR], halfSize, signalEnergy);

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