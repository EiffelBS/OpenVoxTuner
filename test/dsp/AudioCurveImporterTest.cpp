#pragma once
// AudioCurveImporterTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../../Source/dsp/AudioCurveImporter.h"
#include "../../Source/dsp/PitchCurve.h"

#include <cmath>
#include <initializer_list>

/** Sandbox-safe scratch folder for the generated WAV files.
 *  Same constraint as MidiImporterTest: a test binary whose image lives inside
 *  the workspace runs at Low integrity (S-1-16-4096) and cannot write to %TEMP%,
 *  which is Medium-integrity. Every generated file goes next to the executable,
 *  inside the build tree, where the confined process is allowed to write. */
static juce::File audioScratchDir()
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                         .getParentDirectory().getChildFile ("ovt_audio_scratch");
    dir.createDirectory();
    return dir;
}

/** RAII wrapper: holds a unique temp .wav path and deletes the file on scope
 *  exit. The helper writers below fill it from the test body, so a single
 *  object covers "generate, analyse, delete" without leaking a file when an
 *  assertion fails (expect() does not abort the group). */
class TempAudioFile
{
public:
    TempAudioFile()
        : file (audioScratchDir().getChildFile ("ovt_audio_importer_test_"
                                                + juce::String (juce::Random::getSystemRandom().nextInt (1 << 30))
                                                + ".wav"))
    {
    }

    ~TempAudioFile() { file.deleteFile(); }

    const juce::File& getFile() const { return file; }

    TempAudioFile (const TempAudioFile&) = delete;
    TempAudioFile& operator= (const TempAudioFile&) = delete;

private:
    juce::File file;
};

/** Creates a WAV writer through the JUCE 8 AudioFormatWriterOptions API.
 *  Returns nullptr when the file cannot be opened or the format settings are
 *  not supported (8 and 16 bits are both valid WAV depths). */
static std::unique_ptr<juce::AudioFormatWriter> createWavWriter (const juce::File& file,
                                                                 double sampleRate,
                                                                 int numChannels,
                                                                 int bitsPerSample)
{
    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());

    if (stream == nullptr)
        return nullptr;

    juce::WavAudioFormat wav;
    return wav.createWriterFor (stream,
                                juce::AudioFormatWriter::Options{}
                                    .withSampleRate (sampleRate)
                                    .withNumChannels (numChannels)
                                    .withBitsPerSample (bitsPerSample));
}

/** Writes @p buffer to @p file as a WAV of the requested bit depth.
 *  @returns false when the file cannot be created or written. */
static bool writeWavFile (const juce::File& file, const juce::AudioBuffer<float>& buffer,
                          double sampleRate, int bitsPerSample)
{
    file.deleteFile();

    auto writer = createWavWriter (file, sampleRate, buffer.getNumChannels(), bitsPerSample);

    if (writer == nullptr)
        return false;

    // The writer owns the stream from here on; its destructor finalizes the
    // WAV header, which happens before this function returns.
    return writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
}

/** Writes a silent WAV of @p durationSec without ever holding it in memory.
 *  Used by the "too long" group: 601 s at 8 kHz mono is ~4.8 MB, and the
 *  importer must reject it on its duration alone, before any analysis. */
static bool writeSilentWav (const juce::File& file, double sampleRate, int numChannels,
                            int bitsPerSample, double durationSec)
{
    file.deleteFile();

    auto writer = createWavWriter (file, sampleRate, numChannels, bitsPerSample);

    if (writer == nullptr)
        return false;

    const juce::int64 totalSamples = static_cast<juce::int64> (std::llround (sampleRate * durationSec));

    constexpr int kBlockFrames = 8192;
    juce::AudioBuffer<float> block (numChannels, kBlockFrames);
    block.clear();

    for (juce::int64 written = 0; written < totalSamples; )
    {
        const int numThisTime = static_cast<int> (juce::jmin (static_cast<juce::int64> (kBlockFrames),
                                                              totalSamples - written));

        if (! writer->writeFromAudioSampleBuffer (block, 0, numThisTime))
            return false;

        written += numThisTime;
    }

    return true;
}

/** Equal-tempered frequency of a MIDI note (A4 = 440 Hz). */
static double audioNoteHz (int midiNote)
{
    return 440.0 * std::pow (2.0, (midiNote - 69) / 12.0);
}

/** Allocates a cleared mono/stereo buffer of the given duration. */
static juce::AudioBuffer<float> makeAudioBuffer (int numChannels, double sampleRate,
                                                 double durationSec)
{
    const int numSamples = juce::jmax (1, static_cast<int> (std::lround (sampleRate * durationSec)));
    juce::AudioBuffer<float> buffer (numChannels, numSamples);
    buffer.clear();
    return buffer;
}

/** Adds a vowel-like tone on every channel from @p startSec: fundamental plus
 *  @p numHarmonics harmonics weighted 1/h, scaled to @p amplitude, plus a small
 *  white noise floor so YIN sees a voice-like spectrum rather than a pure sine.
 *  The same noise sample goes to every channel, so a stereo file built this way
 *  is two identical channels (a genuine downmix check). */
static void addTone (juce::AudioBuffer<float>& buffer, double sampleRate,
                     double startSec, double durationSec, double hz,
                     float amplitude, int numHarmonics, float noiseFloor,
                     juce::Random& rng)
{
    const int startSample = static_cast<int> (std::lround (startSec * sampleRate));
    const int lengthSamples = static_cast<int> (std::lround (durationSec * sampleRate));
    const int totalSamples = buffer.getNumSamples();

    float weightSum = 0.0f;
    for (int h = 1; h <= numHarmonics; ++h)
        weightSum += 1.0f / static_cast<float> (h);

    if (weightSum <= 0.0f)
        weightSum = 1.0f;

    for (int i = 0; i < lengthSamples; ++i)
    {
        const int index = startSample + i;

        if (index < 0 || index >= totalSamples)
            continue;

        const double phase = juce::MathConstants<double>::twoPi
                           * hz * static_cast<double> (index) / sampleRate;

        float sample = 0.0f;
        for (int h = 1; h <= numHarmonics; ++h)
            sample += (1.0f / static_cast<float> (h)) * static_cast<float> (std::sin (phase * h));

        sample = amplitude * sample / weightSum;
        sample += noiseFloor * (rng.nextFloat() * 2.0f - 1.0f);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.getWritePointer (ch)[index] += sample;
    }
}

/** Adds a fixed-seed white noise burst on every channel. */
static void addWhiteNoise (juce::AudioBuffer<float>& buffer, double sampleRate,
                           double startSec, double durationSec, float amplitude,
                           juce::Random& rng)
{
    const int startSample = static_cast<int> (std::lround (startSec * sampleRate));
    const int lengthSamples = static_cast<int> (std::lround (durationSec * sampleRate));
    const int totalSamples = buffer.getNumSamples();

    for (int i = 0; i < lengthSamples; ++i)
    {
        const int index = startSample + i;

        if (index < 0 || index >= totalSamples)
            continue;

        const float sample = amplitude * (rng.nextFloat() * 2.0f - 1.0f);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.getWritePointer (ch)[index] += sample;
    }
}

/** Builds a scale interval array from a brace list (assuming a bare
 *  juce::Array<int> initializer_list constructor, which older JUCE tags lack). */
static juce::Array<int> makeIntervals (std::initializer_list<int> semitones)
{
    juce::Array<int> intervals;
    for (int semitone : semitones)
        intervals.add (semitone);
    return intervals;
}

/** Distance in seconds between @p targetSec and the closest curve point.
 *  Returns 1e9 for an empty curve so the calling assertion fails loudly instead
 *  of indexing out of range (an out-of-range getPoint() kills the process). */
static double nearestPointDistance (const ovtdsp::PitchCurve& curve, double targetSec)
{
    double best = 1.0e9;

    for (int i = 0; i < curve.getNumPoints(); ++i)
        best = juce::jmin (best, std::abs (curve.getPoint (i).time - targetSec));

    return best;
}

/** True when the points are stored in ascending time order, as PitchCurve
 *  promises (the importer is expected to preserve it through addOrUpdatePoint). */
static bool pointsAreSortedByTime (const ovtdsp::PitchCurve& curve)
{
    for (int i = 1; i < curve.getNumPoints(); ++i)
        if (curve.getPoint (i).time < curve.getPoint (i - 1).time)
            return false;

    return true;
}

class AudioCurveImporterTest : public juce::UnitTest
{
public:
    AudioCurveImporterTest() : juce::UnitTest ("AudioCurveImporter") {}

    void runTest() override
    {
        using namespace ovtdsp;

        // Detection runs on a 4x-decimated signal (44.1 kHz -> ~11 kHz) with a
        // 46 ms window and a 10 ms hop. Note onsets land within ~10 ms and the
        // end is estimated as "last voiced frame + half a window", so the
        // tolerances below are 30 ms on starts and 40 ms on ends. Pitches are
        // medians of the voiced frames: 1 % is a wide margin around the
        // calibrated ~0.1 % error.
        const double sampleRate = 44100.0;
        const juce::Array<int> noScale; // empty interval set = no snapping

        beginTest ("analyzeFile: metadata of a 44.1 kHz mono file");
        {
            auto buffer = makeAudioBuffer (1, sampleRate, 1.5);
            juce::Random rng (101);
            addTone (buffer, sampleRate, 0.0, 1.5, 220.0, 0.4f, 5, 0.01f, rng);

            TempAudioFile tmp;
            expect (writeWavFile (tmp.getFile(), buffer, sampleRate, 16), "WAV generation failed");

            auto info = AudioCurveImporter::analyzeFile (tmp.getFile());
            expect (info.isValid, "the generated file should be analysable");
            expect (info.errorMessage.isEmpty(), "unexpected error: " + info.errorMessage);
            expectEquals (info.sampleRate, 44100);
            expectEquals (info.numChannels, 1);
            expectWithinAbsoluteError (info.durationSec, 1.5, 0.05);
            expect (info.numNotes > 0, "a sustained tone must yield at least one note");
            expectWithinAbsoluteError ((double) info.minDetectedHz, 220.0, 2.2);
            expectWithinAbsoluteError ((double) info.maxDetectedHz, 220.0, 2.2);

            logMessage ("analyzeFile: " + juce::String (info.numNotes) + " note(s), "
                        + juce::String (info.durationSec, 3) + " s, "
                        + juce::String (info.minDetectedHz, 2) + ".."
                        + juce::String (info.maxDetectedHz, 2) + " Hz");
        }

        beginTest ("importFrom: contiguous four-note melody (pitch and boundaries)");
        {
            // A3 (220) / C4 (261.63) / E4 (329.63) / G4 (392), 0.5 s each,
            // contiguous, after 0.1 s of lead silence.
            const double leadIn = 0.1;
            const double noteDuration = 0.5;
            const int melody[] = { 57, 60, 64, 67 };
            constexpr int numMelodyNotes = 4;

            auto buffer = makeAudioBuffer (1, sampleRate, leadIn + noteDuration * numMelodyNotes);
            juce::Random rng (202);
            for (int i = 0; i < numMelodyNotes; ++i)
                addTone (buffer, sampleRate,
                         leadIn + noteDuration * i, noteDuration,
                         audioNoteHz (melody[i]), 0.4f, 5, 0.01f, rng);

            TempAudioFile tmp;
            expect (writeWavFile (tmp.getFile(), buffer, sampleRate, 16), "WAV generation failed");

            auto info = AudioCurveImporter::analyzeFile (tmp.getFile());
            expect (info.isValid, "the generated melody should be analysable");
            expectEquals (info.numNotes, numMelodyNotes, "four held notes should be detected");

            auto curve = AudioCurveImporter::importFrom (tmp.getFile(), false, noScale);
            expect (curve.isStepMode(), "importFrom must force step mode");

            // Two points per note. Contiguous notes do NOT always collapse
            // their shared boundary: the next onset is detected a few ms after
            // the previous end correction, which is above addOrUpdatePoint()'s
            // 1 ms merge tolerance. Accept anywhere between a fully collapsed
            // curve (N + 1) and two distinct points per note (2 * N).
            expect (curve.getNumPoints() >= numMelodyNotes + 1
                        && curve.getNumPoints() <= 2 * numMelodyNotes,
                    "expected 5 to 8 points, got " + juce::String (curve.getNumPoints()));
            expect (pointsAreSortedByTime (curve), "points must be sorted by time");

            for (int i = 0; i < numMelodyNotes; ++i)
            {
                const double expectedHz = audioNoteHz (melody[i]);
                const double midpoint = leadIn + noteDuration * i + noteDuration * 0.5;

                expectWithinAbsoluteError ((double) curve.getPitchAt (midpoint, 0.0f),
                                           expectedHz, expectedHz * 0.01,
                                           "note " + juce::String (i) + " pitch at its midpoint");

                // Every note onset must be represented by a point: either its
                // own start point or the previous note's end point, close by.
                expect (nearestPointDistance (curve, leadIn + noteDuration * i) <= 0.03,
                        "no point near note " + juce::String (i) + " start");
            }

            // Interior boundaries (end of note i == start of note i+1) and the
            // final end at leadIn + 4 * noteDuration.
            for (int i = 1; i <= numMelodyNotes; ++i)
            {
                const double boundary = leadIn + noteDuration * i;
                expect (nearestPointDistance (curve, boundary) <= 0.04,
                        "no point near the boundary at " + juce::String (boundary, 3) + " s");
            }
        }

        beginTest ("importFrom: separated notes keep their own start and end points");
        {
            // 0.0-0.5 s at 220 Hz, 0.3 s of silence, then 0.8-1.3 s at 329.63 Hz.
            auto buffer = makeAudioBuffer (1, sampleRate, 1.4);
            juce::Random rng (303);
            addTone (buffer, sampleRate, 0.0, 0.5, 220.0, 0.4f, 5, 0.01f, rng);
            addTone (buffer, sampleRate, 0.8, 0.5, audioNoteHz (64), 0.4f, 5, 0.01f, rng);

            TempAudioFile tmp;
            expect (writeWavFile (tmp.getFile(), buffer, sampleRate, 16), "WAV generation failed");

            auto info = AudioCurveImporter::analyzeFile (tmp.getFile());
            expect (info.isValid);
            expectEquals (info.numNotes, 2, "two separated notes should be detected");

            auto curve = AudioCurveImporter::importFrom (tmp.getFile(), false, noScale);
            expectEquals (curve.getNumPoints(), 4,
                          "separated notes give exactly two points each");

            // Guarded: getPoint() past the end of an empty curve crashes the
            // whole test process (see MidiImporterTest for the same rule).
            if (curve.getNumPoints() == 4)
            {
                expectWithinAbsoluteError (curve.getPoint (1).time, 0.5, 0.04,
                                           "first note end");
                expectWithinAbsoluteError (curve.getPoint (2).time, 0.8, 0.03,
                                           "second note start");
            }

            expectWithinAbsoluteError ((double) curve.getPitchAt (0.25, 0.0f), 220.0, 2.2,
                                       "first note pitch");
            expectWithinAbsoluteError ((double) curve.getPitchAt (1.05, 0.0f),
                                       audioNoteHz (64), audioNoteHz (64) * 0.01,
                                       "second note pitch");
        }

        beginTest ("silence: valid metadata, no notes, empty curve");
        {
            auto buffer = makeAudioBuffer (1, sampleRate, 2.0); // already cleared

            TempAudioFile tmp;
            expect (writeWavFile (tmp.getFile(), buffer, sampleRate, 16), "WAV generation failed");

            auto info = AudioCurveImporter::analyzeFile (tmp.getFile());
            expect (info.isValid, "a decodable silent file is not an error");
            expect (info.errorMessage.isEmpty(), "unexpected error: " + info.errorMessage);
            expectEquals (info.numNotes, 0, "silence holds no note");
            expect (info.minDetectedHz == 0.0f, "minDetectedHz must stay 0 without notes");
            expect (info.maxDetectedHz == 0.0f, "maxDetectedHz must stay 0 without notes");

            auto curve = AudioCurveImporter::importFrom (tmp.getFile(), false, noScale);
            expectEquals (curve.getNumPoints(), 0, "silence must give an empty curve");
        }

        beginTest ("white noise: no ghost notes");
        {
            auto buffer = makeAudioBuffer (1, sampleRate, 2.0);
            juce::Random rng (404); // fixed seed: the result must be reproducible
            addWhiteNoise (buffer, sampleRate, 0.0, 2.0, 0.3f, rng);

            TempAudioFile tmp;
            expect (writeWavFile (tmp.getFile(), buffer, sampleRate, 16), "WAV generation failed");

            auto info = AudioCurveImporter::analyzeFile (tmp.getFile());
            expect (info.isValid, "noise is a valid file, just not a pitched one");
            expectEquals (info.numNotes, 0, "unvoiced noise must not create notes");

            auto curve = AudioCurveImporter::importFrom (tmp.getFile(), false, noScale);
            expectEquals (curve.getNumPoints(), 0, "noise must give an empty curve");
        }

        beginTest ("short blip before a long note is filtered out");
        {
            // 50 ms at 300 Hz (below the 80 ms note floor) then 500 ms at 220 Hz.
            auto buffer = makeAudioBuffer (1, sampleRate, 1.1);
            juce::Random rng (505);
            addTone (buffer, sampleRate, 0.0, 0.05, 300.0, 0.4f, 5, 0.01f, rng);
            addTone (buffer, sampleRate, 0.4, 0.5, 220.0, 0.4f, 5, 0.01f, rng);

            TempAudioFile tmp;
            expect (writeWavFile (tmp.getFile(), buffer, sampleRate, 16), "WAV generation failed");

            auto info = AudioCurveImporter::analyzeFile (tmp.getFile());
            expect (info.isValid);
            expectEquals (info.numNotes, 1, "the 50 ms blip must be dropped");

            auto curve = AudioCurveImporter::importFrom (tmp.getFile(), false, noScale);
            expectEquals (curve.getNumPoints(), 2, "one note gives two points");
            expectWithinAbsoluteError ((double) curve.getPitchAt (0.65, 0.0f), 220.0, 2.2,
                                       "the long note's pitch");
        }

        beginTest ("snap to scale: 250 Hz tone lands on B3, off keeps the raw pitch");
        {
            // 250 Hz sits 21 cents above B3 (246.94 Hz), the nearest C major note.
            auto buffer = makeAudioBuffer (1, sampleRate, 1.0);
            juce::Random rng (606);
            addTone (buffer, sampleRate, 0.0, 1.0, 250.0, 0.4f, 4, 0.01f, rng);

            TempAudioFile tmp;
            expect (writeWavFile (tmp.getFile(), buffer, sampleRate, 16), "WAV generation failed");

            const juce::Array<int> cMajor = makeIntervals ({ 0, 2, 4, 5, 7, 9, 11 });
            const double b3 = audioNoteHz (59); // 246.94 Hz
            const double midpoint = 0.5;

            auto snapped = AudioCurveImporter::importFrom (tmp.getFile(), true, cMajor);
            expect (snapped.isStepMode(), "importFrom must force step mode");
            expectWithinAbsoluteError ((double) snapped.getPitchAt (midpoint, 0.0f), b3, 0.5,
                                       "snapped pitch must be exactly B3");

            auto raw = AudioCurveImporter::importFrom (tmp.getFile(), false, cMajor);
            expect (raw.getNumPoints() > 0, "the tone must be detected without snapping");
            expectWithinAbsoluteError ((double) raw.getPitchAt (midpoint, 0.0f), 250.0, 2.5,
                                       "unsnapped pitch must stay within 1 % of 250 Hz");

            // Snapping requested with an empty interval set is a no-op, exactly
            // like PitchCurve::snapToIntervals().
            auto emptyScale = AudioCurveImporter::importFrom (tmp.getFile(), true, noScale);
            expectWithinAbsoluteError ((double) emptyScale.getPitchAt (midpoint, 0.0f), 250.0, 2.5,
                                       "an empty scale must not snap");
        }

        beginTest ("unreadable input: missing and garbage files rejected");
        {
            const juce::File missing = audioScratchDir()
                                           .getChildFile ("ovt_audio_importer_missing_file.wav");
            missing.deleteFile();

            auto info = AudioCurveImporter::analyzeFile (missing);
            expect (! info.isValid, "a nonexistent path must be reported invalid");
            expect (info.errorMessage.isNotEmpty(), "an error message must be reported");
            expect (info.errorMessage.startsWith ("Unsupported or unreadable audio file"),
                    "unexpected message: " + info.errorMessage);

            auto curve = AudioCurveImporter::importFrom (missing, false, noScale);
            expectEquals (curve.getNumPoints(), 0, "a failed decode must give an empty curve");

            TempAudioFile junk;
            expect (junk.getFile().replaceWithText ("this is definitely not a WAV file,"
                                                    " not even close to RIFF/WAVE"),
                    "could not write the garbage file");
            expect (junk.getFile().getSize() > 0, "the garbage file must not be empty");

            auto junkInfo = AudioCurveImporter::analyzeFile (junk.getFile());
            expect (! junkInfo.isValid, "garbage bytes must not decode");
            expect (junkInfo.errorMessage.isNotEmpty(), "an error message must be reported");

            auto junkCurve = AudioCurveImporter::importFrom (junk.getFile(), false, noScale);
            expectEquals (junkCurve.getNumPoints(), 0, "a garbage file must give an empty curve");
        }

        beginTest ("stereo file: downmixed detection keeps the pitch");
        {
            auto buffer = makeAudioBuffer (2, sampleRate, 1.0);
            juce::Random rng (707);
            // addTone() feeds the same sample to both channels: a true downmix test.
            addTone (buffer, sampleRate, 0.0, 1.0, 220.0, 0.4f, 5, 0.01f, rng);

            TempAudioFile tmp;
            expect (writeWavFile (tmp.getFile(), buffer, sampleRate, 16), "WAV generation failed");

            auto info = AudioCurveImporter::analyzeFile (tmp.getFile());
            expect (info.isValid);
            expectEquals (info.numChannels, 2);
            expectEquals (info.sampleRate, 44100);
            expect (info.numNotes >= 1, "the downmixed tone must be detected");

            auto curve = AudioCurveImporter::importFrom (tmp.getFile(), false, noScale);
            expectWithinAbsoluteError ((double) curve.getPitchAt (0.5, 0.0f), 220.0, 2.2,
                                       "stereo downmix must keep the 220 Hz pitch");
        }

        beginTest ("too long: 601 s file rejected before analysis");
        {
            // 601 s of silence at 8 kHz, 8-bit mono: ~4.8 MB, written in blocks.
            // The duration check must run before any decoding/analysis work.
            TempAudioFile tmp;
            expect (writeSilentWav (tmp.getFile(), 8000.0, 1, 8, 601.0),
                    "long WAV generation failed");
            expect (tmp.getFile().getSize() > 1000000, "the long file should hold real content");

            auto info = AudioCurveImporter::analyzeFile (tmp.getFile());
            expect (! info.isValid, "a 601 s file exceeds the 10 minute limit");
            expect (info.errorMessage.contains ("10 minutes"),
                    "unexpected message: " + info.errorMessage);
            expectEquals (info.numNotes, 0, "a rejected file has no detected notes");

            auto curve = AudioCurveImporter::importFrom (tmp.getFile(), false, noScale);
            expectEquals (curve.getNumPoints(), 0, "a too long file must give an empty curve");
        }
    }
};

static AudioCurveImporterTest audioCurveImporterTest;
