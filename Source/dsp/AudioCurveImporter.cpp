// AudioCurveImporter.cpp
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.

#include "AudioCurveImporter.h"
#include "YinPitchDetector.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace ovtdsp
{
namespace
{
    // === Decoding ===

    /** A decoded file, already downmixed to mono and decimated for analysis.
     *  Only the decimated mono track is kept in memory (a few MB per minute),
     *  never the full multi-channel native-rate buffer: a 10 min stereo 48 kHz
     *  take would otherwise need ~230 MB. */
    struct DecodedAudio
    {
        std::vector<float> samples;   // mono, decimated
        double analysisRate = 0.0;    // source sample rate / decimation factor
        int sourceChannels = 0;
        int sourceSampleRate = 0;
        double sourceDurationSec = 0.0;
    };

    /** Decimation factor that keeps YIN's cost bounded (~12 kHz analysis rate)
     *  while staying well above the Nyquist frequency of the vocal range. */
    int decimationFactorFor (int sourceSampleRate)
    {
        const int factor = static_cast<int> (std::round (sourceSampleRate / 12000.0));
        return juce::jlimit (1, 16, factor);
    }

    juce::String formatErrorMessage (const juce::String& fileName)
    {
        return "Unsupported or unreadable audio file: " + fileName;
    }

    /** Opens and decodes an audio file. Returns false and fills @p error on
     *  any failure (missing file, unknown codec, empty file, too long). */
    bool decodeToMonoDecimated (const juce::File& audioFile,
                                DecodedAudio& decoded,
                                juce::String& error)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats(); // WAV / AIFF / FLAC / OGG-Vorbis

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (audioFile));
        if (reader == nullptr)
        {
            error = formatErrorMessage (audioFile.getFileName());
            return false;
        }

        if (reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0
            || reader->numChannels == 0)
        {
            error = "The audio file is empty.";
            return false;
        }

        const double durationSec = static_cast<double> (reader->lengthInSamples)
                                 / reader->sampleRate;
        if (durationSec > AudioCurveImporter::kMaxDurationSec)
        {
            error = "The audio file is longer than "
                  + juce::String (static_cast<int> (AudioCurveImporter::kMaxDurationSec / 60.0))
                  + " minutes.";
            return false;
        }

        decoded = DecodedAudio{};
        decoded.sourceChannels = static_cast<int> (reader->numChannels);
        decoded.sourceSampleRate = static_cast<int> (std::round (reader->sampleRate));
        decoded.sourceDurationSec = durationSec;

        const int decimation = decimationFactorFor (decoded.sourceSampleRate);
        decoded.analysisRate = reader->sampleRate / decimation;

        const auto expectedSamples = static_cast<size_t> (std::ceil (durationSec
                                                                   * decoded.analysisRate));
        decoded.samples.reserve (expectedSamples + 8);

        // Read in blocks: the native-rate audio is never fully resident.
        constexpr int kBlockFrames = 8192;
        juce::AudioBuffer<float> block (decoded.sourceChannels, kBlockFrames);
        const float channelScale = 1.0f / static_cast<float> (decoded.sourceChannels);

        float accumulator = 0.0f;
        int accumulatorCount = 0;

        juce::int64 position = 0;
        while (position < reader->lengthInSamples)
        {
            const int numThisTime = static_cast<int> (
                juce::jmin (static_cast<juce::int64> (kBlockFrames),
                            reader->lengthInSamples - position));

            if (numThisTime <= 0)
                break;

            // Both channel flags true: every destination channel is filled from
            // the matching source channel (JUCE maps 1:1 for multi-channel).
            if (! reader->read (&block, 0, numThisTime, position, true, true))
                break;

            for (int i = 0; i < numThisTime; ++i)
            {
                float mono = 0.0f;
                for (int ch = 0; ch < decoded.sourceChannels; ++ch)
                    mono += block.getReadPointer (ch)[i];
                mono *= channelScale;

                // Boxcar decimation (same anti-aliasing filter as the live
                // detector's 4-sample average, generalised to the factor).
                accumulator += mono;
                if (++accumulatorCount == decimation)
                {
                    decoded.samples.push_back (accumulator / static_cast<float> (decimation));
                    accumulator = 0.0f;
                    accumulatorCount = 0;
                }
            }

            position += numThisTime;
        }

        if (decoded.samples.empty())
        {
            error = "The audio file is empty.";
            return false;
        }

        return true;
    }

    // === Note detection ===

    struct DetectedNote
    {
        double startSec = 0.0;
        double endSec = 0.0;
        float pitchHz = 0.0f;
        std::vector<float> framePitches; // every voiced frame of the note
    };

    /** YIN clarity threshold for offline analysis. Stricter than a live
     *  tracking threshold would need (there is no continuity to lean on), but
     *  loose enough for vowel-like singing; white noise stays above it. */
    constexpr float kOfflineThreshold = 0.10f;

    /** Absolute distance between two pitches in cents (1e9 if either is 0). */
    float centsBetween (float a, float b)
    {
        if (a <= 0.0f || b <= 0.0f)
            return 1.0e9f;
        return 1200.0f * std::abs (std::log2 (a / b));
    }

    /** Median of the last @p maxCount values of @p values (no allocation). */
    float medianOfLast (const std::vector<float>& values, int maxCount)
    {
        const int n = static_cast<int> (values.size());
        const int take = juce::jmin (maxCount, n);
        if (take <= 0)
            return 0.0f;

        float sorted[16];
        const int count = juce::jmin (take, static_cast<int> (sizeof (sorted) / sizeof (sorted[0])));
        for (int i = 0; i < count; ++i)
            sorted[i] = values[static_cast<size_t> (n - count + i)];

        // Insertion sort: at most 16 values.
        for (int i = 1; i < count; ++i)
        {
            const float key = sorted[i];
            int j = i - 1;
            while (j >= 0 && sorted[j] > key)
            {
                sorted[j + 1] = sorted[j];
                --j;
            }
            sorted[j + 1] = key;
        }
        return sorted[count / 2];
    }

    /** Median of all values in the vector (empty vector -> 0). */
    float medianOfAll (const std::vector<float>& values)
    {
        if (values.empty())
            return 0.0f;
        std::vector<float> sorted (values);
        std::sort (sorted.begin(), sorted.end());
        return sorted[sorted.size() / 2];
    }

    /** Runs YIN frame by frame and groups the voiced frames into notes.
     *  Rules (see the header): same note within +/-50 cents, merge notes
     *  separated by less than 80 ms, drop notes shorter than 80 ms, and use the
     *  median pitch of the note's frames as its pitch. */
    void detectNotes (const DecodedAudio& audio,
                      float minHz,
                      float maxHz,
                      std::vector<DetectedNote>& notes)
    {
        notes.clear();
        if (audio.analysisRate <= 0.0 || audio.samples.size() < 64)
            return;

        // Keep the requested range inside what the decimated rate can resolve.
        maxHz = juce::jmin (maxHz, static_cast<float> (audio.analysisRate * 0.45));
        minHz = juce::jlimit (30.0f, juce::jmax (31.0f, maxHz - 10.0f), minHz);
        if (maxHz <= minHz)
            return;

        const int windowSamples = juce::jmax (
            64, static_cast<int> (std::round (AudioCurveImporter::kWindowSec * audio.analysisRate)));
        const int hopSamples = juce::jmax (
            1, static_cast<int> (std::round (AudioCurveImporter::kHopSec * audio.analysisRate)));
        const int totalSamples = static_cast<int> (audio.samples.size());
        const int lastFrameStart = totalSamples - windowSamples;
        if (lastFrameStart < 0)
            return;

        YinPitchDetector detector;
        detector.prepare (audio.analysisRate, windowSamples);
        detector.setFrequencyRange (minHz, maxHz);
        detector.setThreshold (kOfflineThreshold);
        detector.setOfflineMode (true); // first YIN minimum = fundamental
        detector.reset();

        // 1) Frame track: time (seconds, frame window start) + raw YIN pitch.
        struct Frame
        {
            double time = 0.0;
            float hz = 0.0f;
        };

        std::vector<Frame> frames;
        frames.reserve (static_cast<size_t> (lastFrameStart / hopSamples) + 1);

        for (int start = 0; start <= lastFrameStart; start += hopSamples)
        {
            Frame frame;
            frame.time = static_cast<double> (start) / audio.analysisRate;
            frame.hz = detector.detectPitch (audio.samples.data() + start, windowSamples);
            frames.push_back (frame);
        }

        // 2) Voiced-frame segmentation, comparing each new frame with the
        //    running median of the current note (robust to isolated outliers).
        std::vector<DetectedNote> segments;
        DetectedNote current;
        bool hasCurrent = false;

        for (const auto& frame : frames)
        {
            if (frame.hz <= 0.0f)
            {
                if (hasCurrent)
                {
                    segments.push_back (std::move (current));
                    current = DetectedNote{};
                    hasCurrent = false;
                }
                continue;
            }

            if (! hasCurrent)
            {
                current.startSec = frame.time;
                current.endSec = frame.time;
                current.framePitches.push_back (frame.hz);
                hasCurrent = true;
                continue;
            }

            const float reference = medianOfLast (current.framePitches, 9);
            if (centsBetween (frame.hz, reference) <= AudioCurveImporter::kNoteToleranceCents)
            {
                current.endSec = frame.time;
                current.framePitches.push_back (frame.hz);
            }
            else
            {
                segments.push_back (std::move (current));
                current = DetectedNote{};
                current.startSec = frame.time;
                current.endSec = frame.time;
                current.framePitches.push_back (frame.hz);
                hasCurrent = true;
            }
        }
        if (hasCurrent)
            segments.push_back (std::move (current));

        if (segments.empty())
            return;

        // 3) Merge consecutive segments that are the same note interrupted by a
        //    short unvoiced gap (consonant, breath, detector dropout).
        const double halfWindow = 0.5 * AudioCurveImporter::kWindowSec;
        std::vector<DetectedNote> merged;
        merged.reserve (segments.size());

        for (auto& segment : segments)
        {
            segment.pitchHz = medianOfAll (segment.framePitches);
            // The last analysing window extends past the note's end by up to
            // one window; half a window is the best available boundary estimate.
            segment.endSec = juce::jmin (segment.endSec + halfWindow,
                                         audio.sourceDurationSec);

            if (! merged.empty())
            {
                auto& previous = merged.back();
                const double gap = segment.startSec - previous.endSec;
                if (gap <= AudioCurveImporter::kMaxGapSec
                    && centsBetween (segment.pitchHz, previous.pitchHz)
                           <= AudioCurveImporter::kNoteToleranceCents)
                {
                    previous.endSec = segment.endSec;
                    previous.framePitches.insert (previous.framePitches.end(),
                                                  segment.framePitches.begin(),
                                                  segment.framePitches.end());
                    previous.pitchHz = medianOfAll (previous.framePitches);
                    continue;
                }
            }

            merged.push_back (std::move (segment));
        }

        // 4) Drop artefacts shorter than the minimum note length.
        notes.reserve (merged.size());
        for (auto& note : merged)
            if (note.endSec - note.startSec >= AudioCurveImporter::kMinNoteSec)
                notes.push_back (std::move (note));

        // 5) Two notes can never overlap: the end correction above may push a
        //    note past the next one's onset (contiguous legato singing), and an
        //    overlapping point pair would insert a spurious pitch spike.
        for (size_t i = 0; i + 1 < notes.size(); ++i)
            notes[i].endSec = juce::jmin (notes[i].endSec, notes[i + 1].startSec);
    }

    /** Builds the final curve: two points per note at its (optionally snapped)
     *  median pitch, step mode on (imported notes are discrete). */
    PitchCurve buildCurve (const std::vector<DetectedNote>& notes,
                           bool snapToScale,
                           const juce::Array<int>& scaleIntervals)
    {
        PitchCurve curve;

        for (const auto& note : notes)
        {
            float hz = note.pitchHz;
            if (snapToScale && ! scaleIntervals.isEmpty())
                hz = PitchCurve::snapToIntervals (hz, scaleIntervals);

            curve.addOrUpdatePoint (note.startSec, hz);
            curve.addOrUpdatePoint (note.endSec, hz);
        }

        curve.setStepMode (true);
        return curve;
    }
} // namespace

AudioImportInfo AudioCurveImporter::analyzeFile (const juce::File& audioFile,
                                                 float minHz,
                                                 float maxHz)
{
    AudioImportInfo info;

    DecodedAudio decoded;
    juce::String error;
    if (! decodeToMonoDecimated (audioFile, decoded, error))
    {
        info.errorMessage = error;
        return info;
    }

    info.numChannels = decoded.sourceChannels;
    info.sampleRate = decoded.sourceSampleRate;
    info.durationSec = decoded.sourceDurationSec;

    std::vector<DetectedNote> notes;
    detectNotes (decoded, minHz, maxHz, notes);

    info.numNotes = static_cast<int> (notes.size());
    if (! notes.empty())
    {
        float lowest = notes.front().pitchHz;
        float highest = notes.front().pitchHz;
        for (const auto& note : notes)
        {
            lowest = juce::jmin (lowest, note.pitchHz);
            highest = juce::jmax (highest, note.pitchHz);
        }
        info.minDetectedHz = lowest;
        info.maxDetectedHz = highest;
    }

    info.isValid = true;
    return info;
}

PitchCurve AudioCurveImporter::importFrom (const juce::File& audioFile,
                                           bool snapToScale,
                                           const juce::Array<int>& scaleIntervals,
                                           float minHz,
                                           float maxHz)
{
    PitchCurve curve;

    DecodedAudio decoded;
    juce::String error;
    if (! decodeToMonoDecimated (audioFile, decoded, error))
        return curve; // analyzed first by the caller, which reports the reason

    std::vector<DetectedNote> notes;
    detectNotes (decoded, minHz, maxHz, notes);

    return buildCurve (notes, snapToScale, scaleIntervals);
}

} // namespace ovtdsp
