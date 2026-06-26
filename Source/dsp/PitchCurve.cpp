// PitchCurve.cpp
// Implementation de la pitch curve.

#include "PitchCurve.h"
#include "ScaleQuantizer.h" // pour l'enum atdsp::Scale (utilise par snapToScale)
#include <cmath>

namespace atdsp
{
    PitchCurve::PitchCurve() = default;

    int PitchCurve::addOrUpdatePoint (double time, float pitch)
    {
        // Cherche si un point existe deja au meme 'time' (a 1 ms pres).
        for (int i = 0; i < points.size(); ++i)
        {
            if (std::abs (points[i].time - time) < 0.001)
            {
                // Fix : on passe par une reference pour eviter un probleme
                // d'evaluation de l-value avec juce::Array::operator[] dans
                // certains contextes (defensive).
                auto& pt = points.getReference (i);
                pt.pitch = pitch;
                return i;
            }
        }

        // Sinon, ajoute un nouveau point.
        PitchPoint p;
        p.time = time;
        p.pitch = pitch;
        points.add (p);
        sortPoints();
        return points.indexOf (p);
    }

    bool PitchCurve::removePointNear (double time, double toleranceSec)
    {
        int bestIdx = -1;
        double bestDist = toleranceSec;
        for (int i = 0; i < points.size(); ++i)
        {
            const double d = std::abs (points[i].time - time);
            if (d < bestDist)
            {
                bestDist = d;
                bestIdx = i;
            }
        }
        if (bestIdx >= 0)
        {
            points.remove (bestIdx);
            return true;
        }
        return false;
    }

    void PitchCurve::setMultiplePointsTimeAndPitch (juce::Array<int>& indices,
                                                    const juce::Array<double>& newTimes,
                                                    const juce::Array<float>& newPitches)
    {
        const int n = indices.size();
        if (n == 0) return;
        if (newTimes.size() != n || newPitches.size() != n) return;

        juce::Array<PitchPoint> moved;
        moved.ensureStorageAllocated (n);

        for (int i = 0; i < n; ++i)
        {
            const int idx = indices.getUnchecked (i);
            if (idx < 0 || idx >= points.size())
            {
                moved.add ({});
                continue;
            }

            auto& pt = points.getReference (idx);
            pt.time = newTimes.getUnchecked (i);
            pt.pitch = newPitches.getUnchecked (i);
            moved.add (pt);
        }

        sortPoints();

        juce::Array<bool> used;
        used.insertMultiple (0, false, points.size());

        for (int i = 0; i < n; ++i)
        {
            const auto& target = moved.getUnchecked (i);
            int found = -1;
            for (int j = 0; j < points.size(); ++j)
            {
                if (used.getUnchecked (j)) continue;
                if (points.getUnchecked (j) == target)
                {
                    found = j;
                    break;
                }
            }
            if (found >= 0)
            {
                used.set (found, true);
                indices.set (i, found);
            }
        }
    }

    void PitchCurve::sortPoints()
    {
        // Tri par insertion (la liste reste petite : qq dizaines de points max).
        for (int i = 1; i < points.size(); ++i)
        {
            const PitchPoint key = points[i];
            int j = i - 1;
            while (j >= 0 && points[j].time > key.time)
            {
                points.set (j + 1, points[j]);
                --j;
            }
            points.set (j + 1, key);
        }
    }

    float PitchCurve::getPitchAt (double time, float defaultValue) const
    {
        const int N = points.size();
        if (N == 0) return defaultValue;
        if (N == 1) return points[0].pitch;

        // Avant le premier point : on tient la valeur du premier.
        if (time <= points[0].time) return points[0].pitch;
        // Apres le dernier point : on tient la valeur du dernier.
        if (time >= points[N - 1].time) return points[N - 1].pitch;

        // Recherche dichotomique du segment [points[i], points[i+1]] contenant time.
        int lo = 0, hi = N - 1;
        while (hi - lo > 1)
        {
            const int mid = (lo + hi) / 2;
            if (points[mid].time <= time) lo = mid;
            else hi = mid;
        }

        // Interpolation lineaire entre points[lo] et points[hi] (ou palier si stepMode).
        const double t0 = points[lo].time;
        const double t1 = points[hi].time;
        const double frac = stepMode ? 0.0 : ((t1 > t0) ? (time - t0) / (t1 - t0) : 0.0);
        return static_cast<float> (points[lo].pitch + frac * (points[hi].pitch - points[lo].pitch));
    }

    float PitchCurve::snapToScale (float hz, int keyInSemitones, Scale scale)
    {
        // Reutilise la logique de ScaleQuantizer (distance circulaire).
        if (hz <= 0.0f) return 0.0f;

        const int key = ((keyInSemitones % 12) + 12) % 12;
        const int midiRef = 69;
        const int currentMidi = static_cast<int> (std::round (12.0f * std::log2 (hz / 440.0f))) + midiRef;
        const int noteInOctave = ((currentMidi % 12) + 12) % 12;

        // Intervalles par mode.
        static const juce::Array<int> intervalsTable[] = {
            { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },  // Chromatic
            { 0, 2, 4, 5, 7, 9, 11 },                  // Major
            { 0, 2, 3, 5, 7, 9, 11 },                  // Melodic Minor
            { 0, 2, 3, 5, 7, 8, 11 },                  // Harmonic Minor
            { 0, 2, 3, 5, 7, 8, 10 },                  // Natural Minor
            { 0, 2, 4, 7, 9 },                         // Major Pentatonic
            { 0, 3, 5, 7, 10 },                        // Minor Pentatonic
            { 0, 3, 5, 6, 7, 10 },                     // Blues
            { 0, 2, 3, 5, 7, 9, 10 },                  // Dorian
            { 0, 1, 3, 5, 7, 8, 10 },                  // Phrygian
            { 0, 2, 4, 6, 7, 9, 11 },                  // Lydian
            { 0, 2, 4, 5, 7, 9, 10 },                  // Mixolydian
            { 0, 1, 3, 5, 6, 8, 10 },                  // Locrian
            { 0, 4, 7 },                               // Major Triad
            { 0, 3, 7 }                                // Minor Triad
        };
        const int scaleIdx = static_cast<int> (scale);
        if (scaleIdx < 0 || scaleIdx >= 15) return hz;
        const auto& intervals = intervalsTable[scaleIdx];

        // Si deja dans la gamme, on garde.
        juce::Array<int> shifted;
        for (int i : intervals) shifted.add ((i + key) % 12);
        if (shifted.contains (noteInOctave)) return hz;

        // Recherche de la note la plus proche.
        auto circularDist = [] (int a, int b) -> int
        {
            int d = b - a;
            d = ((d + 6) % 12) - 6;
            if (d < -6) d += 12;
            return d;
        };

        int bestShift = 0;
        int bestDist = 100;
        for (int s : shifted)
        {
            int sh = circularDist (noteInOctave, s);
            int d = std::abs (sh);
            if (d < bestDist) { bestDist = d; bestShift = sh; if (d == 0) break; }
        }

        const int correctedMidi = currentMidi + bestShift;
        return 440.0f * std::pow (2.0f, (correctedMidi - midiRef) / 12.0f);
    }

    float PitchCurve::snapToScaleCustom (float hz, const juce::Array<int>& customIntervals)
    {
        if (hz <= 0.0f) return 0.0f;
        if (customIntervals.isEmpty()) return hz;

        const int midiRef = 69;
        const int currentMidi = static_cast<int> (std::round (12.0f * std::log2 (hz / 440.0f))) + midiRef;
        const int noteInOctave = ((currentMidi % 12) + 12) % 12;

        // Normalise les intervalles dans [0, 11].
        juce::Array<int> shifted;
        for (int i : customIntervals) shifted.add (((i % 12) + 12) % 12);
        if (shifted.contains (noteInOctave)) return hz;

        auto circularDist = [] (int a, int b) -> int
        {
            int d = b - a;
            d = ((d + 6) % 12) - 6;
            if (d < -6) d += 12;
            return d;
        };

        int bestShift = 0;
        int bestDist = 100;
        for (int s : shifted)
        {
            int sh = circularDist (noteInOctave, s);
            int d = std::abs (sh);
            if (d < bestDist) { bestDist = d; bestShift = sh; if (d == 0) break; }
        }

        const int correctedMidi = currentMidi + bestShift;
        return 440.0f * std::pow (2.0f, (correctedMidi - midiRef) / 12.0f);
    }

    std::unique_ptr<juce::XmlElement> PitchCurve::toXml() const
    {
        auto xml = std::make_unique<juce::XmlElement> ("PITCH_CURVE");
        xml->setAttribute ("numPoints", points.size());
        xml->setAttribute ("stepMode", stepMode);
        xml->setAttribute ("snapEnabled", snapEnabled);
        xml->setAttribute ("snapToGridEnabled", snapToGridEnabled);
        for (int i = 0; i < points.size(); ++i)
        {
            auto* p = xml->createNewChildElement ("POINT");
            p->setAttribute ("t", points[i].time);
            p->setAttribute ("p", points[i].pitch);
        }
        return xml;
    }

    void PitchCurve::fromXml (const juce::XmlElement& xml)
    {
        points.clear();
        if (!xml.hasTagName ("PITCH_CURVE")) return;
        stepMode = xml.getBoolAttribute ("stepMode", false);
        snapEnabled = xml.getBoolAttribute ("snapEnabled", true);
        snapToGridEnabled = xml.getBoolAttribute ("snapToGridEnabled", true);
        for (auto* p : xml.getChildWithTagNameIterator ("POINT"))
        {
            PitchPoint pt;
            pt.time = p->getDoubleAttribute ("t", 0.0);
            pt.pitch = static_cast<float> (p->getDoubleAttribute ("p", 440.0));
            points.add (pt);
        }
        sortPoints();
    }

    void PitchCurve::loadPreset (const juce::String& presetName)
    {
        points.clear();

        // Reset editing state to defaults
        stepMode = false;
        snapEnabled = true;
        snapToGridEnabled = true;

        // Helpers
        auto makeFlat = [&](float hz) {
            points.add ({ 0.0,  hz });
            points.add ({ 4.0,  hz });
        };
        
        auto makeSpoken = [&](float baseHz, float variation) {
            for (int i = 0; i < 8; ++i) {
                const double t = i * 0.5;
                const float p = baseHz + variation * std::sin (static_cast<float> (i) * 0.7f);
                points.add ({ t, p });
            }
        };

        auto makeMelody = [&](const float* pitches, int count) {
            for (int i = 0; i < count; ++i)
                points.add ({ i * 0.5, pitches[i] });
        };

        if (presetName == "default")
        {
            makeFlat(220.0f); // A3 par defaut
        }
        else if (presetName == "robot_c3")
        {
            makeFlat(130.81f); // C3
            stepMode = true; // flat robotique = step mode ok
        }
        else if (presetName == "robot_c4")
        {
            makeFlat(261.63f); // C4
            stepMode = true;
        }
        else if (presetName == "spoken_male")
        {
            makeSpoken(120.0f, 5.0f); // Autour de B2
        }
        else if (presetName == "spoken_female")
        {
            makeSpoken(220.0f, 10.0f); // Autour de A3
        }
        else if (presetName == "bass")
        {
            const float p[] = { 82.41f, 98.00f, 110.00f, 130.81f, 110.00f, 98.00f, 82.41f, 82.41f };
            makeMelody(p, 8);
            stepMode = true; // notes separees
        }
        else if (presetName == "baritone")
        {
            const float p[] = { 110.00f, 130.81f, 146.83f, 174.61f, 146.83f, 130.81f, 110.00f, 110.00f };
            makeMelody(p, 8);
            stepMode = true;
        }
        else if (presetName == "tenor")
        {
            const float p[] = { 130.81f, 146.83f, 164.81f, 196.00f, 164.81f, 146.83f, 130.81f, 130.81f };
            makeMelody(p, 8);
            stepMode = true;
        }
        else if (presetName == "alto")
        {
            const float p[] = { 174.61f, 196.00f, 220.00f, 261.63f, 220.00f, 196.00f, 174.61f, 174.61f };
            makeMelody(p, 8);
            stepMode = true;
        }
        else if (presetName == "mezzo")
        {
            const float p[] = { 220.00f, 246.94f, 261.63f, 329.63f, 261.63f, 246.94f, 220.00f, 220.00f };
            makeMelody(p, 8);
            stepMode = true;
        }
        else if (presetName == "soprano")
        {
            const float p[] = { 261.63f, 293.66f, 329.63f, 392.00f, 329.63f, 293.66f, 261.63f, 261.63f };
            makeMelody(p, 8);
            stepMode = true;
        }
    }
}
