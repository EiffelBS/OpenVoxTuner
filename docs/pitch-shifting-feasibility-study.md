# Étude de faisabilité : Algorithmes alternatifs de Pitch Shifting & Time Stretching

> **📁 ARCHIVÉ (2026-07-11) :** Étude historique. OpenVoxTuner utilise désormais uniquement le moteur PSOLA maison. RubberBand et SoundTouch ont été retirés.

Suite à l'implémentation de **RubberBand**, **SoundTouch** et d'un moteur **Delay-Line Crossfade (WSOLA-like)**, cette étude évalue d'autres moteurs et algorithmes disponibles sur le marché pour une intégration dans le projet Autotune Clone.

## 1. Algorithmes Temporels (Time-Domain)

### 1.1. WSOLA (Waveform Similarity Overlap-Add)
- **Description** : Amélioration de l'algorithme OLA classique. Cherche la meilleure corrélation croisée (cross-correlation) entre le grain à synthétiser et le flux audio pour aligner parfaitement les phases.
- **Compatibilité** : Très bonne. C'est le standard de l'industrie pour les algorithmes temporels open-source (SoundTouch utilise une variante de TD-PSOLA/WSOLA).
- **Performances** : Très rapide (faible CPU). Latence très faible (généralement la taille de la fenêtre d'analyse, ~30-50ms).
- **Qualité Audio** : Très bonne pour la voix et les instruments monophoniques. Peut créer des artefacts de "flanging" ou de "phasiness" sur des mixages polyphoniques complexes ou des percussions.
- **Recommandation** : **Déjà implémenté.** Notre moteur "PSOLA/Legacy" actuel a été réécrit pour utiliser un Delay-Line Crossfade qui est l'architecture fondamentale sur laquelle repose WSOLA. Pour aller plus loin, il faudrait ajouter une étape d'alignement de phase (cross-correlation) sur les têtes de lecture, mais la version actuelle est déjà robuste.

## 2. Algorithmes Spectraux (Frequency-Domain)

### 2.1. Phase Vocoder (Rubber Band)
- **Description** : Analyse le signal via STFT (Short-Time Fourier Transform), modifie les fréquences ou le temps, puis resynthétise via iSTFT tout en préservant/corrigeant la phase des bins fréquentiels.
- **Compatibilité** : Excellente.
- **Performances** : CPU moyen à élevé. Latence inhérente liée à la taille de la fenêtre STFT (souvent > 1024 samples, soit ~25-50ms).
- **Qualité Audio** : Excellente pour la polyphonie et les grands ratios de transposition.
- **Recommandation** : **Déjà implémenté.** `RubberBand` est actuellement notre meilleur moteur. C'est l'algorithme spectral open-source de référence.

### 2.2. zplane Élastique Pro
- **Description** : L'algorithme de pitch-shifting/time-stretching commercial le plus réputé de l'industrie audio (utilisé par Ableton Live, FL Studio, Reaper, Cubase, etc.).
- **Compatibilité** : Excellente (fourni sous forme d'une SDK C++ facile à intégrer dans JUCE). Latence extrêmement faible.
- **Performances** : Extrêmement optimisé (SIMD/AVX).
- **Qualité Audio** : La référence absolue. Formant preservation parfaite, aucun artefact de transient, respect de la polyphonie.
- **Licence** : Commerciale uniquement, très onéreuse (plusieurs milliers d'euros pour une licence de distribution commerciale, ou redevance par unité vendue).
- **Recommandation** : **Non recommandé** à ce stade du projet en raison des coûts de licence prohibitifs. C'est la solution ultime si le plugin est destiné à être vendu à grande échelle.

## 3. Algorithmes basés sur l'Intelligence Artificielle (Deep Learning)

### 3.1. RVC (Retrieval-based Voice Conversion) / DDSP
- **Description** : Modèles de deep learning (souvent basés sur des architectures vocoder neurales comme HiFi-GAN, ou des modèles de diffusion) capables de resynthétiser complètement une voix. Ils peuvent transposer le pitch tout en préservant parfaitement le timbre (voire en le changeant pour imiter quelqu'un d'autre).
- **Compatibilité** : Très difficile. Nécessite l'intégration de runtimes d'inférence lourds (ONNX Runtime, libtorch/PyTorch C++). Les dépendances font exploser la taille du plugin (> 500 Mo).
- **Performances** : CPU/GPU extrêmement gourmand. Sans GPU (CUDA/CoreML/Metal), l'inférence en temps réel sur CPU est très difficile voire impossible sans craquements sur des machines moyennes. Latence très élevée (souvent > 100-200ms) incompatible avec du monitoring live (chant en direct).
- **Qualité Audio** : Phénoménale pour la voix (qualité humaine indiscernable), mais uniquement pour la voix monophonique propre.
- **Licence** : Souvent open-source (MIT/Apache) pour le code, mais les poids des modèles peuvent avoir des licences restrictives.
- **Recommandation** : **Non recommandé pour du Live.** Incompatible avec un usage Autotune "Zero-Latency" ou "Low-Latency". C'est l'avenir pour du post-processing (édition offline), mais l'architecture actuelle du plugin (temps réel, traitement bloc par bloc) n'est pas adaptée.

## 4. Synthèse et Classement des Solutions

Pour un plugin VST3 "Auto-Tune" (temps réel, faible latence, focus voix), voici le classement des moteurs par pertinence :

1. **Rubber Band (Spectral / Phase Vocoder)** : *Intégré.* Meilleur rapport qualité / open-source.
2. **SoundTouch (Temporel / WSOLA-like)** : *Intégré.* Excellente alternative low-CPU, très bon sur la voix.
3. **Delay-Line Crossfade (Temporel pur)** : *Intégré.* Le plus basique, utile pour des effets "robotiques" ou Chorus.
4. **zplane Élastique Pro** : L'idéal absolu, mais bloqué par son coût commercial.
5. **RVC / Neural Vocoders** : Qualité vocale parfaite, mais totalement inadapté au temps réel en raison de la latence et de la charge CPU/GPU.

### Conclusion de l'étude
Notre infrastructure actuelle couvre déjà les meilleures options open-source disponibles. L'ajout d'une étape de *cross-correlation* (alignement de phase) à notre moteur Delay-Line permettrait d'atteindre la qualité de WSOLA sans dépendre de SoundTouch, ce qui serait l'amélioration logique suivante si l'on souhaite se détacher des bibliothèques tierces.