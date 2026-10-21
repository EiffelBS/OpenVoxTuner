// AudioCurveImporter.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PitchCurve.h"

namespace ovtdsp
{
    /** Metadata about an audio file, plus the notes detected in it.
     *  analyzeFile() fills every field; importFrom() only reads the metadata
     *  and regenerates the curve. */
    struct AudioImportInfo
    {
        int numChannels = 0;         // channels in the source file
        int sampleRate = 0;          // source sample rate (Hz)
        double durationSec = 0.0;    // source duration (seconds)
        int numNotes = 0;            // note spans kept after filtering
        float minDetectedHz = 0.0f;  // lowest note pitch (0 when numNotes == 0)
        float maxDetectedHz = 0.0f;  // highest note pitch (0 when numNotes == 0)
        bool isValid = false;        // file readable, decodable and within the duration limit
        juce::String errorMessage;   // human-readable reason when ! isValid
    };

    /**
     * Utility class that turns a recorded audio file into a PitchCurve.
     *
     * Audio counterpart of MidiImporter: same static-only pattern and the same
     * output convention. The pipeline is
     *   1) decode the file with juce::AudioFormatManager (WAV/AIFF/FLAC/OGG),
     *   2) downmix to mono and decimate by a small factor (YIN cost),
     *   3) run the YIN detector frame by frame (46 ms window / 10 ms hop),
     *   4) segment voiced frames into notes (same note within +/-50 cents,
     *      merge gaps < 80 ms, drop notes shorter than 80 ms, pitch = median),
     *   5) emit two points per note (start and end) at the median pitch,
     *      optionally snapped to the active scale.
     *
     * Time convention: point times are in SECONDS, exactly like MidiImporter.
     * (The DSP consumes beats; that pre-existing mismatch is tracked
     * separately and is intentionally not "fixed" here, so both importers
     * behave identically.)
     *
     * MP3 is not supported: JUCE's MP3 decoder is disabled in this build.
     * Supported: WAV, AIFF, FLAC, OGG-Vorbis.
     *
     * All members are thread-agnostic pure functions, but decoding a long file
     * takes time (roughly 1 s for 3 min of mono): call them from the message
     * thread, never from processBlock().
     */
    class AudioCurveImporter
    {
    public:
        // == Tuning constants (public so tests and the UI can reference them) ==
        static constexpr double kMaxDurationSec   = 600.0;  // 10 min hard limit
        static constexpr double kWindowSec        = 0.046;  // YIN analysis window
        static constexpr double kHopSec           = 0.010;  // frame hop
        static constexpr double kMinNoteSec       = 0.080;  // shorter notes are filtered
        static constexpr double kMaxGapSec        = 0.080;  // unvoiced gap merged when smaller
        static constexpr float  kNoteToleranceCents = 50.0f;// same-note tolerance
        static constexpr float  kDefaultMinHz     = 60.0f;  // default detection range
        static constexpr float  kDefaultMaxHz     = 1000.0f;

        /** Analyze an audio file without building a curve.
         *  Decodes the file and runs the full detection so the import dialog
         *  can show the duration and the number of detected notes.
         *  @param audioFile  path to a WAV/AIFF/FLAC/OGG file
         *  @param minHz      lowest pitch to look for (default 60 Hz)
         *  @param maxHz      highest pitch to look for (default 1000 Hz)
         */
        static AudioImportInfo analyzeFile (const juce::File& audioFile,
                                            float minHz = kDefaultMinHz,
                                            float maxHz = kDefaultMaxHz);

        /** Import an audio file and return a ready-to-edit PitchCurve.
         *  The returned curve has step mode enabled (imported notes are
         *  discrete), two points per detected note, in seconds, sorted by time.
         *  An unreadable, too long, silent or pitch-less file yields an empty
         *  curve (call analyzeFile() first to explain why to the user).
         *  @param audioFile       path to a WAV/AIFF/FLAC/OGG file
         *  @param snapToScale     snap every note pitch to the scale below
         *  @param scaleIntervals  absolute semitone offsets in [0, 11], already
         *                         shifted by the musical key (empty = no snap)
         */
        static PitchCurve importFrom (const juce::File& audioFile,
                                      bool snapToScale,
                                      const juce::Array<int>& scaleIntervals,
                                      float minHz = kDefaultMinHz,
                                      float maxHz = kDefaultMaxHz);

    private:
        AudioCurveImporter() = delete; // static-only class
    };
}
