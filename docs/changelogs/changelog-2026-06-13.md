# Changelog - 2026-06-13

## Optimisations de Performance (CPU)
* **PitchDetector (YIN)** :
  * Restructuration de la boucle interne de la fonction de différence pour permettre l'auto-vectorisation par le compilateur (SIMD).
  * Réduction de la plage de recherche de la boucle externe (`maxTau`) pour ne calculer que jusqu'à la limite inférieure de fréquence requise, évitant des calculs inutiles.
* **PluginProcessor** :
  * Implémentation d'une décimation par 4 du signal d'entrée avant l'analyse YIN. La fenêtre d'analyse de 2048 échantillons est compressée à 512 échantillons, ce qui divise le nombre de calculs du détecteur de pitch par 16.
  * Augmentation de `analysisHopSize` de 512 à 1024 échantillons. Le calcul du pitch s'effectue désormais toutes les ~23ms (au lieu de ~11ms).
* **PitchShifter (Internal Engine)** :
  * Optimisation drastique de la méthode `findBestOffset` (calcul de corrélation croisée pour l'alignement de phase granulaire). Remplacement des accès par interpolation flottante et des boucles `while` de wrapping par un accès direct entier avec masquage binaire (`bitwise AND`).
  * Augmentation du pas de recherche de phase (step) de 2 à 4, et décimation de la comparaison de corrélation (1 échantillon sur 2 évalué), réduisant considérablement la charge CPU lors de la génération de chaque nouveau grain audio.

## Fonctionnalités (Features)
* **Formant Shift** : 
  * Ajout d'un nouveau paramètre de décalage de formants (Formant Shift) contrôlable via un bouton rotatif dans l'interface principale, allant de -12 à +12 demi-tons.
  * Connecté au moteur DSP `FormantPreserver`. Permet d'assombrir ou d'éclaircir artificiellement le timbre de la voix indépendamment du pitch.

## Architecture & Dépendances
* **Préparation au support ARA2 (Audio Random Access)** :
  * Intégration du Celemony ARA SDK (v2.2.0) au projet via CMake (`juce_set_ara_sdk_path`).
  * Activation du flag `IS_ARA_EFFECT TRUE` dans la configuration CMake. Le plugin est désormais compilé avec les interfaces d'extension ARA2.
  * Implémentation du contrôleur de base `AutotuneCloneARADocumentController` et du générateur `createARAFactory()`.
  * Implémentation d'un mécanisme de secours robuste dans `processBlock` : si le DAW ne supporte pas ARA, le plugin bascule silencieusement sur le moteur temps-réel habituel.
  * Lecture et Extraction dynamique de la **Tonalité (Key Signature)** : Interrogation de l'`ARAMusicalContext` via `HostContentReader` pour décoder le `root` (conversion du Cycle des Quintes en Chromatique) et le mode (Majeur/Mineur).
  * Synchronisation automatisée des paramètres de l'UI (`key` et `scale`) avec la tonalité détectée par ARA.
  * **Correction de l'extraction ARA Key Signature** : Le décodage du tableau `intervals` distingue désormais correctement le Majeur, le Mineur Naturel, et le Chromatique.
  * **Éditeur Graphique synchronisé au BPM** : La timeline (axe X) utilise désormais le `PPQ` du DAW pour boucler sur 16 Beats (4 mesures en 4/4) au lieu de 4 secondes fixes, avec affichage des marqueurs de mesures ("M 1", "M 2", etc.).
  * **Synchronisation Playhead (ARA et Live)** : Si l'hôte boucle, le playhead soustrait le point de départ de la boucle pour toujours recommencer à gauche de l'écran. En mode Live/Standalone, le playhead continue d'avancer visuellement même s'il y a du silence.
  * **Bouton Clear Curve** : Ajout d'un bouton pour vider la courbe et réinitialiser le compteur de temps Standalone à zéro.
  * **VRAI Formant Shift Intégré (WSOLA)** : Remplacement complet du filtre Peaking par une modification profonde du moteur granulaire WSOLA. Le micro-timing (Formant) est désormais totalement découplé du macro-timing (Pitch), permettant de changer la taille du conduit vocal de -5 à +5 demi-tons sans annuler le tuning et sans artefacts.
  * Rédaction du document de spécifications ARA2 (`docs/ARA_Specifications.md`) détaillant la hiérarchie Clip/Track et les DAW cibles.
* **Suppression des moteurs externes** : 
  * Retrait complet de **RubberBand** et **SoundTouch** de la base de code (`CMakeLists.txt`, headers, cpp).
  * Le moteur `Internal` maison étant désormais supérieur en termes de CPU (0%) et de qualité (aucun clic ni pop), conserver ces dépendances externes n'avait plus de sens, d'autant plus qu'elles imposaient des licences contraignantes (GPL / LGPL).
  * Retrait du menu déroulant "Engine" de l'interface utilisateur et des paramètres internes du plugin. Le routage s'effectue exclusivement et directement vers l'instance de `PitchShifter`.

## Correction de Bugs
* **Moteur Internal (WSOLA)** : 
  * Fix de légers "pops" audio ("clics") survenant lorsque l'utilisateur maintient une note parfaitement juste (`ratio ≈ 1.0`). La logique de "passthrough" brutale qui désactivait l'alignement de phase (et provoquait une interférence destructive à la frontière du grain) a été supprimée. Le moteur WSOLA reste désormais toujours actif pour garantir une continuité de phase parfaite en toute circonstance.
  * Restriction de la fenêtre de recherche de corrélation (`searchWindowMs`) à 10ms (strictement inférieure à la latence granulaire de 15ms) afin de garantir mathématiquement que la tête de lecture ne tentera jamais de lire des échantillons audio du "futur" (non encore écrits dans le ring buffer).
* Validation et confirmation de la réussite des 60 tests unitaires, incluant ceux de `ScaleQuantizer`.
