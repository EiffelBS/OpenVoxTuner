// HarmonyEngine.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_core/juce_core.h>
#include "ScaleQuantizer.h"

namespace ovtdsp
{
    // Harmony type presets (interval of each voice relative to the base note).
    enum class HarmonyType
    {
        None              = 0,  // No harmony
        ThirdBelow        = 1,  // Third below (-2 scale degrees)
        ThirdAbove        = 2,  // Third above (+2 scale degrees)
        ThirdBelowAbove   = 3,  // Third below + third above (-2 / +2)
        FourthBelow       = 4,  // Fourth below (-3 scale degrees)
        FourthAbove       = 5,  // Fourth above (+3 scale degrees)
        FourthBelowAbove  = 6,  // Fourth below + fourth above (-3 / +3)
        FifthBelow        = 7,  // Fifth below (-4 scale degrees)
        FifthAbove        = 8,  // Fifth above (+4 scale degrees)
        FifthBelowAbove   = 9,  // Fifth below + fifth above (-4 / +4)
        ThirdBelowFifthAbove  = 10, // Third below + fifth above (-2 / +4)
        FifthBelowThirdAbove  = 11, // Fifth below + third above (-4 / +2)
        OctaveBelow       = 12, // Octave below
        OctaveAbove       = 13, // Octave above
        OctaveBelowAbove  = 14, // Octave below + octave above
        VocalStack3       = 15, // Third + fifth + octave (0 +3 / +7 / +12)
        VocalStack4       = 16, // 3rd below + 3rd above + 5th above + octave above
        PowerChord        = 17, // Fifth + octave (+7 +12)
        ParallelThird     = 18, // Parallel third (+1 degree)
        Drone             = 19, // Fixed note (tonic)
        Unison2           = 20, // Unison (2 identical voices)
        UnisonOctaves4    = 21, // Unison + octave below + octave above (4 voices)
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
         * @param gateActive When the Noise Gate module is enabled, the per-voice
         *        attack is forced to a short "gate-follow" time so the harmony
         *        voices swell together WITH the gated dry signal instead of
         *        arriving later and creating a perceived volume surplus at the
         *        note onset. When the gate is off, the configurable attackMs
         *        (set via setEnvelopeTimes) governs the per-voice fade-in.
         */
        void renderHarmonies (float inputFreq, const juce::Array<float>& harmonyFrequencies,
                              float volume, double sampleRate, juce::AudioBuffer<float>& outputBuffer,
                              int key, int scaleIndex, float blend, int toneMode = 0, float toneColor = 0.5f,
                              bool gateActive = false);

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
        float attackMs = 35.0f;             // attack time in ms (smoothstep fade-in ramp to avoid hard transient)
        float releaseMs = 80.0f;           // release time in ms (increased to reduce clicks)
        // Per-voice progressive (smoothstep) attack envelope state.
        // A smoothstep (raised-cosine) ramp has zero slope at both ends, so the
        // note-onset transient is much softer than a linear ramp or a one-pole.
        std::vector<int>   attackSamplesRemaining; // samples left in the current attack ramp
        std::vector<int>   attackTotalSamples;     // total length (samples) of the current attack ramp
        std::vector<float> attackStartAmp;         // amplitude captured when the current attack started
        std::vector<uint8_t> voicePrevGate;         // previous block's gate state per voice (for retrigger)
        // When the Noise Gate is enabled, the per-voice attack is clamped to this
        // short time so harmony voices track the gate envelope (no late swell).
        float gateFollowMs = 12.0f;
        // Slow phase accumulator per voice (used by the "Organ" tone to slowly
        // drift the formant across blocks, adding subtle movement to the carrier).
        std::vector<double> slowPhase;

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



