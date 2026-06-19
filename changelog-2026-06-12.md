# Changelog - 12 Juin 2026

## 1. Resolution des artefacts sur buffers non-standards (Round 13)
* **Probleme** : Le traitement audio avec `RubberBand` etait fonctionnel aux buffers 1024 et 2048, mais des artefacts (chops/clicks) apparaissaient sur d'autres tailles (ex: 1280, 1920) car le shifter exige exactement 512 echantillons par appel, et notre ancien buffer circulaire ne gerait pas correctement les desynchronisations entre l'hote (Variable Block Size) et le shifter (Fixed Block Size).
* **Solution** : 
  - Remplacement complet des buffers circulaires manuels par des `juce::AbstractFifo` dans `RubberBandPitchShifter`.
  - Separation totale entre l'entree (host -> inputFifo), le traitement DSP (inputFifo -> shifter -> outputFifo) par blocs stricts de 512, et la sortie (outputFifo -> host).
  - Pre-remplissage du buffer de sortie avec un silence de securite (bufferingLatency) afin d'eviter tout risque de buffer underrun lie au jitter de consommation du VST3.

## 2. Refonte de l'interface graphique (Style Auto-Tune Pro)
* **Theme & Couleurs** : Creation d'un `AutotuneLookAndFeel` heritant de `juce::LookAndFeel_V4`.
* **Knobs (Sliders)** : Remplacement de l'affichage par defaut par des boutons rotatifs sombres avec un anneau lumineux peripherique (glow) dependant de l'etat d'activation.
* **ComboBox** : Redessinees pour etre plates et modernes avec une fleche d'indication personnalisee.
* **PianoKeyboard** : Refonte totale pour un effet 3D. 
  - Touches blanches avec gradient vertical et bordures fines.
  - Touches noires avec ombre portee, gradient asymetrique et coins droits arrondis.
  - Fix d'un bug ou la hauteur des touches n'etait que d'un pixel.
* **Layout General** : Ajout d'un gradient global en arriere-plan dans `PluginEditor` (de sombre a noir/violet). Les fonds de `PitchCurveEditor` et `PianoKeyboard` ont ete rendus transparents afin de laisser apparaitre le beau degrade d'arriere-plan.
* **Visualiseur** : Ajout d'un effet de remplissage sous la courbe de pitch corrige (vert neon avec transparence) pour un effet de brillance "glow".

## 3. Architecture Multi-Moteurs de Pitch-Shifting
* **Conception et Interface** : Création d'une interface commune `IPitchShifter` (`Source/dsp/IPitchShifter.h`) garantissant l'interchangeabilité de plusieurs moteurs en exposant des méthodes standard (`prepare`, `reset`, `process`, `getLatencySamples`).
* **Intégration SoundTouch** : Téléchargement et intégration de la bibliothèque SoundTouch (LGPL) directement via `CMakeLists.txt`. Création d'un wrapper `SoundTouchPitchShifter`.
* **Restauration de PSOLA** : Restauration de l'ancien code d'analyse-synthèse PSOLA maison, converti pour utiliser l'interface `IPitchShifter`.
* **Switch Dynamique** : Ajout d'une option `AudioParameterChoice` (`engine`) et d'un menu déroulant (`ComboBox`) "Engine" dans l'UI pour basculer à la volée entre **RubberBand**, **SoundTouch** et **PSOLA**.
* **Documentation** : Ajout du fichier `docs/multi-engine-architecture.md` décrivant le fonctionnement, la sélection, et l'ajout ultérieur de moteurs.

## 4. Fix des moteurs alternatifs (SoundTouch & PSOLA)
* **SoundTouch** : Résolution du problème de "clics" audibles à chaque changement de pitch. 
  - *Cause* : Sans FIFO, l'hôte recevait des zéros lors des latences internes. De plus, la FIFO insérée était purgée brutalement en cas de dérive, et le paramètre de ratio n'était pas lissé.
  - *Correction* : Ajout d'un lissage du ratio sur 50ms (pour ne pas perturber l'algorithme interne de SoundTouch), et retrait complet de la purge de FIFO, laissant l'algorithme stabiliser la phase de sortie naturellement.
* **PSOLA / Maison** : L'algorithme PSOLA original a été totalement supprimé car fondamentalement instable (il "choppait" dès qu'une correction était appliquée).
  - *Cause* : Les têtes de lecture "sautaient" lors des modulations de ratio sans respecter le passage par zéro des fenêtres d'amplitude, et sans vérifier la cohérence de phase (ce qui créait des "chops" et des annulations de phase).
  - *Correction* : Réécriture complète sous la forme d'un algorithme robuste de type "Delay-Line Crossfade". Implémentation complète de **WSOLA (Waveform Similarity Overlap-Add)** : au moment du saut de tête de lecture (wrap), l'algorithme recherche la meilleure cross-corrélation (alignement de phase maximal) entre le signal "passé" et le signal "présent" dans une fenêtre temporelle donnée. Le décalage optimal est appliqué avant le fondu croisé. Le résultat est garanti sans aucune coupure et préserve l'intégrité de la forme d'onde, éliminant totalement l'effet "hélicoptère/chops".

## 5. Étude de faisabilité des algorithmes alternatifs
* Rédaction d'un rapport détaillé dans `docs/pitch-shifting-feasibility-study.md` évaluant la pertinence de :
  - **Algorithmes temporels** : WSOLA (recommandé et proche de notre implémentation Delay-Line).
  - **Algorithmes spectraux** : Rubber Band (déjà intégré), zplane Élastique Pro (qualité ultime mais bloqué par le coût de licence).
  - **Algorithmes IA** : RVC / DDSP (qualité phénoménale mais inadapté pour un "Autotune" en temps réel).

## 6. Correctifs UX & Configuration
* **Configuration par défaut** : Le moteur "PSOLA (Legacy)" a été renommé "Internal" et défini comme moteur de traitement audio par défaut au démarrage du plugin.
* **Éditeur de pitch (PitchCurveEditor)** : 
  - Réparation de la fonctionnalité de glisser-déposer (Drag & Drop) des points de la courbe.
  - Ajout d'une infobulle (Tooltip) dynamique affichant la note (ex: C4) et le temps (ex: 1.25s) lors du survol et du déplacement.
  - Clarification de la grille temporelle avec l'affichage explicite des secondes (1.0s, 2.0s, etc.).
  - Implémentation du bouclage temporel continu (modulo 4.0s) pour que l'éditeur graphique soit fonctionnel en mode Standalone.
  - Ajout du magnétisme intelligent (Snap) : snap-to-grid temporel (à 0.05s près) et snap-to-note (à 15 cents près) même lorsque la correction de gamme globale est désactivée.
  - **Playhead** : Ajout d'une barre de lecture verticale rouge qui suit la position actuelle du séquenceur ou le temps continu en mode Standalone.
* **Refonte de la Sélection de Gamme** : 
  - Suppression complète des 12 cases à cocher de la gamme personnalisée.
  - Création d'un nouveau composant `ScaleKeyboardComponent` : un mini-clavier de piano horizontal et interactif.
  - En mode *Preset* (Majeur, Mineur, etc.), le clavier affiche de manière lisible toutes les notes actives de la gamme correspondante (read-only).
  - En mode *Custom*, le clavier devient interactif : un clic sur chaque touche (blanche ou noire) active ou désactive la note avec un retour visuel coloré instantané, tout en restant parfaitement synchronisé avec le moteur audio.
  - Bascule intelligente : interagir avec une touche sur n'importe quelle gamme préconfigurée bascule automatiquement l'interface et le moteur DSP sur le mode "Custom" pour conserver la modification de l'utilisateur.
  - Extension majeure de la liste des gammes : Ajout de *Melodic Minor, Harmonic Minor, Natural Minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Blues, Major Triad et Minor Triad* pour un total de 15 modes + Custom.
* **Interface des Modes** : Suppression de la liste déroulante redondante "Mode (Auto/Graphic)". Le changement de mode est désormais piloté de façon fluide et exclusive par les onglets.
* **Refonte Visuelle Bas de Page** : 
  - Réorganisation des contrôles inférieurs en 3 blocs thématiques distincts (Correction, Moteur, Gamme) pour une meilleure lisibilité.
  - Correction du bug d'affichage (hauteur excessive) des listes déroulantes.

## 8. Optimisation CPU (Sleep Mode)
* **Détection Intelligente de Silence** :
  - Ajout d'une analyse rapide du RMS/Peak de l'entrée audio.
  - Mise en veille automatique (bypass interne) du plugin après 500 ms de silence total (<-80 dB).
  - Désactivation totale du détecteur YIN, de la préservation des formants et des algorithmes de pitch-shifting (RubberBand, SoundTouch, Internal) pendant la veille.
  - **Résultat** : La consommation CPU du plugin chute de 14% (Internal) / 49% (RubberBand) à **~1%** lorsqu'il n'y a pas d'audio à traiter, sans aucun artefact (clic ou décrochage) lors de la reprise de l'audio.
* **Déclaration de Latence Dynamique (PDC)** :
  - Le plugin déclare désormais correctement sa latence de traitement au DAW (ex: Studio One) via l'appel `setLatencySamples()`.
  - La valeur de latence s'ajuste dynamiquement en fonction du moteur sélectionné et est transmise instantanément au séquenceur. Le DAW affiche désormais correctement la latence (~10ms) au lieu de 0.0ms.

## 7. Correction DSP (Quantificateur)
* **Gamme Chromatique** : Correction du bug qui empêchait la gamme chromatique d'autoriser toutes les notes. Le quantificateur a été réécrit pour corriger le pitch vers le demi-ton le plus proche, agissant comme un véritable effet "T-Pain" sur l'ensemble des 12 notes sans les filtrer vers la gamme Majeure de C.
* **Synchronisation UI/DSP** : Correction d'un bug majeur où le changement de gamme (Scale) dans l'interface graphique n'était pas propagé au moteur audio. Les gammes (ex: Chromatic, Major, Minor) s'appliquent désormais instantanément.
* Fix d'un probleme de compilation des tests unitaires cause par une visibilite `private` des constantes de couleur dans `PluginEditor.h`.
* Fix des chemins d'inclusion (`#include`) dans les tests unitaires.
* Fix d'un bug de retour de reference locale dans `PitchCurve.h` detecte par le compilateur.
* Fix de l'initialisation du moteur par défaut : le plugin chargeait silencieusement RubberBand au démarrage tout en affichant "Internal" dans l'UI. Cela causait des artefacts audio sur certains buffers jusqu'à ce que l'utilisateur force un aller-retour dans la liste déroulante. L'initialisation dynamique garantit désormais que le véritable moteur "Internal" est chargé dès le lancement.
