// HarmonyEngine.h
// Module d'harmonie vocale polyphonique.
// Calcule les frequences d'harmonie basnees sur la gamme courante.
//
// Algorithme :
//   1) Identifier la position (degre) de la note quantifiee dans la gamme
//   2) Avancer/arreter de X degres (intervals) dans la gamme pour chaque voix
//   3- Retourner les frequences correspondantes

#pragma once

#include <juce_core/juce_core.h>
#include "ScaleQuantizer.h"

namespace ovtdsp
{
    // Types d'harmonie presets (intervalle de chaque voix par rapport a la note de base).
    enum class HarmonyType
    {
        None              = 0,  // Pas d'harmonie
        ThirdBelow        = 1,  // Tierce en dessous (-2 degres)
        ThirdAbove        = 2,  // Tierce au-dessus (+2 degres)
        ThirdBelowAbove   = 3,  // Tierce en-dessous + tierce en-dessus (-2 / +2)
        FourthBelow       = 4,  // Quarte en dessous (-3 degres)
        FourthAbove       = 5,  // Quarte au-dessus (+3 degres)
        FourthBelowAbove  = 6,  // Quarte en-dessous + quarte en-dessus (-3 / +3)
        FifthBelow        = 7,  // Quinte en dessous (-4 degres)
        FifthAbove        = 8,  // Quinte au-dessus (+4 degres)
        FifthBelowAbove   = 9,  // Quinte en-dessous + quinte en-dessus (-4 / +4)
        ThirdBelowFifthAbove  = 10, // Tierce en-dessous + quinte au-dessus (-2 / +4)
        FifthBelowThirdAbove  = 11, // Quinte en-dessous + tierce au-dessus (-4 / +2)
        OctaveBelow       = 12, // Octave en dessous
        OctaveAbove       = 13, // Octave au-dessus
        OctaveBelowAbove  = 14, // Octave en-dessous + octave au-dessus
        VocalStack3       = 15, // Tierce + quinte + octave (0 +3 / +7 / +12)
        VocalStack4       = 16, // 3rd below + 3rd above + 5th above + octave above
        PowerChord        = 17, // Quinte + octave (+7 +12)
        ParallelThird     = 18, // Tierce parallele (+1 degre)
        Drone             = 19, // Note fixe (tonique)
        Unison2           = 20, // Unisson (2 voix identiques)
        UnisonOctaves4    = 21, // Unisson + octave dessous + octave dessus (4 voix)
    };

    /**
     * Engine d'harmonie vocale base sur la gamme active.
     * Prend une liste de note quantifiee et calcule les frequences d'harmonie.
     */
    class HarmonyEngine
    {
    public:
        HarmonyEngine() = default;

        // Prepare with host sample rate (stores it for oscillator increments)
        void prepare (double sampleRate);

        /**
         * Calcule les frequences d'harmonie pour une note quantifiee.
         * @param baseFreq Frequency de la note quantifiee en Hz
         * @param key      Index de la tonique (0=C, 1=C#, ..., 11=B)
         * @param scaleIntervals Liste des intervals de la gamme (0-11)
         * @param harmonyType Type d'harmonie desire
         * @returns Liste de frequences pour chaque voix activee
         */
        juce::Array<float> getHarmonyNotes (float baseFreq, const juce::Array<int>& scaleIntervals,
                                             HarmonyType harmonyType) const;

        /**
         * Calcule le degre (position dans la gamme) d'une frequence.
         */
        int getScaleDegree (float freq, int key, const juce::Array<int>& scaleIntervals) const;

        /**
         * Render les harmonies directement dans un buffer audio.
         * Implementation simplification pour le moment.
         */
        void renderHarmonies (float inputFreq, const juce::Array<float>& harmonyFrequencies, 
                              float volume, double sampleRate, juce::AudioBuffer<float>& outputBuffer,
                              int key, int scaleIndex, float blend, int toneMode = 0, float toneColor = 0.5f);

        /**
         * Retourne le nom lisible d'un preset d'harmonie.
         */
        static juce::String getHarmonyName (HarmonyType type)
        {
            switch (type)
            {
                case HarmonyType::None                   : return "None";
                case HarmonyType::ThirdBelow             : return "3rd Below";
                case HarmonyType::ThirdAbove             : return "3rd Above";
                case HarmonyType::ThirdBelowAbove        : return "3rd Below + Above";
                case HarmonyType::FourthBelow            : return "4th Below";
                case HarmonyType::FourthAbove            : return "4th Above";
                case HarmonyType::FourthBelowAbove       : return "4th Below + Above";
                case HarmonyType::FifthBelow             : return "5th Below";
                case HarmonyType::FifthAbove             : return "5th Above";
                case HarmonyType::FifthBelowAbove        : return "5th Below + Above";
                case HarmonyType::ThirdBelowFifthAbove   : return "3rd Below + 5th Above";
                case HarmonyType::FifthBelowThirdAbove   : return "5th Below + 3rd Above";
                case HarmonyType::OctaveBelow            : return "Octave Below";
                case HarmonyType::OctaveAbove            : return "Octave Above";
                case HarmonyType::OctaveBelowAbove       : return "Octave Below + Above";
                case HarmonyType::VocalStack3            : return "Vocal Stack (3 voices)";
                case HarmonyType::VocalStack4            : return "Vocal Stack (4 voices)";
                case HarmonyType::PowerChord             : return "Power Chord";
                case HarmonyType::ParallelThird          : return "Parallel 3rd";
                case HarmonyType::Drone                  : return "Drone";
                default: return "Unknown";
            }
        }

        // Returns the number of voices generated by a harmony type.
        static int getHarmonyVoiceCount (HarmonyType type)
        {
            switch (type)
            {
                case HarmonyType::None:                  return 0;
                case HarmonyType::ThirdBelow:            return 1;
                case HarmonyType::ThirdAbove:            return 1;
                case HarmonyType::ThirdBelowAbove:       return 2;
                case HarmonyType::FourthBelow:           return 1;
                case HarmonyType::FourthAbove:           return 1;
                case HarmonyType::FourthBelowAbove:      return 2;
                case HarmonyType::FifthBelow:            return 1;
                case HarmonyType::FifthAbove:            return 1;
                case HarmonyType::FifthBelowAbove:       return 2;
                case HarmonyType::ThirdBelowFifthAbove:  return 2;
                case HarmonyType::FifthBelowThirdAbove:  return 2;
                case HarmonyType::OctaveBelow:           return 1;
                case HarmonyType::OctaveAbove:           return 1;
                case HarmonyType::OctaveBelowAbove:      return 2;
                case HarmonyType::VocalStack3:           return 3;
                case HarmonyType::VocalStack4:           return 4;
                case HarmonyType::PowerChord:            return 2;
                case HarmonyType::ParallelThird:         return 1;
                case HarmonyType::Drone:                 return 1;
                case HarmonyType::Unison2:               return 2;
                case HarmonyType::UnisonOctaves4:        return 4;
                default:                                 return 0;
            }
        }

    private:
        /**
         * Calcule la frequence d'une note a partir d'un degre dans la gamme.
         * @param scaleDegree Degre en demi-tons par rapport a la tonique
         * @param baseFreq Frequency de la note quantifiee en Hz
         * @param numOctaves Nombre d'octaves a deplacer (+/-)
         */
        float degreeToFreq (int scaleDegree, float baseFreq, int numOctaves) const;

        // Runtime state for synthesis
        std::vector<double> phases;        // persistent phase per voice
        std::vector<float> amplitudes;     // current amplitude per voice (for smoothing)
        std::vector<float> targetAmps;     // target amplitude per voice
        double currentSampleRate = 44100.0; // stored sample rate
        bool voiceGate = true;             // whether voices are allowed to sound
        float attackMs = 20.0f;             // attack time in ms (increased from 5ms to prevent clicks on staccato)
        float releaseMs = 80.0f;           // release time in ms (increased to reduce clicks)

    public:
        // Control voice gating (note on/off). When turned on, volume is used as
        // masterVoiceVolume and voices will ramp up; when turned off, voices
        // will ramp down according to releaseMs to avoid clicks.
        void setVoiceGate (bool on);
        void setEnvelopeTimes (float attackMilliseconds, float releaseMilliseconds)
        {
            attackMs = attackMilliseconds;
            releaseMs = releaseMilliseconds;
        }
        // Returns true if any voice amplitude is above a tiny threshold (engine active / releasing)
        bool isActive() const;
    };
}
