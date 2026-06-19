// HarmonyEngine.cpp
// Implémentation du moteur d'harmonie vocale.
// Calcule les fréquences d'harmonie basées sur la gamme courante.

#include "HarmonyEngine.h"

namespace atdsp
{
    int HarmonyEngine::getScaleDegree (float freq, int key, const juce::Array<int>& intervals) const
    {
        if (freq <= 0.0f || intervals.isEmpty())
            return 0;

        // Convertir la fréquence en demi-tons par rapport à la tonique
        float semis = hzToSemitones (freq);
        float keySemis = (float (key % 12) + 12.0f) % 12.0f;
        float freqKeySemis = ((semis - keySemis) + 12.0f) % 12.0f;
        
        // Trouver le degrc le plus prochain dans la gamme
        int bestDegree = 0;
        float bestDist = std::numeric_limits<float>::infinity();
        
        for (int i = 0; i < intervals.size(); ++i)
        {
            float dist = std::abs (float (intervals[i]) - freqKeySemis);
            if (dist < bestDist)
            {
                bestDist = dist;
                bestDegree = i;
            }
        }
        
        return bestDegree;
    }

    juce::Array<float> HarmonyEngine::getHarmonyNotes (float baseFreq, const ScaleQuantizer& quantizer,
                                                       HarmonyType harmonyType) const
    {
        juce::Array<float> harmonies;
        harmonies.reserve (6); // Maximum 6 voix
        
        if (baseFreq <= 0.0f || harmonyType == HarmonyType::None)
            return harmonies;
        
        const auto& intervals = quantizer.getScaleIntervals();
        int key = quantizer.getKey();
        int degree = getScaleDegree (baseFreq, key, intervals);
        int numIntervals = intervals.size();
        
        if (baseFreq > 0.0f)
            harmonies.add (baseFreq); // Voix principale
        
        switch (harmonyType)
        {
        case HarmonyType::ThirdBelow:
            // Tierce en dessous
            if (degree >= 1 && degree < numIntervals)
            {
                int idxB = degree >= 1 ? (degree - 1) : 0;
                float semiBelow = float (key) + float (intervals[idxB]);
                harmonies.add (semitonesToHz (semiBelow));
            }
            break;
            
        case HarmonyType::ThirdAbove:
            // Tierce au-dessus
            if (degree < numIntervals)
            {
                int idx = degree + 1 >= numIntervals ? numIntervals - 1 : degree + 1;
                float semiAbove = float (key) + float (intervals[idx]);
                harmonies.add (semitonesToHz (semiAbove));
            }
            break;
            
        case HarmonyType::ThirdBelowAbove:
            // Tierce en-dessous + tierce en-dessus
            if (degree >= 1 && degree < numIntervals)
            {
                float semiBelow = float (key) + float (intervals[degree - 1]);
                harmonies.add (semitonesToHz (semiBelow));
            }
            if (degree < numIntervals)
            {
                int idx = degree + 1 >= numIntervals ? numIntervals - 1 : degree + 1;
                float semiAbove = float (key) + float (intervals[idx]);
                harmonies.add (semitonesToHz (semiAbove));
            }
            break;
            
        case HarmonyType::FifthAbove:
            // Quinte au-dessus
            if (degree < numIntervals)
            {
                int idx = degree + 2 >= numIntervals ? numIntervals - 1 : degree + 2;
                float semiAbove = float (key) + float (intervals[idx]);
                harmonies.add (semitonesToHz (semiAbove));
            }
            break;
            
        case HarmonyType::FourthAbove:
            // Quarte au-dessus
            if (degree < numIntervals)
            {
                int idx = degree + 2 >= numIntervals ? numIntervals - 1 : degree + 2;
                float semiAbove = float (key) + float (intervals[idx]);
                harmonies.add (semitonesToHz (semiAbove));
            }
            break;

        case HarmonyType::PowerChord:
            // Quinte + octave
            if (degree < numIntervals)
            {
                int idx = degree + 3 >= numIntervals ? numIntervals - 1 : degree + 3;
                float semiAbove = float (key) + float (intervals[idx]);
                harmonies.add (semitonesToHz (semiAbove));
            }
            harmonies.add (baseFreq * 2.0f); // Octave au-dessus
            break;

        case HarmonyType::VocalStack:
            // Tierce + quinte + octave
            if (degree < numIntervals)
            {
                int idx3 = degree + 1 >= numIntervals ? numIntervals - 1 : degree + 1;
                float semiThird = float (key) + float (intervals[idx3]);
                harmonies.add (semitonesToHz (semiThird));

                int idx5 = degree + 2 >= numIntervals ? numIntervals - 1 : degree + 2;
                float semiFifth = float (key) + float (intervals[idx5]);
                harmonies.add (semitonesToHz (semiFifth));
            }
            harmonies.add (baseFreq * 2.0f); // Octave au-dessus
            break;

        case HarmonyType::ParallelThird:
            // Tierce parallele (toutes les notes avancent de +1 degrc)
            if (degree < numIntervals)
            {
                int idx = degree + 1 >= numIntervals ? numIntervals - 1 : degree + 1;
                float semiAbove = float (key) + float (intervals[idx]);
                harmonies.add (semitonesToHz (semiAbove));
            }
            break;
            
        case HarmonyType::Drone:
            // Note fixe (tonique de la gamme, degrc 0)
            harmonies.add (semitonesToHz (float (key)));
            harmonies.add (semitonesToHz (float (key)) * 2.0f); // Octave
            break;
            
        case HarmonyType::None:
        default:
            break;
        }
        
        return harmonies;
    }

    float HarmonyEngine::degreeToFreq (int scaleDegree, float baseFreq, int numOctaves) const
    {
        // Calculer la fréquence d'un degrc dans la gamme
        float semis = hzToSemitones (baseFreq) + float (scaleDegree) + (float (numOctaves) * 12.0f);
        return semitonesToHz (semis);
    }
}
