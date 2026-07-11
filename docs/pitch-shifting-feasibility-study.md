# Feasibility Study: Alternative Pitch Shifting & Time Stretching Algorithms

> **📁 ARCHIVED (2026-07-11):** Historical study. OpenVoxTuner now uses only the in-house PSOLA engine. RubberBand and SoundTouch have been removed.

Following the implementation of **RubberBand**, **SoundTouch** and a **Delay-Line Crossfade (WSOLA-like)** engine, this study evaluates other engines and algorithms available on the market for integration into the Autotune Clone project.

## 1. Time-Domain Algorithms

### 1.1. WSOLA (Waveform Similarity Overlap-Add)
- **Description**: An improvement over the classic OLA algorithm. It searches for the best cross-correlation between the grain to be synthesized and the audio stream in order to perfectly align the phases.
- **Compatibility**: Very good. It is the industry standard for open-source time-domain algorithms (SoundTouch uses a TD-PSOLA/WSOLA variant).
- **Performance**: Very fast (low CPU). Very low latency (generally the size of the analysis window, ~30-50ms).
- **Audio Quality**: Very good for voice and monophonic instruments. Can create "flanging" or "phasiness" artefacts on complex polyphonic mixes or percussion.
- **Recommendation**: **Already implemented.** Our current "PSOLA/Legacy" engine was rewritten to use a Delay-Line Crossfade, which is the fundamental architecture on which WSOLA is built. To go further, we would need to add a phase-alignment (cross-correlation) step on the read heads, but the current version is already robust.

## 2. Spectral Algorithms (Frequency-Domain)

### 2.1. Phase Vocoder (Rubber Band)
- **Description**: Analyzes the signal via STFT (Short-Time Fourier Transform), modifies frequencies or time, then resynthesizes via iSTFT while preserving/correcting the phase of the frequency bins.
- **Compatibility**: Excellent.
- **Performance**: Medium to high CPU. Inherent latency tied to the STFT window size (often > 1024 samples, i.e. ~25-50ms).
- **Audio Quality**: Excellent for polyphony and large transposition ratios.
- **Recommendation**: **Already implemented.** `RubberBand` is currently our best engine. It is the reference open-source spectral algorithm.

### 2.2. zplane Élastique Pro
- **Description**: The most renowned commercial pitch-shifting/time-stretching algorithm in the audio industry (used by Ableton Live, FL Studio, Reaper, Cubase, etc.).
- **Compatibility**: Excellent (provided as a C++ SDK that is easy to integrate into JUCE). Extremely low latency.
- **Performance**: Extremely optimized (SIMD/AVX).
- **Audio Quality**: The absolute benchmark. Perfect formant preservation, no transient artefacts, full polyphony support.
- **Licence**: Commercial only, very expensive (several thousand euros for a commercial distribution licence, or royalty per unit sold).
- **Recommendation**: **Not recommended** at this stage of the project due to the prohibitive licence costs. It is the ultimate solution if the plugin is intended to be sold at large scale.

## 3. Artificial Intelligence Based Algorithms (Deep Learning)

### 3.1. RVC (Retrieval-based Voice Conversion) / DDSP
- **Description**: Deep learning models (often based on neural vocoder architectures such as HiFi-GAN, or diffusion models) capable of fully resynthesizing a voice. They can transpose the pitch while perfectly preserving the timbre (or even changing it to imitate someone else).
- **Compatibility**: Very difficult. Requires the integration of heavy inference runtimes (ONNX Runtime, libtorch/PyTorch C++). The dependencies blow up the plugin size (> 500 MB).
- **Performance**: Extremely CPU/GPU intensive. Without a GPU (CUDA/CoreML/Metal), real-time inference on CPU is very difficult or even impossible without crackling on average machines. Very high latency (often > 100-200ms) incompatible with live monitoring (live singing).
- **Audio Quality**: Phenomenal for voice (indistinguishable human quality), but only for clean monophonic voice.
- **Licence**: Often open-source (MIT/Apache) for the code, but the model weights may have restrictive licences.
- **Recommendation**: **Not recommended for Live use.** Incompatible with a "Zero-Latency" or "Low-Latency" Autotune use case. It is the future for post-processing (offline editing), but the plugin's current architecture (real-time, block-by-block processing) is not suited to it.

## 4. Synthesis and Solution Ranking

For a VST3 "Auto-Tune" plugin (real-time, low latency, voice focus), here is the ranking of engines by relevance:

1. **Rubber Band (Spectral / Phase Vocoder)**: *Integrated.* Best quality / open-source ratio.
2. **SoundTouch (Time-Domain / WSOLA-like)**: *Integrated.* Excellent low-CPU alternative, very good on voice.
3. **Delay-Line Crossfade (Pure Time-Domain)**: *Integrated.* The most basic, useful for "robotic" effects or Chorus.
4. **zplane Élastique Pro**: The absolute ideal, but blocked by its commercial cost.
5. **RVC / Neural Vocoders**: Perfect voice quality, but totally unsuited to real-time due to latency and CPU/GPU load.

### Study Conclusion
Our current infrastructure already covers the best open-source options available. Adding a *cross-correlation* step (phase alignment) to our Delay-Line engine would allow us to reach WSOLA quality without depending on SoundTouch, which would be the logical next improvement if we want to move away from third-party libraries.
