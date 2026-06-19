// PitchShifter.h
// Module de transposition de pitch par PSOLA (Phase 4).
//
// Algorithme :
//   1) Detection de la frequence fondamentale courante (via PitchDetector).
//   2) Detection des "pitch marks" : position des periodes fondamentales dans
//      le signal (typiquement les pics positifs du signal fenetre).
//   3) Analyse : on stocke les pitch marks et les grains Hann centres dessus.
//   4) Synthese : on replace les pitch marks a des positions correspondant a
//      la frequence cible (f_target = f_source * ratio), et on additionne
//      les grains en overlap-add.
//
// References :
//   - Moulines & Charpentier, "Pitch-synchronous waveform processing
//     techniques for text-to-speech synthesis using diphones",
//     Speech Communication, 1990.
//   - "DAFX" (Zolzer), chapter 6 : Pitch-shifting and time-stretching.
//
// Ameliorations par rapport a la v1 MVP :
//   - Vrai PSOLA : pas de flanger, preservation des transitoires.
//   - Compensation de formants optionnelle (preserve le timbre).
//   - Gestion douce des passages a/pitch nul.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "IPitchShifter.h"
#include <atomic>

namespace atdsp
{
    class PitchDetector; // forward decl

    /**
     * PitchShifter basé sur l'algorithme PSOLA (Pitch Synchronous Overlap-Add).
     * Ancienne implémentation maison de Phase 4.
     */
    class PitchShifter
    {
    public:
        PitchShifter();
        ~PitchShifter() = default;

        void prepare (double sampleRate, int maximumBlockSize);        
        void reset();
        void process (juce::AudioBuffer<float>& buffer, float pitchRatio, float formantRatio, float f0);
        void process (const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output, float pitchRatio, float formantRatio, float f0);
        void setLatencyMs (float newLatencyMs);
        // Force-create a test grain (for debug): will create a single active grain
        void forceCreateTestGrain();
        int getLatencySamples() const { return latencySamples; }

    private:
        double sampleRate = 44100.0;
        int latencySamples = 0;
        float latencyMs = 20.0f;
        
        static constexpr int bufferSize = 65536; 
        static constexpr int bufferMask = bufferSize - 1;
        juce::AudioBuffer<float> ringBuffer;
        
        uint64_t absoluteWritePos = 0;
        
        float currentRatio = 1.0f;
        float currentFormantRatio = 1.0f;
        
        double outPhase = 0.0;
        double lastGrainCenter = 0.0;
        
        struct Grain {
            double readPos = 0.0;
            double speed = 1.0;
            double phase = 0.0;
            double phaseInc = 0.0;
            double gain = 1.0;
            bool active = false;
        };
        static constexpr int MAX_GRAINS = 32;
        Grain grains[MAX_GRAINS];
        
        double findBestOffset (double idealReadPos, double targetToMatch, double searchWindowMs, float f0, double maxOffset) const;
        float getInterpolatedSample(int channel, double readPos) const;
    };
}

// Global debug counter incremented when a grain is created (used by host/logger)
extern std::atomic<int> gPitchShifterGrainEvents;
