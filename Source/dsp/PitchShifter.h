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
//   - Attack envelope on voice onset to prevent clicks

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "IPitchShifter.h"
#include <atomic>

namespace ovtdsp
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
        // Reset l'etat interne SANS re-armer la fade-in de demarrage. A
        // utiliser lors d'un seek/preset/reglage en cours de session, afin
        // d'eviter une nouvelle fade-in sur un signal deja a pleine amplitude
        // (qui genererait un pop). prepare() appelle reset() complet.
        void resetSoft();
        void process (juce::AudioBuffer<float>& buffer, float pitchRatio, float formantRatio, float f0);
        void process (const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output, float pitchRatio, float formantRatio, float f0);
        void setLatencyMs (float newLatencyMs);
        // Force-create a test grain (for debug): will create a single active grain
        void forceCreateTestGrain();
        int getLatencySamples() const { return latencySamples; }

        // Set attack time for voice onset (default 30ms). 0 = no attack envelope.
        void setAttackTimeMs (float ms);

    private:
        double sampleRate = 44100.0;
        int latencySamples = 0;
        float latencyMs = 20.0f;

        // Somme COLA du KBD (beta=6) a 50% d'overlap. Le gain de grain est
        // calcule pour que la somme des fenetres vaille 2.0 (cas Hann) ;
        // comme le KBD a une somme COLA differente, on la mesure une fois
        // dans prepare() et on en tient compte pour eviter sur-gain/clip.
        // Sentinel: -1.0 = pas encore calcule (calcule au premier prepare()).
        double kbdColaSum = -1.0;
        
        static constexpr int bufferSize = 65536; 
        static constexpr int bufferMask = bufferSize - 1;
        juce::AudioBuffer<float> ringBuffer;
        
        uint64_t absoluteWritePos = 0;
        // Nombre total d'echantillons ecrits dans le ring depuis le dernier
        // reset. Sert a savoir si l'historique disponible couvre la latence
        // demandee (evite de lire des zeros avant le demarrage du signal).
        int64_t totalWritten = 0;
        
        float currentRatio = 1.0f;
        float currentFormantRatio = 1.0f;

        // Pitch d'entree lisse (one-pole, ~5 ms) utilise pour calculer
        // targetF0 / Tin / Tout. Sans cela, un saut brutal de f0 (nouvelle
        // note ou attaque) change la periode des grains d'un coup ->
        // discontinuite dans l'OLA -> clic. Le lissage adoucit la
        // transition de periode sans colorer le timbre (le ratio de
        // correction est deja lisse par RetargetEnvelope en aval).
        float smoothedF0 = 0.0f;
        static constexpr float kF0SmoothAlpha = 0.002f;
        
        double outPhase = 0.0;
        double lastGrainCenter = 0.0;
        
        // Attack envelope state
        float attackMs = 30.0f;
        double attackAlpha = 0.0;
        float attackGain = 0.0f;
        bool wasVoiced = false;
        float lastF0 = 0.0f; // For detecting sudden pitch jumps (note attacks)

        // Hysteresis + debounce pour la detection d'onset (evite les clics
        // repetes quand le pitch frémit autour du seuil voiced/unvoiced au
        // demarrage d'une note). Seuil montee/descente differents et N
        // echantillons consécutifs requis avant de valider un changement.
        static constexpr float kVoiceOnThreshold  = 45.0f;  // montee
        static constexpr float kVoiceOffThreshold = 35.0f;  // descente
        static constexpr int   kVoiceDebounceSamples = 256; // ~6 ms @44.1k
        bool   hystVoiced = false;            // etat voiced filtre
        int    voiceDebounceCounter = 0;      // compteur d'echantillons stables

        // Startup fade-in: ring buffer starts empty (zeros), so first N samples
        // are garbage. Fade in over first ~20ms to avoid click on plugin start.
        int startupSamplesRemaining = 0;
        float startupGain = 0.0f;
        double startupAlpha = 0.0;

        // Vrai uniquement apres le 1er prepare() (demarrage du plugin). Permet
        // de distinguer un reset de session (seek/preset) d'un vrai demarrage.
        bool firstPrepareDone = false;

        // Compteur d'attaque lente apres un saut de pitch : pendant ces N
        // echantillons, l'enveloppe attackGain utilise un alpha plus lent
        // (80 ms) au lieu de l'alpha normal (attackMs, defaut 30 ms). Cela
        // garde attackGain < 0.14 a 12 ms du saut, masquant le step entre
        // l'output pre-saut (attackGain=1) et post-saut (attackGain=0).
        int    slowAttackSamplesRemaining = 0;

        // Compteur de ramp-down doux apres un saut de pitch : pendant ces N
        // echantillons, le smoother attackGain a target=0 au lieu de 1, ce
        // qui le fait converger exponentiellement de 1.0 vers ~0.86 au lieu
        // d'etre reset a 0 instantanement (evite le click de step).
        int    attackRampDownSamplesRemaining = 0;

        struct Grain {
            double readPos = 0.0;
            double speed = 1.0;
            double phase = 0.0;
            double phaseInc = 0.0;
            double gain = 1.0;
            bool active = false;
            // Per-grain attack: fade in over first N samples to avoid clicks
            // when reading from ring buffer positions that may have discontinuities
            float attackGain = 0.0f;
            double attackAlpha = 0.0;
        };
        static constexpr int MAX_GRAINS = 32;
        Grain grains[MAX_GRAINS];
        
        double findBestOffset (double idealReadPos, double targetToMatch, double searchWindowMs, float f0, double maxOffset) const;
        float getInterpolatedSample(int channel, double readPos) const;
    };
}

extern std::atomic<int> gPitchShifterGrainEvents;