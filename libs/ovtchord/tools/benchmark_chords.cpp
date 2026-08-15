// benchmark_chords.cpp
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Benchmark: measures the internal chroma + chord engine accuracy and CPU cost
// on a set of reference chords (generated as sums of sines).
//
// Optional Aubio comparison: compile with -DOVTCHORD_HAVE_AUBIO=1 and provide
// aubio headers/libs to add the aubio_chroma leg.

#include "audio/audio_processor.h"
#include "ovtchord/chord_engine.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

#if defined(OVTCHORD_HAVE_AUBIO)
#include <aubio/aubio.h>
#endif

namespace
{
    const double twoPi() { return 2.0 * std::acos (-1.0); }
    const double kA4 = 440.0;

    // Frequency for a pitch class (0=C..11=B) in a given octave (C0 = midi 12).
    double freqForPc (int pc, int octave)
    {
        const int midi = 12 * (octave + 1) + pc; // C0 = 12
        return kA4 * std::pow (2.0, (midi - 69) / 12.0);
    }

    // Sum of sines at the chord fundamentals (normalized).
    std::vector<float> genChord (const std::vector<int>& pitchClasses, double sr, int n)
    {
        std::vector<float> sig (static_cast<std::size_t> (n), 0.0f);
        for (int i = 0; i < n; ++i)
        {
            double s = 0.0;
            for (int pc : pitchClasses)
                s += std::sin (twoPi() * freqForPc (pc, 3) * i / sr); // octave 3
            sig[static_cast<std::size_t> (i)] = static_cast<float> (s / static_cast<double> (pitchClasses.size()));
        }
        return sig;
    }

    struct Ref
    {
        const char* name;
        const char* expected;
        std::vector<int> pcs;
    };
}

#if defined(OVTCHORD_HAVE_AUBIO)
namespace
{
    // Run the reference chords through aubio's chroma and report results.
    // Template for the future Aubio comparison leg (needs aubio installed).
    void runAubioLeg (double sr, int windowSize, const std::vector<Ref>& refs)
    {
        std::printf ("\n=== Aubio chroma ===\n");
        int correct = 0;
        for (const auto& ref : refs)
        {
            auto sig = genChord (ref.pcs, sr, windowSize * 3);

            aubio_t *o = new_aubio ("chroma", static_cast<uint_t> (windowSize),
                                    static_cast<uint_t> (windowSize / 2), static_cast<uint_t> (sr));
            if (o == nullptr) { std::printf ("aubio init failed\n"); return; }
            fvec_t* in  = new_fvec (static_cast<uint_t> (windowSize));
            fvec_t* out = new_fvec (12);

            float peak[12] = { 0 };
            for (int i = 0; i + windowSize <= static_cast<int> (sig.size()); i += windowSize / 2)
            {
                for (int k = 0; k < windowSize; ++k)
                    in->data[k] = sig[static_cast<std::size_t> (i + k)];
                aubio_chroma_do (o, in, out);
                for (int k = 0; k < 12; ++k)
                    if (out->data[k] > peak[k]) peak[k] = out->data[k];
            }
            // Derive the pitch-class set from the aubio chroma (threshold 0.35).
            std::vector<int> pcs;
            float mx = 0; for (int k = 0; k < 12; ++k) if (peak[k] > mx) mx = peak[k];
            for (int k = 0; k < 12; ++k) if (peak[k] >= mx * 0.35f) pcs.push_back (k);

            ovtchord::ChordEngine engine;
            auto r = engine.recognize (pcs, -1, 0.0);
            const bool ok = (r.valid && r.symbol == ref.expected);
            if (ok) ++correct;
            std::printf ("%-8s expected=%-8s aubio-pc={", ref.name, ref.expected);
            for (std::size_t j = 0; j < pcs.size(); ++j) std::printf ("%s%d", j ? "," : "", pcs[j]);
            std::printf ("} detected=%-8s %s\n", r.symbol.c_str(), ok ? "OK" : "FAIL");

            del_fvec (in); del_fvec (out); del_aubio (o);
        }
        std::printf ("aubio accuracy: %d/%d\n", correct, static_cast<int> (refs.size()));
    }
}
#endif

int main()
{
    const std::vector<Ref> refs = {
        { "C",     "C",     { 0, 4, 7 } },
        { "Cm",    "Cm",    { 0, 3, 7 } },
        { "C7",    "C7",    { 0, 4, 7, 10 } },
        { "Cmaj7", "Cmaj7", { 0, 4, 7, 11 } },
        { "Cm7",   "Cm7",   { 0, 3, 7, 10 } },
        { "Csus4", "Csus4", { 0, 5, 7 } },
        { "Csus2", "Csus2", { 0, 2, 7 } },
        { "C6",    "C6",    { 0, 4, 7, 9 } },
        { "Cdim",  "Cdim",  { 0, 3, 6 } },
        { "Caug",  "Caug",  { 0, 4, 8 } },
        { "C5",    "C5",    { 0, 7 } },
    };

    const double sr = 44100.0;
    const int windowSize = 8192;

    std::printf ("=== Internal chroma ===\n");
    int correct = 0;
    double totalMs = 0.0;
    for (const auto& ref : refs)
    {
        auto sig = genChord (ref.pcs, sr, windowSize * 3);

        ovtchord::AudioProcessor::Config cfg;
        cfg.windowSize = windowSize;
        cfg.hopSize = 1024;
        cfg.sampleRate = sr;
        ovtchord::AudioProcessor ap (cfg);
        ovtchord::ChordEngine::Config ecfg;
        ecfg.stabilityFrames = 1;
        ecfg.minNotes = 2; // allow 2-note power chords (C5) in the reference set
        ovtchord::ChordEngine engine (ecfg);
        ovtchord::ChordResult last;

        const auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < static_cast<int> (sig.size()); i += 1024)
            if (ap.process (sig.data() + i, 1024))
                last = engine.recognize (ap.lastPitchClasses(), -1, 0.0);
        const auto t1 = std::chrono::high_resolution_clock::now();
        totalMs += std::chrono::duration<double, std::milli> (t1 - t0).count();

        const auto& pcs = ap.lastPitchClasses();
        std::printf ("  pcs={");
        for (std::size_t j = 0; j < pcs.size(); ++j) std::printf ("%s%d", j ? "," : "", pcs[j]);
        std::printf ("}\n");

        const bool ok = (last.valid && last.symbol == ref.expected);
        if (ok) ++correct;
        std::printf ("%-8s expected=%-8s detected=%-8s %s\n",
                     ref.name, ref.expected, last.symbol.c_str(), ok ? "OK" : "FAIL");
    }
    std::printf ("\ninternal accuracy: %d/%d (%.1f%%)\n", correct, (int) refs.size(),
                 100.0 * static_cast<double> (correct) / static_cast<double> (refs.size()));
    std::printf ("internal avg CPU per chord (incl. gen): %.3f ms\n", totalMs / refs.size());

#if defined(OVTCHORD_HAVE_AUBIO)
    runAubioLeg (sr, windowSize, refs);
#else
    std::printf ("\n[Aubio leg disabled — compile with -DOVTCHORD_HAVE_AUBIO=1 + aubio deps]\n");
#endif

    return correct == static_cast<int> (refs.size()) ? 0 : 1;
}