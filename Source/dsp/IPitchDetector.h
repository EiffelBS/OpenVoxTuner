// IPitchDetector.h
// Abstract interface for pitch detection algorithms.
// Allows runtime switching between YIN and SWIPE' implementations
// with a clean rollback path.
//
// References:
//   YIN:  de Cheveigne & Kawahara, J. Acoust. Soc. Am. 111 (4), 2002
//   SWIPE': Camacho & Harris, IEEE Trans. ASLP, 2008

#pragma once

#include <juce_core/juce_core.h>

namespace atdsp
{

/**
 * Abstract interface for pitch detection algorithms.
 *
 * All implementations must provide:
 *   - prepare()  : init with sample rate and block size
 *   - reset()    : clear internal state
 *   - detectPitch() : return fundamental frequency in Hz (0.0 = no pitch)
 *   - setThreshold() / getThreshold() : sensitivity control
 */
class IPitchDetector
{
public:
    virtual ~IPitchDetector() = default;

    /** Prepare the detector with the given sample rate and block size.
     *  Must be called before detectPitch(). */
    virtual void prepare (double sampleRate, int blockSize) = 0;

    /** Reset internal state (history, buffers, etc.). */
    virtual void reset() = 0;

    /** Detect the fundamental frequency from a mono audio buffer.
     *  @param samples    audio samples (float)
     *  @param numSamples number of samples
     *  @return fundamental frequency in Hz, or 0.0 if no pitch detected */
    virtual float detectPitch (const float* samples, int numSamples) = 0;

    /** Set the detection threshold (0.01 - 0.99). Lower = stricter. */
    virtual void setThreshold (float t) = 0;

    /** Get the current detection threshold. */
    virtual float getThreshold() const = 0;

    /** Human-readable name of the algorithm (for UI/logging). */
    virtual juce::String getName() const = 0;
};

} // namespace atdsp