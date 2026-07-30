// PitchShifter.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "IPitchShifter.h"
#include "BlockAwareOnePole.h"
#include <atomic>

namespace ovtdsp
{
    class PitchDetector; // forward decl

    /**
     * PitchShifter basÃ© sur l'algorithme PSOLA (Pitch Synchronous Overlap-Add).
     * Ancienne implÃ©mentation maison de Phase 4.
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

        // Enable/disable the internal attack envelope. When disabled, the
        // PitchShifter does NOT apply its own onset fade-in/down at note
        // onsets or pitch jumps. This is used to coordinate with an
        // external attack-aware correction helper (e.g. ovtdsp::AttackAwareEnv):
        // when the helper already controls the correction gain, having
        // BOTH the helper and the internal envelope run at the same time
        // creates a "double attenuation" that the user perceives as a
        // scratchy artifact, especially at low Amount values. Default: ON
        // (backward-compatible behaviour).
        void setAttackEnvelopeEnabled (bool enabled) noexcept
        {
            attackEnvelopeEnabled = enabled;
            // When the envelope is disabled, snap attackGain to 1.0 so the
            // output is never muted.
            if (! enabled) attackGain = 1.0f;
        }

        bool isAttackEnvelopeEnabled() const noexcept { return attackEnvelopeEnabled; }

        // 2026-07-23: external attack-gain driver (Fix AW â€” see
        // PluginProcessor.cpp for the call site). When set, the value is
        // used as the BLOCK-LEVEL target for `attackGain`, with a
        // block-aware one-pole smoother (TC = externalAttackTauSeconds,
        // default 15 ms) absorbing the per-block jumps from the external
        // source. Crucially, the modulation is applied to the OUTPUT
        // multiplier, NOT to the OLA target ratio, so the OLA chain's
        // grain spacing is stable across the transition (no re-alignment
        // clicks). Pass a value in [0, 1]; pass a NEGATIVE value to
        // disable the external driver and fall back to the internal
        // envelope (or the raw output if `attackEnvelopeEnabled` is off).
        //
        // This is the architectural fix for the "Speed=0 + Attack=10 ms
        // scratch" bug: the original code modulated the amount (which
        // multiplied into targetRatio) and disabled the internal envelope
        // to avoid double-attenuation, leaving the OLA chain with no
        // smoothing at all when the user's Speed knob was 0.
        void setExternalAttackGain (float gain, float blockDurSec) noexcept
        {
            if (gain < 0.0f)
            {
                externalAttackEnabled = false;
                return;
            }
            externalAttackEnabled = true;
            const float clamped = juce::jlimit (0.0f, 1.0f, gain);
            // Single-step IIR smoother on the per-block target. The
            // smoother's TC (default 15 ms) is independent of the
            // buffer size, so the per-block jumps from the external
            // source (AttackAwareEnv's IIR ramp) are absorbed smoothly
            // without per-sample discontinuities. The output multiplier
            // is constant within the block, so the OLA chain's grain
            // spacing is stable across the transition.
            attackGain = externalAttackSmoother.step (clamped, blockDurSec);
        }

        // Set the time constant (in seconds) of the external attack
        // smoother. Default is 0.015 (15 ms). Should be called from
        // prepare() once the sample rate is known (it does NOT depend on
        // the sample rate, but the smoother's internal alpha does).
        void setExternalAttackTauSeconds (float tauSec) noexcept
        {
            externalAttackTauSeconds = juce::jmax (0.001f, tauSec);
            externalAttackSmoother.setTimeConstantSeconds (externalAttackTauSeconds);
        }

        // Reset the external attack smoother (e.g. on transport stop or
        // preset change). After reset, the next setExternalAttackGain
        // call snaps the smoother to the new target.
        void resetExternalAttackGain() noexcept
        {
            externalAttackSmoother.snapTo (1.0f);
            attackGain = 1.0f;
            externalAttackEnabled = false;
        }

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

        // Pitch d'entree lisse (one-pole) utilise pour calculer
        // targetF0 / Tin / Tout. Sans cela, un saut brutal de f0 (nouvelle
        // note ou attaque) change la periode des grains d'un coup ->
        // discontinuite dans l'OLA -> clic. Le lissage adoucit la
        // transition de periode sans colorer le timbre (le ratio de
        // correction est deja lisse par RetargetEnvelope en aval).
        //
        // 2026-07-23 (Fix BA): alpha was 0.002 per block, giving TC =
        // 1 / (0.002 * 172 blocks/sec) = ~2.9 seconds. That was way too
        // slow: at 64-256 sample buffers, the user reported audible pops
        // at every note onset because smoothedF0 took several seconds
        // to converge to the new f0, during which the OLA chain targeted
        // WRONG periods (mid-transition between the old and new pitch).
        // The new alpha of 0.02 gives TC = 1 / (0.02 * 172) = ~290 ms,
        // which is fast enough to follow typical note attacks (5-30 ms
        // attack time on most instruments) within a few blocks, and slow
        // enough to remain a lowpass filter for the 5Hz vibrato (|H(5Hz)|
        // = 0.62, so the smoothedF0 follows the vibrato at 62% of its
        // amplitude â€” sufficient because the YIN pitch detector already
        // does its own smoothing, and the VibratoPreserver does the
        // vibrato preservation on the targetRatio, not on smoothedF0).
        float smoothedF0 = 0.0f;
        static constexpr float kF0SmoothAlpha = 0.02f;
        
        double outPhase = 0.0;
        double lastGrainCenter = 0.0;

        // 2026-07-23 (Fix BB): previous pitch ratio (per block), used to
        // detect sudden changes in `pitchRatio` (e.g. FlexTune transitions
        // out of/into the deadband, humanize random walk, vibrato
        // preservation switches). When the per-block delta exceeds a
        // threshold (3% in this implementation), the internal attack
        // envelope is armed to mask the OLA re-organisation. Without
        // this, the OLA chain "snaps" to a new period every time the
        // smoother output changes by more than ~1% per block, producing
        // the user-reported "pop/clics aux changements de pitch" with
        // Flex>0. Note: the ONSET detection (f0 transitions to voiced
        // or >2-semitone jumps) is independent and still works.
        float lastPitchRatio = 1.0f;

        // Attack envelope state
        float attackMs = 30.0f;
        double attackAlpha = 0.0;
        float attackGain = 0.0f;
        bool attackEnvelopeEnabled = true; // Disable when an external helper (AttackAwareEnv) owns the envelope.
        bool wasVoiced = false;
        float lastF0 = 0.0f; // For detecting sudden pitch jumps (note attacks)

        // 2026-07-23: external attack-gain smoother (Fix AW). The smoother
        // is a BlockAwareOnePole (TC = 15 ms by default) that absorbs the
        // per-block jumps from the external source (AttackAwareEnv). The
        // smoothed value is the OUTPUT multiplier, not a target-ratio
        // modulation, so the OLA chain's grain spacing is stable.
        BlockAwareOnePole externalAttackSmoother;
        bool externalAttackEnabled = false;
        // 15 ms TC. Matches the average user expectation for "fast but
        // not clicky" attack. Can be tuned at the call site via
        // setExternalAttackGainTauSeconds().
        float externalAttackTauSeconds = 0.015f;

        // Hysteresis + debounce pour la detection d'onset (evite les clics
        // repetes quand le pitch frÃ©mit autour du seuil voiced/unvoiced au
        // demarrage d'une note). Seuil montee/descente differents et N
        // echantillons consÃ©cutifs requis avant de valider un changement.
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


