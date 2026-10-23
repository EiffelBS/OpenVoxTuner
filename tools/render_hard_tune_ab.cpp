// tools/render_hard_tune_ab.cpp
// OpenVoxTuner — offline Hard Tune shifter A/B renderer
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Renders the SAME Hard Tune correction trajectory through different pitch
// shifters, so the shifter's character can be judged by ear on one take:
//
//   OLA 10 ms, shipped preset colour (creative Formant +2 semitones)
//   OLA 10 ms, formant 0                (isolates the creative formant from the shifter)
//   Doppler probe 2.5 ms                (tools/DopplerProbe.h; formants follow pitch)
//   Doppler probe 4.0 ms
//
// The correction chain mirrors the Hard Tune mode: YIN detection with the short
// analysis window (2048/512 at 44.1 kHz, median 3), chromatic quantisation,
// zero retarget smoothing, no speed floor and no ratio-jump duck (the OLA
// shifter keeps its note-onset mask). It is NOT the full plugin chain: no
// detector-side anti-octave filter, no de-esser/saturation/limiter, no formant
// preserver - so the comparison stays about the shifter itself.
//
// Outputs are written to <outDir> (default build/ab_hard_tune): dry.wav,
// take_01.wav .. take_04.wav (anonymised, shuffled order) and key.txt. Listen
// BEFORE reading key.txt.
//
// Usage:
//   render_hard_tune_ab [input.wav] [output-dir]
// With no input a synthetic voice-like phrase is used (notes with vibrato plus
// short fricative bursts) so the tool is runnable out of the box.

#include "../Source/dsp/PitchShifter.h"
#include "../Source/dsp/YinPitchDetector.h"
#include "DopplerProbe.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace
{
    constexpr int    kBlock   = 256;
    constexpr double kRefRate = 44100.0;

    double twoPi() { return 2.0 * std::acos (-1.0); }

    struct Detection
    {
        long long sample = 0; // window end, in samples
        float f0 = 0.0f;      // detected pitch (Hz)
        float ratio = 1.0f;   // correction ratio to the quantised target
    };

    // ------------------------------------------------------------------
    // Input
    // ------------------------------------------------------------------

    /** Synthetic fallback: a short sung phrase with vibrato and fricatives. */
    std::vector<float> makePhrase (double sr)
    {
        const double notes[] = { 220.0, 261.63, 329.63, 293.66, 220.0 };
        // Deliberate intonation errors: an in-tune take cannot demonstrate a tuner
        // (the first A/B pass on tune-perfect material made every OLA take sound
        // like the dry take). Each note is sung 27-44 cents off; every offset stays
        // inside +/-50 cents, so the chromatic snap pulls each note back to its own
        // semitone and the correction is audible.
        const double detuneCents[] = { 38.0, -42.0, 27.0, -35.0, 44.0 };
        const double noteDur = 1.15;
        const double gap = 0.10;
        const int n = (int) std::llround ((5 * (noteDur + gap) + 0.4) * sr);
        std::vector<float> v ((size_t) n, 0.0f);

        for (int note = 0; note < 5; ++note)
        {
            const double start = 0.2 + note * (noteDur + gap);
            const int s0 = (int) std::llround (start * sr);
            const int len = (int) std::llround (noteDur * sr);
            const double f0 = notes[note] * std::pow (2.0, detuneCents[note] / 1200.0);
            double phase = 0.0;

            for (int i = 0; i < len && s0 + i < n; ++i)
            {
                const double t = i / sr;
                const double env = std::min ({ 1.0, t / 0.035, (noteDur - t) / 0.06 });
                const double vib = (t > 0.25) ? 20.0 * std::sin (twoPi() * 5.0 * (t - 0.25)) : 0.0;
                const double inst = f0 * std::pow (2.0, vib / 1200.0);
                // Integrate the phase: writing sin(2*pi*inst*t) with a time-varying
                // inst multiplies a drift by t and destroys the intended pitch.
                phase += twoPi() * inst / sr;
                const double s = 0.60 * std::sin (phase)
                               + 0.28 * std::sin (2.0 * phase)
                               + 0.12 * std::sin (3.0 * phase);
                v[(size_t) (s0 + i)] += (float) (0.45 * juce::jmax (0.0, env) * s);
            }

            // Short fricative before the next note: wide-band noise burst.
            if (note < 4)
            {
                const int b0 = (int) std::llround ((start + noteDur + 0.02) * sr);
                std::mt19937 rng (1000 + note);
                std::uniform_real_distribution<float> dist (-1.0f, 1.0f);
                for (int i = 0; i < (int) (0.075 * sr) && b0 + i < n; ++i)
                {
                    const double env = std::sin (juce::MathConstants<double>::pi * i / (0.075 * sr));
                    v[(size_t) (b0 + i)] += (float) (0.12 * env * dist (rng));
                }
            }
        }
        return v;
    }

    std::vector<float> readWav (const juce::File& file, double& sampleRateOut)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
        if (reader == nullptr)
        {
            std::printf ("ERROR: cannot read '%s'\n", file.getFullPathName().toRawUTF8());
            return {};
        }

        sampleRateOut = reader->sampleRate;
        const int n = (int) reader->lengthInSamples;
        juce::AudioBuffer<float> buffer ((int) reader->numChannels, n);
        reader->read (&buffer, 0, n, 0, true, true);

        std::vector<float> mono ((size_t) n, 0.0f);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < n; ++i)
                mono[(size_t) i] += buffer.getSample (ch, i) / (float) buffer.getNumChannels();

        return mono;
    }

    // ------------------------------------------------------------------
    // Correction trajectory (the Hard Tune mode's detector configuration)
    // ------------------------------------------------------------------

    float quantiseToChromatic (float hz)
    {
        if (hz <= 0.0f) return 0.0f;
        const double midi = std::round (69.0 + 12.0 * std::log2 ((double) hz / 440.0));
        return (float) (440.0 * std::pow (2.0, (midi - 69.0) / 12.0));
    }

    std::vector<Detection> computeTrajectory (const std::vector<float>& x, double sr,
                                             float rangeMin, float rangeMax,
                                             int windowSamples = 2048, int hopSamples = 512,
                                             int medianFrames = 3, bool offlineProbe = false)
    {
        const int window = juce::jmax (128, (int) std::lround ((double) windowSamples * sr / kRefRate));
        const int hop    = juce::jmax (32,  (int) std::lround ((double) hopSamples * sr / kRefRate));
        const int fifoSize = juce::jmax (4096, window * 2);
        const int decim = 4;
        const int decimWindow = juce::jmax (2, window / decim);

        ovtdsp::YinPitchDetector det;
        det.prepare (sr / (double) decim, 512);
        det.setFrequencyRange (rangeMin, rangeMax);
        det.setMedianFrames (medianFrames);

        // The register probe runs with the batch heuristics off: the live
        // anti-octave heuristic can transpose a clean held tone down an octave
        // when the octave-down lag is still inside the search range (see
        // DetectorResponseTest), and a wrong octave would set a 2x correction
        // ratio for the whole render.
        if (offlineProbe)
        {
            det.setOfflineMode (true);
            det.setMedianFrames (1);
        }

        std::vector<float> fifo ((size_t) fifoSize, 0.0f);
        std::vector<float> linear ((size_t) decimWindow, 0.0f);
        int writeIdx = 0, filled = 0, sinceHop = 0;

        std::vector<Detection> trajectory;
        trajectory.reserve (x.size() / (size_t) hop + 4);

        for (size_t n = 0; n < x.size(); ++n)
        {
            fifo[(size_t) writeIdx] = x[n];
            writeIdx = (writeIdx + 1) % fifoSize;
            filled = juce::jmin (filled + 1, fifoSize);
            ++sinceHop;

            if (filled < window || sinceHop < hop)
                continue;
            sinceHop = 0;

            int idx = (writeIdx - window + fifoSize) % fifoSize;
            for (int d = 0; d < decimWindow; ++d)
            {
                float sum = 0.0f;
                for (int k = 0; k < decim; ++k)
                {
                    sum += fifo[(size_t) idx];
                    idx = (idx + 1) % fifoSize;
                }
                linear[(size_t) d] = sum / (float) decim;
            }

            const float f = det.detectPitch (linear.data(), decimWindow);
            if (f <= 0.0f)
                continue;

            const float target = quantiseToChromatic (f);
            Detection d;
            d.sample = (long long) n;
            d.f0 = f;
            d.ratio = (target > 0.0f) ? juce::jlimit (0.25f, 4.0f, target / f) : 1.0f;
            trajectory.push_back (d);
        }
        return trajectory;
    }

    /** Median of the detected pitches, used to choose the detector range. */
    float medianPitch (const std::vector<Detection>& trajectory)
    {
        if (trajectory.empty()) return 0.0f;
        std::vector<float> f;
        f.reserve (trajectory.size());
        for (auto& d : trajectory) f.push_back (d.f0);
        std::sort (f.begin(), f.end());
        return f[f.size() / 2];
    }

    // ------------------------------------------------------------------
    // Shifter variants
    // ------------------------------------------------------------------

    struct Variant
    {
        std::string label;
        bool doppler = false;
        double latencyMs = 10.0;
        double formantRatio = 1.0;
        std::vector<float> out;
    };

    void renderVariant (Variant& v, const std::vector<float>& in,
                        const std::vector<Detection>& trajectory, double sr)
    {
        std::vector<float> out (in.size(), 0.0f);
        size_t detIdx = 0;
        float currentF0 = 0.0f;
        float currentRatio = 1.0f;

        juce::AudioBuffer<float> inBuf (1, kBlock), outBuf (1, kBlock);
        std::unique_ptr<ovtdsp::PitchShifter> ola;
        ovtprobe::DopplerShifter doppler;

        if (v.doppler)
        {
            doppler.prepare (sr);
            doppler.setLatencyMs (v.latencyMs);
        }
        else
        {
            ola = std::make_unique<ovtdsp::PitchShifter>();
            ola->prepare (sr, kBlock);
            ola->setAttackTimeMs (30.0f);
            ola->setLatencyMs ((float) v.latencyMs);
            ola->setRatioJumpEnvelopeEnabled (false); // Hard Tune: no pre-break duck
        }

        for (size_t pos = 0; pos < in.size(); pos += kBlock)
        {
            const int n = (int) std::min<size_t> ((size_t) kBlock, in.size() - pos);

            // Zero-order hold on the latest detection (Speed 0, no retarget smoothing).
            while (detIdx < trajectory.size() && trajectory[detIdx].sample <= (long long) pos)
            {
                currentF0 = trajectory[detIdx].f0;
                currentRatio = trajectory[detIdx].ratio;
                ++detIdx;
            }

            if (v.doppler)
            {
                doppler.process (in.data() + pos, out.data() + pos, n, currentRatio);
            }
            else
            {
                inBuf.setSize (1, kBlock, false, true, false);
                outBuf.setSize (1, kBlock, false, true, false);
                inBuf.clear();
                outBuf.clear();
                inBuf.copyFrom (0, 0, in.data() + pos, n);
                ola->process (inBuf, outBuf, currentRatio, (float) v.formantRatio, currentF0);
                std::memcpy (out.data() + pos, outBuf.getReadPointer (0), (size_t) n * sizeof (float));
            }
        }

        v.out = std::move (out);
    }

    // ------------------------------------------------------------------
    // Output
    // ------------------------------------------------------------------

    double rmsOf (const std::vector<float>& v, size_t from = 0)
    {
        double sum = 0.0;
        for (size_t i = from; i < v.size(); ++i) sum += (double) v[i] * v[i];
        return std::sqrt (sum / (double) juce::jmax<size_t> (1, v.size() - from));
    }

    bool writeWav (const juce::File& file, const std::vector<float>& samples, double sr)
    {
        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
        if (stream == nullptr) return false;

        juce::WavAudioFormat wav;
        auto options = juce::AudioFormatWriterOptions{}
                           .withSampleRate (sr)
                           .withNumChannels (1)
                           .withBitsPerSample (24)
                           .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::integral);
        auto writer = wav.createWriterFor (stream, options);
        if (writer == nullptr) return false;

        juce::AudioBuffer<float> buffer (1, (int) samples.size());
        buffer.copyFrom (0, 0, samples.data(), (int) samples.size());
        const bool ok = writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
        writer.reset();
        return ok;
    }
}

int main (int argc, char* argv[])
{
    std::printf ("OpenVoxTuner — Hard Tune shifter A/B renderer\n");

    // CLI: [--fixed <semitones>] [--fast-detector] [input.wav] [output-dir]
    int argIndex = 1;
    bool fixedMode = false, fastDetector = false;
    double fixedSemitones = 0.0;
    if (argc > 2 && std::strcmp (argv[1], "--fixed") == 0)
    {
        fixedMode = true;
        fixedSemitones = std::atof (argv[2]);
        argIndex = 3;
    }
    if (argc > argIndex && std::strcmp (argv[argIndex], "--fast-detector") == 0)
    {
        fastDetector = true;
        ++argIndex;
    }

    // Option 1 (Hard Tune vibrato residual): a shorter analysis window, a shorter
    // hop and no median. 1536/256/1 is the shortest config that still resolves the
    // 82.41 Hz bass floor (a 1024 window has no valid YIN minimum there).
    const int analysisWindow = fastDetector ? 1536 : 2048;
    const int analysisHop    = fastDetector ? 256 : 512;
    const int analysisMedian = fastDetector ? 1 : 3;

    const juce::File outputDir = (argc > argIndex + 1)
        ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[argIndex + 1])
        : juce::File::getCurrentWorkingDirectory().getChildFile (
              fixedMode ? "build/ab_hard_tune_fixed"
                        : (fastDetector ? "build/ab_hard_tune_short" : "build/ab_hard_tune"));
    outputDir.createDirectory();

    std::vector<float> input;
    double sr = kRefRate;

    if (argc > argIndex)
    {
        const juce::File inFile = juce::File::getCurrentWorkingDirectory().getChildFile (argv[argIndex]);
        input = readWav (inFile, sr);
        if (input.empty())
            return 1;
        std::printf ("input: %s (%.2f s, %.0f Hz, mono downmix)\n",
                     inFile.getFileName().toRawUTF8(), (double) input.size() / sr, sr);
    }
    else
    {
        input = makePhrase (sr);
        std::printf ("input: synthetic voice-like phrase, deliberately detuned per note (%.2f s, %.0f Hz)\n",
                     (double) input.size() / sr, sr);
    }

    // Register probe runs with the batch heuristics off: the live anti-octave
    // heuristic can transpose a clean held tone down an octave when the octave-down
    // lag is still inside the search range (see DetectorResponseTest), and a wrong
    // octave would set a 2x correction ratio for the whole render.
    const auto probe = computeTrajectory (input, sr, 30.0f, 1000.0f, 2048, 512, 3, true);
    const float median = medianPitch (probe);

    std::vector<Detection> trajectory;
    if (fixedMode)
    {
        // Constant-ratio shifter stress test: no detector, no quantiser. This is
        // what exposes the shifter's own character (combing, flanging) whatever the
        // take's intonation is.
        Detection d;
        d.sample = 0;
        d.f0 = (median > 0.0f) ? median : 220.0f;
        d.ratio = (float) std::pow (2.0, fixedSemitones / 12.0);
        trajectory.push_back (d);
        std::printf ("fixed transposition %+.2f semitones (no detection, shifter stress test)\n",
                     fixedSemitones);
    }
    else
    {
        float rangeMin = 60.0f, rangeMax = 1200.0f;
        if (median > 0.0f)
        {
            rangeMin = juce::jmax (40.0f, median * 0.6f);
            rangeMax = juce::jmin (2000.0f, median * 1.8f);
        }
        trajectory = computeTrajectory (input, sr, rangeMin, rangeMax, analysisWindow, analysisHop, analysisMedian);
        std::printf ("detector: %d/%d/median %d%s; detected median %.1f Hz; range %.0f..%.0f Hz; %d detections\n",
                     analysisWindow, analysisHop, analysisMedian,
                     fastDetector ? " (fast-detector option 1a)" : " (shipped hard tune)",
                     median, rangeMin, rangeMax, (int) trajectory.size());

        if (trajectory.empty())
        {
            std::printf ("ERROR: no voiced pitch detected; nothing to correct.\n");
            return 1;
        }

        double corrected = 0.0, sumCents = 0.0;
        for (auto& d : trajectory)
        {
            const double cents = 1200.0 * std::log2 ((double) d.ratio);
            sumCents += std::abs (cents);
            if (std::abs (cents) > 5.0) ++corrected;
        }
        std::printf ("correction: mean |cents| %.1f, %.0f %% of frames moved > 5 cents (chromatic snap)\n",
                     sumCents / (double) trajectory.size(), 100.0 * corrected / (double) trajectory.size());
    }

    const std::string prefix = fixedMode
        ? ("fixed " + juce::String (fixedSemitones, 2).toStdString() + " st: ")
        : std::string();
    std::vector<Variant> variants;
    variants.push_back ({ prefix + "OLA 10 ms, Formant +2 st (shipped preset colour)", false, 10.0, std::pow (2.0, 2.0 / 12.0), {} });
    variants.push_back ({ prefix + "OLA 10 ms, Formant 0", false, 10.0, 1.0, {} });
    variants.push_back ({ prefix + "Doppler 2.5 ms", true, 2.5, 1.0, {} });
    variants.push_back ({ prefix + "Doppler 4.0 ms", true, 4.0, 1.0, {} });

    const double dryRms = rmsOf (input, (size_t) (0.1 * sr));
    for (auto& v : variants)
    {
        renderVariant (v, input, trajectory, sr);

        // Level-match to the dry take so the comparison is about tone, not level.
        const double outRms = rmsOf (v.out, (size_t) (0.1 * sr));
        double gain = (outRms > 1.0e-9) ? dryRms / outRms : 1.0;
        float peak = 0.0f;
        for (auto s : v.out) peak = juce::jmax (peak, std::abs (s));
        if (peak * (float) gain > 0.89f) gain = 0.89 / (double) peak;
        for (auto& s : v.out) s = (float) (s * gain);

        std::printf ("  rendered %-52s (gain %+5.1f dB)\n",
                     v.label.c_str(), 20.0 * std::log10 (juce::jmax (1.0e-9, gain)));
    }

    // Anonymised filenames: listen first, then read key.txt.
    std::vector<int> order { 0, 1, 2, 3 };
    std::shuffle (order.begin(), order.end(), std::mt19937 (20261023));

    writeWav (outputDir.getChildFile ("dry.wav"), input, sr);
    for (int i = 0; i < (int) order.size(); ++i)
    {
        const juce::String name = "take_" + juce::String (i + 1).paddedLeft ('0', 2) + ".wav";
        writeWav (outputDir.getChildFile (name), variants[(size_t) order[(size_t) i]].out, sr);
    }

    juce::String key;
    for (int i = 0; i < (int) order.size(); ++i)
        key += "take_" + juce::String (i + 1).paddedLeft ('0', 2) + ".wav = "
             + variants[(size_t) order[(size_t) i]].label.c_str() + "\n";
    outputDir.getChildFile ("key.txt").replaceWithText (key);

    std::printf ("\nwrote %s\n", outputDir.getFullPathName().toRawUTF8());
    std::printf ("listen to dry.wav then take_01..take_04 BEFORE reading key.txt.\n");
    std::printf ("for a real take: render_hard_tune_ab <vocal.wav> [output-dir]\n");
    std::printf ("shifter stress test: render_hard_tune_ab --fixed 2 [input.wav] [output-dir]\n");
    return 0;
}
