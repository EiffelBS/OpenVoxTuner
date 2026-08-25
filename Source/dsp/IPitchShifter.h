// IPitchShifter.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.


#include <juce_audio_processors/juce_audio_processors.h>

namespace ovtdsp
{
    /**
     * Abstract interface for pitch shifting engines.
     * Allows swapping RubberBand, SoundTouch or a custom PSOLA
     * implementation transparently for the rest of the pipeline.
     */
    class IPitchShifter
    {
    public:
        virtual ~IPitchShifter() = default;

        /**
         * Prepares the shifter with the sample rate and block size.
         */
        virtual void prepare (double sampleRate, int maximumBlockSize) = 0;

        /**
         * Resets the internal state (e.g. flushes FIFOs).
         */
        virtual void reset() = 0;

        /**
         * Processes an audio buffer in place.
         * @param buffer The stereo audio buffer to modify.
         * @param ratio The transposition ratio (e.g. 1.0 = none, 2.0 = +1 octave).
         * @param f0 The detected fundamental frequency (required by some
         *        algorithms such as PSOLA).
         */
        virtual void process (juce::AudioBuffer<float>& buffer, float ratio, float f0) = 0;

        /**
         * Returns the engine's internal latency in samples.
         */
        virtual int getLatencySamples() const = 0;
    };
}



