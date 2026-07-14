// ScaleQuantizer.cpp
// Implementation reelle du quantificateur.
// Prend une frequence d'entree, la convertit en demi-tons (relatifs a A4),
// trouve le demi-ton le plus proche appartenant a la gamme, puis retourne la
// frequence correspondante.

#include "ScaleQuantizer.h"
#include <vector>

namespace ovtdsp
{
    // Demi-tons d'une octave pour chaque mode (relatifs a la tonique).
    static const std::vector<int> chromaticIntervals       = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    static const std::vector<int> majorIntervals           = { 0, 2, 4, 5, 7, 9, 11 };
    static const std::vector<int> melodicMinorIntervals    = { 0, 2, 3, 5, 7, 9, 11 };
    static const std::vector<int> harmonicMinorIntervals   = { 0, 2, 3, 5, 7, 8, 11 };
    static const std::vector<int> naturalMinorIntervals    = { 0, 2, 3, 5, 7, 8, 10 };
    static const std::vector<int> majorPentatonicIntervals = { 0, 2, 4, 7, 9 };
    static const std::vector<int> minorPentatonicIntervals = { 0, 3, 5, 7, 10 };
    static const std::vector<int> bluesIntervals           = { 0, 3, 5, 6, 7, 10 };
    static const std::vector<int> dorianIntervals          = { 0, 2, 3, 5, 7, 9, 10 };
    static const std::vector<int> phrygianIntervals        = { 0, 1, 3, 5, 7, 8, 10 };
    static const std::vector<int> lydianIntervals          = { 0, 2, 4, 6, 7, 9, 11 };
    static const std::vector<int> mixolydianIntervals      = { 0, 2, 4, 5, 7, 9, 10 };
    static const std::vector<int> locrianIntervals         = { 0, 1, 3, 5, 6, 8, 10 };
    static const std::vector<int> majorTriadIntervals      = { 0, 4, 7 };
    static const std::vector<int> minorTriadIntervals      = { 0, 3, 7 };

    ScaleQuantizer::ScaleQuantizer()
    {
        // Par defaut, on active toute la gamme chromatique comme custom.
        for (int i = 0; i < 12; ++i) customIntervals.add (i);
        rebuildIntervals();
    }

    void ScaleQuantizer::setKey (int keyInSemitones)
    {
        key = ((keyInSemitones % 12) + 12) % 12;
        rebuildIntervals();
    }

    void ScaleQuantizer::setScale (Scale s)
    {
        currentScale = s;
        rebuildIntervals();
    }

    void ScaleQuantizer::setCustomIntervals (const juce::Array<int>& notesInSemitones)
    {
        customIntervals.clear();
        for (int n : notesInSemitones)
        {
            const int m = ((n % 12) + 12) % 12;
            if (! customIntervals.contains (m))
                customIntervals.add (m);
        }
        // Si vide, on retombe sur chromatique pour eviter une gamme vide.
        if (customIntervals.isEmpty())
            for (int i = 0; i < 12; ++i) customIntervals.add (i);
        rebuildIntervals();
    }

    void ScaleQuantizer::rebuildIntervals()
    {
        intervals.clear();

        if (currentScale == Scale::Custom)
        {
            for (int n : customIntervals)
                intervals.add (((n % 12) + 12) % 12);
            return;
        }

        static const int chromatic[]       = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
        static const int major[]           = { 0, 2, 4, 5, 7, 9, 11 };
        static const int melodicMinor[]    = { 0, 2, 3, 5, 7, 9, 11 };
        static const int harmonicMinor[]   = { 0, 2, 3, 5, 7, 8, 11 };
        static const int naturalMinor[]    = { 0, 2, 3, 5, 7, 8, 10 };
        static const int majorPentatonic[] = { 0, 2, 4, 7, 9 };
        static const int minorPentatonic[] = { 0, 3, 5, 7, 10 };
        static const int blues[]           = { 0, 3, 5, 6, 7, 10 };
        static const int dorian[]          = { 0, 2, 3, 5, 7, 9, 10 };
        static const int phrygian[]        = { 0, 1, 3, 5, 7, 8, 10 };
        static const int lydian[]          = { 0, 2, 4, 6, 7, 9, 11 };
        static const int mixolydian[]      = { 0, 2, 4, 5, 7, 9, 10 };
        static const int locrian[]         = { 0, 1, 3, 5, 6, 8, 10 };

        const int* base = nullptr;
        int size = 0;

        switch (currentScale)
        {
            case Scale::Chromatic:       base = chromatic;       size = 12; break;
            case Scale::Major:           base = major;           size = 7; break;
            case Scale::MelodicMinor:    base = melodicMinor;    size = 7; break;
            case Scale::HarmonicMinor:   base = harmonicMinor;   size = 7; break;
            case Scale::NaturalMinor:    base = naturalMinor;    size = 7; break;
            case Scale::MajorPentatonic: base = majorPentatonic; size = 5; break;
            case Scale::MinorPentatonic: base = minorPentatonic; size = 5; break;
            case Scale::Blues:           base = blues;           size = 6; break;
            case Scale::Dorian:          base = dorian;          size = 7; break;
            case Scale::Phrygian:        base = phrygian;        size = 7; break;
            case Scale::Lydian:          base = lydian;          size = 7; break;
            case Scale::Mixolydian:      base = mixolydian;      size = 7; break;
            case Scale::Locrian:         base = locrian;         size = 7; break;
            case Scale::Custom:          return;
            default:
                // Safety net: fall back to Chromatic for unexpected enum values.
                base = chromatic; size = 12; break; 
        }

        if (base != nullptr)
        {
            for (int i = 0; i < size; ++i)
                intervals.add ((base[i] + key) % 12);
        }
    }

    float ScaleQuantizer::quantize (float hzIn) const
    {
        if (hzIn <= 0.0f) return 0.0f;

        // 1) Conversion Hz -> demi-tons par rapport a A4 (440 Hz).
        const float semitones = hzToSemitones (hzIn);

        // 2) Separer la partie entiere (octave) et fractionnaire (position dans l'octave).
        //    La gamme se repete a chaque octave, donc on ne quantifie que modulo 12.
        const float roundedSemi = std::round (semitones);
        const float frac = semitones - roundedSemi; // dans [-0.5, 0.5]

        // 3) Convertir la position dans l'octave en [0, 12).
        //    "roundedSemi" est la note MIDI en demi-tons par rapport a A4.
        //    A4 = 69 en MIDI. Demi-ton 0 = A, 1 = A#, ..., 12 = A suivant.
        const int midiRef = 69; // A4 en MIDI
        const int currentMidi = static_cast<int> (std::round (roundedSemi)) + midiRef;
        const int noteInOctave = ((currentMidi % 12) + 12) % 12; // [0,11]

        // 4) Trouver le demi-ton de la gamme le plus proche.
        //    Si la note exacte est dans la gamme, on la garde.
        //    Sinon, on prend la note de la gamme la plus proche (en cycles modulo 12).
        if (intervals.size() == 0)
            return hzIn;

        if (intervals.contains (noteInOctave))
        {
            // Deja dans la gamme, la frequence n'est pas modifiee SI le quantificateur
            // etait parfait (T-Pain effect absolu). Mais un Autotune doit *corriger* 
            // le pitch meme s'il est de la bonne note (ex: 445Hz -> 440Hz).
            const int correctedMidi = currentMidi;
            return 440.0f * std::pow (2.0f, (correctedMidi - midiRef) / 12.0f);
        }

        // Recherche de la note de la gamme la plus proche, en distance circulaire.
        // Distance circulaire entre deux notes modulo 12, dans [-6, 6].
        auto circularDist = [] (int a, int b) -> int
        {
            int d = b - a;
            d = ((d + 6) % 12) - 6; // ramene dans [-6, 5]
            if (d < -6) d += 12;
            return d;
        };

        int bestShift = 0;
        int bestDist = 100;
        for (int scaleNote : intervals)
        {
            int shift = circularDist (noteInOctave, scaleNote);
            int dist = std::abs (shift);
            if (dist < bestDist)
            {
                bestDist = dist;
                bestShift = shift;
                if (dist == 0) break;
            }
        }

        // 5) Recalculer la frequence corrigee.
        const int correctedMidi = currentMidi + bestShift;
        return 440.0f * std::pow (2.0f, (correctedMidi - midiRef) / 12.0f);

        // Note : la valeur "frac" est conservee pour une future extension
        // (quantification avec preservation du micro-tonal).
        juce::ignoreUnused (frac);
    }
}
