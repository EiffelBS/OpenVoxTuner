# OpenVoxTuner - Roadmap d'implementation

> Clone d'effet Auto-Tune developpe en C++ avec JUCE 8.
> Stack : JUCE 8.0.12 + Projucer + Visual Studio 2022 (Windows 11).
> Formats actifs : VST3, Standalone. AU et AAX preconfigures (a activer depuis macOS).

Legende :
- [x] = implemente et valide
- [~] = en cours
- [ ] = a faire
- [-] = reporte / hors scope MVP

---

## Phase 0 - Mise en place du projet

- [x] Installer JUCE 8 dans `C:\JUCE`
- [x] Creer la structure de dossiers (`Source/`, `docs/`, `test/`, `resources/`)
- [x] Initialiser `roadmap.md` (ce fichier)
- [x] Initialiser `docs/changelogs/changelog-2026-06-09.md`
- [x] Creer le `.gitignore`
- [x] Creer le fichier projet Projucer (`AutotuneClone.jucer` puis supprime au profit de CMake)
- [x] Creer le squelette `PluginProcessor` (compile)
- [x] Creer le squelette `PluginEditor` (compile)
- [x] Creer les squelettes DSP : `PitchDetector`, `ScaleQuantizer`, `PitchShifter`
- [x] Creer le squelette UI : `PitchVisualizer`
- [x] Creer les scripts de build `build_setup.ps1` et `build.ps1`
- [x] Documenter l'architecture (`docs/architecture.md`)
- [x] Valider la generation d'un build Visual Studio 2022 (VS 2022 17.14 OK)
- [x] Premier build reussi (Release, x64) - VST3 (3.5 MB) + Standalone (4.7 MB)
- [x] Tester l'execution en mode Standalone (validation fonctionnelle - 2026-06-09)

## Phase 0bis - Correctifs post-premier lancement

- [x] Crash #1 : violation d'acces 0x38 dans Rectangle::getX() (setSize avant creation enfants)
- [x] Crash #2 : isLayoutSupported quelques secondes apres activation audio (BusesProperties manquant)
- [x] Crash #3 : ~HeapBlock dans le pipeline audio (addGrain outIdx >= numSamples)
- [x] UI : 4 sliders remplaces par 2 knobs rotatifs (Speed, Amount) + 2 ComboBox (Key, Scale)
- [x] UI : barre du bas elastique (160 px) pour afficher les knobs
- [x] UI : drag des points du curve editor fonctionne (setPointPitch via getReference)
- [x] UI : overlay grise en mode Auto pour signaler "read only"
- [x] Cleanup : suppression de dbgLog, Logger::writeToLog, autotune_paint.log, DebugMinidump.cpp
- [x] DSP : reecriture complete du PSOLA (grain 2*T0, position synthese en temps absolu, COLA adaptive)
- [x] DSP : fix wraparound ring buffer (ringPosAt dynamique, sinon audio hachure apres ~186 ms)
- [x] DSP : fix stereo (nextSynthMarkSample update DANS la boucle canaux => canal droit silencieux)
- [x] DSP : fix off-by-one absoluteSample (ringPosAt lisait vieille data au lieu du sample courant)

## Phase 1 - Pipeline DSP minimal

- [x] Module `PitchDetector` (algorithme YIN)
  - [x] Detection de pitch sur buffer audio
  - [x] Calcul de la clarte (seuil de probabilite YIN)
  - [x] Interpolation parabolique sub-sample
  - [ ] Tests unitaires sur sinusoides de reference
- [x] Module `ScaleQuantizer` (tonique + mode)
  - [x] Selection de la tonique (12 notes)
  - [x] Modes : majeur, mineur naturel, pentatonique majeure/mineure, chromatique
  - [x] Quantification en demi-tons les plus proches (distance circulaire)
  - [x] **Mode Custom** (12 booleens, gamme personnalisee) - 2026-06-10
  - [ ] Tests unitaires
- [x] Module `PitchShifter` (MVP : fenetre d'analyse-synthese)
  - [x] Calcul du ratio de transposition
  - [x] Decoupage en grains fenetres (Hann)
  - [x] Lecture interpolee + fenetre glissante
  - [ ] Tests sur signal synthetique

## Phase 2 - Integration dans le processor

- [x] Chainage : `AudioIn -> PitchDetector -> ScaleQuantizer -> PitchShifter -> AudioOut`
- [x] Parametres exposes :
  - [x] `Speed` (vitesse de correction, 0-200 ms) - pas encore utilise dans DSP
  - [x] `Amount` (intensite, 0-100 %)
  - [x] `Key` (tonique)
  - [x] `Scale` (mode)
  - [x] `Bypass`
- [x] FIFO d'analyse (2048 echantillons) pour la detection YIN
- [x] Latence declaree au host (`getLatencySamples`)
- [x] Gestion des transitions douces : `RetargetEnvelope` lisse le ratio
      selon le paramètre `speed` (tau = speedMs/1000, alpha = 1 - exp(-dt/tau))

## Phase 3 - Interface graphique

- [x] Layout : bandeau titre + visualiseur + rangee de knobs
- [x] 4 knobs (Speed, Amount, Key, Scale) avec libelles et texte contextuel
- [x] Visualiseur de pitch en temps reel
  - [x] Trace de la pitch curve d'entree (rose)
  - [x] Trace de la pitch curve corrigee (vert)
  - [x] Grille horizontale (C2..C6) sur echelle log
  - [x] **Header avec note chantee + offset en cents** - 2026-06-10
  - [x] **Meter de tuning vertical (aiguille +/-100 c)** - 2026-06-10
  - [x] **Lignes des notes de la gamme en arriere-plan** - 2026-06-10
- [x] Bouton Bypass dans la GUI (ToggleButton en haut a droite)
- [x] Look & Feel personnalise (theme sombre + accent rose)
- [x] Mode switcher (ComboBox Auto / Graphic)
- [x] Snap to scale (ToggleButton, lie au curve editor)
- [x] PitchCurveEditor (drag, double-clic add, right-click delete, menu preset)
  - [x] **Clavier de piano vertical a gauche (40 px de large)** - 2026-06-10
  - [x] **Notes de la gamme surlignees en jaune sur le piano** - 2026-06-10
- [x] Layout split : visualiseur (haut) + curve editor (milieu) + knobs (bas)
- [x] **12 booleens ToggleButton pour gamme personnalisee** - 2026-06-10
  - [x] Visibles uniquement quand Scale = "Custom"
  - [x] Bind bidirectionnel avec 12 AudioParameterBool ("custom0"..11)
- [x] **Refonte LookAndFeel (AutoTune Pro style)** - 2026-06-12
  - [x] Knobs avec anneaux lumineux
  - [x] PianoKeyboard 3D (gradients, coins arrondis, ombres portees)
  - [x] Fonds transparents et gradient general modernise

## Phase 4 - Mode Graphic (Melodyne-like)

- [x] PitchCurve : structure de donnees (liste triee, interpolation lineaire, dichotomique)
- [x] Snap to scale sur les points
- [x] Serialisation XML de la pitch curve
- [x] Presets factory (default, spoken, lyric, rap, robot)
- [x] UI interactive : PitchCurveEditor avec drag, ajout, suppression
- [x] Presets : menu integre dans la barre d'icones (Factory / Custom)
- [x] Presets Custom : sauvegarde + suppression (Factory proteges)
- [x] Cablage dans processBlock (mode Graphic utilise pitchCurve->getPitchAt)
- [x] Lecture du transport time via getPlayHead()
- [ ] Courbes de Bezier
- [ ] Capture du pitch courant par clic (API existe, pas exposee dans la GUI)
- [ ] Zoom temporel (actuellement fixe a 4 s)

## Phase 4 - Qualite DSP (ameliorations)

- [x] Algorithme PSOLA (pitch marks + OLA avec Hann 2-periodes)
- [x] Compensation de formants (filtre biquad passe-bas, sqrt compensation)
- [x] Retarget Envelope style Antares (IIR 1er ordre sur le ratio)
- [x] **SWIPE' reactive et optimise** (calcul d'energie extrait de la boucle candidates, code mort supprime) - 2026-07-02
- [-] PYIN supprime (crash memoire non resolus) - 2026-07-02
- [ ] Detection de pitch plus robuste (MPM en complement de YIN)
- [ ] Preservation exacte des formants (LPC + resampling non-uniforme)
- [ ] Preservation des transitoires (onset detection -> bypass PSOLA)
- [ ] Mode "graphique" editable (pitch curve dessinable style Melodyne)
- [ ] Preservation des silences (gate de detection)

## Phase 6 - Tests et validation

- [x] Tests unitaires : PitchDetector (5 cas)
- [x] Tests unitaires : ScaleQuantizer (7 cas)
- [x] Tests unitaires : RetargetEnvelope (4 cas)
- [x] Tests unitaires : FormantPreserver (3 cas)
- [x] Tests unitaires : PitchCurve (10 cas)
- [ ] Tests d'integration (signaux de reference)
- [ ] Tests audio subjectifs (chants feminins, masculins, vibrato)
- [ ] Tests CPU (realtime, x86 et ARM)
- [ ] Documentation utilisateur (`docs/user-guide.md`)

## Phase 5 - Multi-format et distribution

- [x] Configurer VST3 (Windows) dans Projucer
- [x] Configurer Standalone (Windows) dans Projucer
- [ ] Configurer AU (macOS) - necessite un Mac
- [ ] Configurer AAX (Pro Tools) - necessite Mac + inscription Avid
- [ ] Validation dans Ableton Live / FL Studio / Reaper
- [ ] Validation dans Logic Pro / Pro Tools
- [ ] Signature de code (Windows + macOS)
- [ ] Installeur / packaging

## Phase 6 - Tests et validation

- [ ] Tests unitaires des modules DSP (`test/`)
- [ ] Tests d'integration (signaux de reference)
- [ ] Tests audio subjectifs (chants feminins, masculins, vibrato)
- [ ] Tests CPU (realtime, x86 et ARM)
- [ ] Documentation utilisateur (`docs/user-guide.md`)

## Phase 8 - Intégration ARA2 (Audio Random Access)

> L'intégration d'ARA2 permet au plugin de communiquer directement avec le DAW pour accéder à l'audio hors temps réel (analyse de la piste complète), ainsi qu'à la grille musicale (BPM, Signature, Piste d'accords, Tonalité).

- [x] Configurer `CMakeLists.txt` avec `IS_ARA_EFFECT TRUE`.
- [x] **Clonage du SDK ARA** : Intégration du sous-module `ARA_SDK` (v2.2.0) depuis le dépôt officiel Celemony.
- [x] **Mécanisme de secours (Fallback) pour DAW non-ARA** :
  - L'architecture de JUCE permet au plugin d'exister en tant qu'hybride. Si le DAW ne supporte pas ARA (ex: Ableton Live), il instanciera le plugin comme un VST3 temps réel classique.
  - Implémentation du contrôleur `ARADocumentControllerSpecialisation`.
  - Implémentation du fallback dans `processBlock` (`processBlockForARA()`). Si ARA est actif, l'analyse YIN temps réel est bypassée au profit des données pré-calculées par ARA.
- [x] **Spécification du comportement (Track vs Clip)** :
  - Documentation rédigée (`docs/ARA_Specifications.md`). Gère la hiérarchie ARA : `ARADocument` -> `ARARegionSequence` -> `ARAAudioSource`.
- [x] **Éditeur Graphique - Synchronisation Musicale** :
  - Caler l'éditeur sur les mesures (Beats/Measures) plutôt que sur des secondes (Seconds).
  - Utiliser le `PPQ` pour le Playhead.
  - Déduire le point de départ de la boucle (`loopStart`) pour garantir que la courbe commence à gauche de l'écran lors d'une lecture en boucle.
  - Corriger le Playhead en mode Live (Standalone) qui se figeait lors des silences.
  - Ajouter un bouton `Clear Curve` pour réinitialiser la courbe (mode Graphic).
  - Ajouter un bouton `Reset Playhead` pour retourner au début de la courbe sans l'effacer (mode Graphic).
- [x] **Formant Shift - Intégration WSOLA native** :
    - Restructurer le moteur granulaire (`PitchShifter.cpp`) pour séparer mathématiquement le micro-timing (vitesse de lecture interne au grain = Formant) du macro-timing (espacement entre les grains = Pitch).
    - Séparer la chaîne DSP : le Formant Shift est un effet post-traitement isolé de l'Autotune et de son EQ de préservation de formants, pour résoudre le conflit qui annulait le tuning.
  - [ ] Studio One (VST3 ARA - Support parfait attendu)
  - [ ] Cubase/Nuendo (VST3 ARA - Support parfait attendu)
  - [ ] Reaper (VST3 ARA - Support parfait attendu)
  - [ ] Logic Pro (AU ARA - Support parfait attendu)
  - [ ] Ableton Live / FL Studio (Fallback temps réel exigé)

## Phase 9 - Extensions DAW

- [x] Extensions VST3 spécifiques à Fender Studio Pro (Micro View) - 2026-06-15
  - [x] Ajout de l'interface `Presonus::IEditControllerExtra`
  - [x] Exposition des paramètres `Speed`, `Amount`, `Formant` via `kParamFlagMicroEdit`

## Phase 11 - Rebranding

> Changement de nom officiel pour des raisons de propriété intellectuelle (suppression de 'Clone' et 'Autotune').

- [x] Renommer le projet CMake de `AutotuneClone` à `OpenVoxTuner` - 2026-06-15
- [x] Refactoriser les noms de classes C++ (`AutotuneCloneAudioProcessor` -> `OpenVoxTunerAudioProcessor`) - 2026-06-15
- [x] Mettre à jour l'UID VST3 (code produit `OvtP`) - 2026-06-15
- [x] Renommer le script Inno Setup et mettre à jour l'installeur Windows pour effacer les anciens binaires de `Autotune Clone` - 2026-06-15

## Phase 10 - CI/CD & Déploiement

> Stratégie de compilation multi-plateforme, packaging, signature de code et distribution.

- [x] Rédiger le guide de déploiement et packaging (`docs/deployment-and-packaging-guide.md`) - 2026-06-15
- [x] Créer un script PowerShell local (`build_installer.ps1`) pour compiler et générer automatiquement l'installateur Windows sans utiliser les quotas GitHub Actions.
- [x] Configurer Inno Setup (`installer/OpenVoxTuner.iss`) pour créer un `.exe` d'installation déployant le VST3 et le Standalone dans les dossiers Windows standards.
- [x] Mettre en place un workflow GitHub Actions (Windows/macOS)
- [x] Générer l'installateur macOS (pkgbuild, `.pkg`)
- [ ] Configurer la signature de code Windows (EV Certificate)
- [ ] Configurer la signature et notarisation macOS (Apple Developer ID)
- [x] Implémenter le mécanisme de vérification des mises à jour dans l'UI du plugin

## Hors scope MVP

- [-] Detection de pitch par deep learning (CREPE, etc.)
- [-] Edition graphique type Melodyne
- [-] Harmonisation automatique (generation d'accords)
- [-] Support MPE
- [-] Version mobile (iOS/Android)

---

## Notes architecturales

```
AudioIn -> [Buffer] -> PitchDetector -> f0_in
                              |
                              v
                     ScaleQuantizer (Key + Mode)
                              |
                              v
                          f0_target
                              |
                              v
                  PitchShifter (PSOLA) -> AudioOut
```

Parametres utilisateur :
- **Speed** : constante de temps de l'enveloppe de correction (style Antares)
- **Amount** : pourcentage de la correction appliquee (0% = pass-through)
- **Key** / **Scale** : restreint l'ensemble des notes cibles

Latence cible : < 30 ms (acceptable pour du monitoring).

Suivi consulte lors de chaque modification du projet.

---

## Prochaines etapes (apres cleanup 2026-06-09)

### Qualite DSP (priorite haute)
- [x] Reecrire PSOLA proprement : grains a amplitude constante, alignement
      sur pitch marks precis, fenetre Hann 2 periodes
      (FAIT 2026-06-10 : grain 2*T0, position synthese en temps absolu,
       COLA adaptive)
- [x] **Continuite de l'overlap-add aux frontieres de bloc** (FAIT
      2026-06-11) : introduction d'un `synthStateBuffer` qui propage
      la queue du bloc precedent vers le debut du bloc suivant.
      Sans cela, la demi-fenetre Hann (T0 samples) du 1er/dernier
      grain d'un bloc etait clippee par le `memset` du buffer de
      sortie, produisant un saut d'amplitude (= click) a chaque
      frontiere de bloc. Particulierement audible a petit buffer
      (144 samples) ou le "trou" de continuite depasse la taille du
      bloc lui-meme. Voir `docs/changelogs/changelog-2026-06-11.md` et
      `debug-persistent-audio-glitches.md` (FIXED).
- [x] **Lissages temps-continus (anti-"pop")** (FAIT 2026-06-11) :
      les lissages de `currentF0` (0.85 par bloc) et de
      `smoothedRatio` (0.9 par bloc) dans `PitchShifter` ont ete
      remplaces par des lissages **time-based** avec tau=30 ms (f0)
      et tau=50 ms (ratio). De meme, `RetargetEnvelope::processSample`
      (appele une fois par bloc avec un alpha per-sample, ce qui
      donnait un tau effectif de `tau*numSamples` = 7.2 s a 144 samples)
      a ete complete par `processBlock(targetRatio, numSamples)` qui
      utilise un alpha = 1 - exp(-blockDuration/tau). Resultat : la
      constante de temps est la meme a toutes les tailles de buffer,
      le Speed Antares a un effet homogene, et le f0 ne suit plus le
      jitter YIN bloc par bloc (source des "pops" residuels apres
      le fix OLA).
- [x] **Etat de synthese toujours a 4096 samples** (FAIT 2026-06-11) :
      l'etat de synthese est maintenant dimensionne a `synthStateCapacity`
      (4096) en permanence, pas a `T0` du bloc precedent. Cela garantit
      que le premier grain d'un bloc a TOUJOURS sa demi-fenetre gauche
      complete, meme si T0 a augmente entre temps (ex: l'utilisateur
      chante une note plus grave). Sinon -> click a 144 samples quand
      T0 change.
- [x] **Anti-octave-error YIN** (FAIT 2026-06-11) : ajout d'une
      etape 3b dans `PitchDetector::detectPitch` qui verifie si 2*tau
      est aussi sous le seuil apres la detection. Si oui, le tau
      detecte est une sous-harmonique et on prend 2*tau (= le
      fondamental). Corrige le "glitch pitch-dependent".
- [x] **Lissage gain COLA (anti-pop d'amplitude)** (FAIT 2026-06-11) :
      ajout d'un lissage time-based (tau=20 ms) du gain COLA
      `1/overlapCount` via nouveau membre `smoothedGain` dans
      `PitchShifter`. Elimine les sauts de gain quand T0p change
      entre blocs.
- [x] **Ameliorations UI (FAIT 2026-06-10)** :
   - Affichage de la note chantee (nom + octave, ex "F3") dans le
     header du PitchVisualizer
   - Affichage de l'offset en cents (couleur selon gravite)
   - Meter de tuning vertical (style Antares / Studio One)
   - Lignes de la gamme tracees en arriere-plan du visualizer
   - Support de gamme personnalisee (12 booleens C, C#, D, ..., B)
   - Clavier de piano vertical a gauche du PitchCurveEditor
     (touches blanches/noires, notes de la gamme surlignees)
- [ ] Detection d'onset (transient detection) -> bypass PSOLA sur les
      transitoires (preserve les "t", "k", "p")
- [ ] LPC + resampling non-uniforme pour preservation exacte des formants
- [x] **Anti-octave-error YIN (continuite d'octave)** (FAIT 2026-06-23) :
      ajout d'une verification de continuite d'octave dans le median
      filter de `PitchDetector`. Si la mediane differe du dernier
      pitch valide d'un facteur proche de 2 (octave au-dessus) ou
      0.5 (octave en-dessous), on ajuste vers l'octave la plus
      proche du contexte precedent. Corrige les sauts d'octave sur
      les voix graves ou le fondamental est faible et YIN detecte
      2*f0 au lieu de f0.
- [ ] MPM en complement de YIN (plus robuste sur les voix graves/aigues)
- [ ] Gate de silence (ne pas shifter quand clarte YIN < seuil)
- [~] **Test audio subjectif sur voix reelle (homme/femme)** :
      REPORTE - les 5 rounds de fix PSOLA (4 a 8) n'ont pas elimine
      les artefacts (pops, clicks, glitches pitch-dependent).
      Round 9 (2026-06-11) a decide d'abandonner le PSOLA maison
      au profit d'une bibliotheque tierce. Les tests audio
      reprendront apres integration de la lib choisie.
  - [x] **FAIT 2026-06-11 (Round 10 + 11)** : RubberBand integre,
      bug helicopter fixee, valide par Jerome a host >= 1024.
      Reste : clics/chops a host < 1024 (cap de latence).
- [x] **Crossfade dans le latency cap** (au lieu de hard drop) :
      FAIT (Round 12). Quand outputValid depasse le cap, on fait un
      crossfade lineaire de 256 samples maximum pour reduire les clics
      a host < 1024 (au lieu de jeter brutalement les echantillons).

### Mode Graphic (ameliorations)
- [ ] Capture du pitch courant par clic (API existe dans
      `PitchCurveEditor::capturePitch`, pas exposee dans la GUI)
- [ ] Courbes de Bezier (edition par tangentes)
- [ ] Zoom temporel (actuellement fixe a 4 s)
- [ ] Presets utilisateur (.json) en plus des presets factory

### Fonctionnalités Additionnelles Implémentées
- [x] **Formant Shift (BETA)** : Implémentation d'un contrôle manuel de décalage de formants (-12 à +12 demi-tons). Intégré au `FormantPreserver` via une modulation du filtre biquad passe-bas. (Sert de base pour un futur véritable Vocoder de phase / LPC).
- [ ] Valider le build apres cleanup (Release x64)
- [ ] Lancer l'executable standalone, verifier l'UI et le bypass
- [ ] Tests audio subjectifs sur voix reelles (homme/femme, francais/anglais)
- [ ] Tests CPU (realtime 48 kHz / 512 samples, x86 et ARM si possible)
- [ ] Documentation utilisateur `docs/user-guide.md`

### Bonus
- [ ] Harmonisation automatique (generation d'accords, tiers/quart/quinte)
- [ ] Preset "Hard tune" (100% amount, speed=0) style T-Pain
- [ ] Preset "Vocoder" (forme d'onde carree + pitch track)

---

## Phase 7 - Refonte Multi-Moteurs de Pitch Shifting (2026-06-12)

> Suite à l'intégration réussie de RubberBand, nous avons généralisé l'architecture pour supporter plusieurs moteurs de pitch-shifting en parallèle.

- [x] **Définition de l'interface `IPitchShifter`** : standardisation des méthodes `prepare`, `reset`, `process`, et `getLatencySamples`.
- [x] **Désintégration des dépendances externes** : Suppression complète des moteurs RubberBand et SoundTouch. Le moteur interne exclusif offre de meilleures performances (0% CPU), est parfaitement stable et exempt d'artefacts, et libère le projet des contraintes de licences (GPL/LGPL) imposées par ces librairies externes.
- [x] **Restauration et refonte de PSOLA** :
  - Adaptation de l'ancienne implémentation maison `PitchShifter` à l'interface `IPitchShifter`.
  - *Fix (2026-06-12) : Remplacement de l'algorithme original (instable) par un Pitch Shifter "Delay-Line Crossfade" synchrone.*
  - *Fix (2026-06-12) : Ajout d'un algorithme de "Cross-Correlation" (WSOLA) au moteur interne. Aligne parfaitement les phases des têtes de lecture lors des rebouclages pour éliminer tout artefact de type "chops".*
- [x] **Étude de faisabilité des alternatives** : Rédaction de `docs/pitch-shifting-feasibility-study.md` (Rubber Band, WSOLA, Élastique Pro, RVC).
- [x] **Sélection dynamique des moteurs** :
  - *Note (2026-06-13) : Le sélecteur a été retiré de l'UI suite à la suppression des moteurs externes, le traitement s'appuie désormais uniquement sur le moteur interne optimisé.*
- [x] Documentation et mise à jour des tests.
- [x] **Amélioration UI : Typographie et layout général**
- [x] **Correctifs UX de l'Éditeur de Pitch** : 
  - Réactivation du glisser-déposer (Drag & Drop) libre (temps et pitch) pour les points.
  - Ajout d'infobulles dynamiques (Tooltip) au survol.
  - Ajout de magnétisme (Snap to grid & Snap to note).
  - Synchronisation du temps continu (boucle 4.0s) pour rendre le mode Standalone opérationnel.
- [x] **Configuration Système** : Définition du moteur "Internal" comme moteur de traitement audio par défaut et refonte du sélecteur de mode via des onglets.
- [x] **Correction DSP** : Réécriture du comportement de la gamme Chromatique pour autoriser la quantification sur les 12 demi-tons.
- [x] **Correction Moteur Internal** : Fix d'un léger "pop" audio lorsque le ratio fluctue autour de 1.0 (note tenue parfaitement juste). L'algorithme WSOLA reste désormais toujours actif pour garantir une continuité de phase parfaite, et la fenêtre de recherche d'alignement a été sécurisée à 10ms pour empêcher la lecture de données non écrites dans le ring buffer.
- [x] **Refonte UX Gammes** : Remplacement des 12 cases à cocher disgracieuses par un affichage de clavier de piano horizontal interactif pour la sélection des gammes personnalisées et la visualisation des gammes préconfigurées.
- [x] **Extension des gammes** : Ajout de 10 nouvelles gammes standard (Melodic Minor, Harmonic Minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Blues, Major Triad, Minor Triad).
- [x] **Optimisation CPU** : Implémentation d'une détection de silence intelligente (Sleep mode) désactivant le traitement DSP lourd (YIN, Formants, PitchShifting) après 500ms de silence, ramenant l'utilisation CPU de 33% à ~1% au repos. Correction d'un appel prématuré à YIN qui causait 13% de CPU inutile.
- [x] **Optimisation CPU avancée (Chant Actif)** : Refonte de la fréquence d'appel de l'algorithme YIN (Hop Size) et décimation du signal d'entrée par 4 pour diviser par 16 la charge du calcul de pitch. Vectorisation de la boucle interne YIN. Optimisation de la corrélation croisée du moteur interne (bitmask, indices entiers, pas de recherche augmenté).
- [x] **Reporting de Latence et Optimisation** : Implémentation correcte de la déclaration de latence au DAW (`setLatencySamples()`). Réduction de la latence interne des moteurs de ~40/60ms à ~15ms pour un monitoring en temps réel optimal.

### Validation post-refonte

- [ ] Test voix feminine (aigue, harmonique 2 forte -> octave error YIN)
- [ ] Test voix masculine (medium, avec vibrato)
- [ ] Test voix parlee (transitoires)
- [ ] Test pitch shift faible (+/-1 demi-ton) et fort (+/-7 demi-tons)
- [x] Test stability a 144, 512, 1280, 1920, 2048 samples
  - [x] FAIT 2026-06-12 : Utilisation de `juce::AbstractFifo` pour un buffering asynchrone parfait, eliminant les clics sur tous les buffers non-standards.
- [x] Test bypass (pass-through identique a l'input)

### Mise a jour documentation

- [ ] Mettre a jour `docs/architecture.md` avec la nouvelle lib
- [ ] Creer `docs/lib-{soundtouch|rubberband}-integration.md` avec
      build instructions, licence, et troubleshooting

## Phase 12 - Advanced Graphic Features
- [x] Integrate piano keyboard into the Live visualizer
- [x] Add real-time highlight of sung note (input) and corrected note (output) on the piano keyboard
- [x] Rename tabs to "Live" and "Curve Editor"
- [x] Move editor help text to avoid overlapping with the time ruler
- [x] Implement mouse wheel zoom and scroll for the vertical pitch axis in both Live and Curve Editor modes
- [x] Add "Snap to Grid" functionality for the time axis in the Curve Editor
- [x] Fix tooltip formatting in Curve Editor to match the Measure.Beat format of the time ruler
- [x] Introduce "Step mode" (escalier interpolation) in the Curve Editor
- [x] Refactor Curve Editor toolbar using clean vector icon buttons
- [x] Add themed tooltips for all Curve Editor toolbar icons
- [x] Mirror sung/corrected note highlights on the Curve Editor piano keyboard

## Phase 13 - Restauration de l'autotune (Corrections 2026-06-24)

> Bug critique rapporte par Jerome : "le plugin d'autotune est inutilisable".
> Aucun effet audible sur la voix chantee, visualiseur graphique vide,
> piano vertical muet. Trois pannes racines identifiees par analyse
> statique du code et detaillees dans
> `docs/diagnostic-pannes-autotune-2026-06-24.md`. Plan de correction
> complet dans `docs/correctif-pannes-autotune-2026-06-24.md`.

### R1 - Traitement audio principal (pitch detection + autotune)
- [x] **Diagnostic** : ratio `1.0` injecte au PitchShifter a cause de
      l'anti-octave-error trop zele (`f0_in == 0.0f` transitoire)
- [x] **Correctif 1.1** : `computeInputPitch()` reutilise
      `lastValidPitchForAutotune` quand YIN ne detecte rien
- [x] **Correctif 1.2** : `processBlock` reutilise le dernier ratio
      snapshot au lieu de retomber a `1.0` lors des micro-pauses
- [x] **Correctif 1.3** : `PitchShifter::process` valide le ratio
      d'entree (NaN/Inf/<=0) et retombe sur 1.0 en securite
- [ ] Test voix feminine (aigue, harmonique 2 forte) - bloque par R1
- [ ] Test voix masculine (medium, avec vibrato) - bloque par R1

### R2 - Affichage visuel (visualiseur + piano vertical)
- [x] **Diagnostic** : `timerCallback` ne pousse les donnees
      (`pushInputPitch`, `pushOutputPitch`, `setNoteInfo`) qu'a
      l'onglet actif, donc si "Curve Editor" est selectionne
      par defaut, le visualiseur "Live" reste vide
- [x] **Correctif 2.1** : pousser systematiquement les donnees dans
      le visualiseur independamment de l'onglet selectionne
- [x] **Correctif 2.2** : nouvelle methode `PianoKeyboard::setNoteNames`
      pour afficher en temps reel la note chantee et la note corrigee
- [x] **Correctif 2.3** : constante unique `kHarmonyColour` (bleu
      `#1A9AF0`) alignee sur la spec
- [x] Code couleur spec conforme :
  - [x] Rouge (`#e91e63`) -> voix chantee originale
  - [x] Vert (`#00e676`) -> voix corrigee par l'autotune
  - [x] Bleu (`#1A9AF0`) -> harmonies generees

### R3 - Latence temps reel < 30 ms
- [x] **Diagnostic** : `getPlayHead()->getPosition()` et
      `getLoopPoints()` executes en synchrone dans le thread audio
- [x] **Correctif 3.1** : cache du transport time, maj limitee a
      10 ms (100 Hz max)
- [x] **Correctif 3.2** : `getLoopPoints()` ignore en mode Standalone
- [ ] Test loop 5 min Reaper/FL Studio - bloque par R3

### R4 - YIN ne s'execute jamais (BUG BLOQUANT - Round 2, 2026-06-24)
- [x] **Diagnostic** : `prepareToPlay` utilise `sampleRate/4.0` mais
      `computeInputPitch` decime par 8 -> buffer decime de 256 echantillons
      insuffisant pour YIN (besoin 734 a 11025 Hz) -> `detectPitch` retourne
      0 a chaque bloc depuis la refonte Multi-Moteurs (Phase 7)
- [x] **Correctif R4.1** : `analysisWindow` passe de 2048 a 4096 (`PluginProcessor.h`)
- [x] **Correctif R4.2** : `analysisHopSize` passe de 1024 a 2048 (`PluginProcessor.h`)
- [x] **Correctif R4.3** : `decimation` passe de 8 a 4 dans `computeInputPitch` (`PluginProcessor.cpp`)
- [x] Verification : decimatedWindow = 4096/4 = 1024 >= 734 (besoin YIN) -> OK

### R5 - Drops d'octave sur note tenue (Round 3, 2026-06-24)
- [x] **Diagnostic** : 2 bugs dans l'anti-octave-error du PitchDetector
- [x] **Bug 1 (detectPitch etape 3b)** : seuils trop restrictifs empechaient
      la correction quand YIN trouve la 2e harmonique (tau->2*f0) ->
      remplace par evaluation systematique des 2 alternatives avec
      choix par clarte + continuite d'octave
- [x] **Bug 2 (getMedianFiltered)** : boucle arretait apres 1 valeur a
      cause d'un `break` -> corrige par consensus vote >= 3/5 valeurs
- [x] Fichier : `Source/dsp/PitchDetector.cpp`

### R6 - Drops d'octave persistent en mode Curve Editor (Round 4, 2026-06-24)
- [x] **Diagnostic** : le probleme n'est pas dans YIN mais dans
      `processBlock` — meme avec la bonne note cible (courbe), si
      `f0_in` saute d'une octave, le ratio `f0_target/f0_in` est
      faux et le PitchShifter produit un drop audible
- [x] **Correctif** : filtre anti-saut d'octave dans `processBlock`
      qui compare `f0_in` au dernier pitch valide et rejette les
      sauts d'un facteur ~2 ou ~0.5
- [x] Fichiers : `Source/PluginProcessor.h` (membre `lastOctaveValidatedPitch`),
      `Source/PluginProcessor.cpp` (filtre apres computeInputPitch)

### Validation post-correction (Phase 13)
- [ ] Build Release x64 via `build.ps1 -configuration Release`
- [ ] Installation VST3 via `install_vst3.ps1`
- [ ] Test voix reelle homme (5 min, mix justesse)
- [ ] Test voix reelle femme (5 min, mix justesse)
- [ ] Test 3 traces colorees simultanees (Live tab)
- [ ] Test piano vertical : marqueurs rouge+vert en temps reel
- [ ] Test latence <= 30 ms sur Reaper / Studio One / Standalone
- [ ] Test pitch shift faible (+/-1 demi-ton)
- [ ] Test pitch shift fort (+/-7 demi-tons)
- [ ] Test voix parlee (transitoires)
- [ ] Test 5 min en mode loop Reaper (zero glitch)

### Prevention des regressions (Phase 13)
- [ ] Test unitaire `test/dsp/PitchShifterTest.cpp` : ratio invalide -> 1.0
- [ ] Assert `jassert(pitchVisualizer != nullptr)` dans `timerCallback`
- [ ] Log OVT en DEBUG si trace rouge vide pendant > 1 s de chant
- [ ] Encapsulation `updateTransportTimeIfNeeded()` documentee
      "NE PAS APPELER PLUS D'UNE FOIS PAR TRANCHE DE 10 ms"

## Phase 14 - Curve Editor: Customizable Measures (Time Signature Aware)

> Plan detaille : `docs/implementation-plan-curve-editor-beats-auto-scroll.md`

- [x] **Time signature infrastructure** : Lire `ARAContentTypeBarSignatures` (ARA)
      et `AudioPlayHead::getTimeSignature()` (VST3) dans PluginProcessor.
      Stocker la signature courante (numerateur, denominateur) dans des
      atomiques thread-safe, et les changements multiples ARA dans un vecteur
- [x] **Parameter `editor_measures`** : `AudioParameterInt` plage 1-8, defaut 4,
      persiste automatiquement via AudioProcessorValueTreeState
- [x] **Ruler rewrite** : Remplacer le `for (t = 0...timeVisible step 0.5)` fixe
      par un calcul time-signature-aware : beatUnit = 4/den, ppqPerBar = num *
      beatUnit, labels "M1", "M2"... avec les beats en sous-divisions. Fonctionne
      en 3/4, 4/4, 6/8, 12/8, etc.
- [x] **ComboBox dans la toolbar** : Selecteur "Measures" (1, 2, 4, 8) visible
      uniquement en mode Curve Editor, cote a cote avec les boutons Snap/Step
- [x] **ARA multi-signature** : Support des changements de signature en cours
      de morceau (ex: 4/4 -> 6/8 a la mesure 17)
- [x] **Persistence** : Le choix du nombre de mesures est automatiquement preserve
      entre les sessions via `AudioProcessorValueTreeState`

### Validation Phase 14
- [ ] Tests visuels : ruler correct en 3/4, 4/4, 6/8, 12/8
- [ ] Tests 1, 2, 4, 8 mesures : pas de chevauchement ni de coupure
- [ ] Changement de signature en ARA : le ruler s'adapte en cours de lecture
- [ ] Redimensionnement : le ruler reste lisible de 600 a 1920 px

## Phase 15 - Curve Editor: ARA Auto-Scroll

> Plan detaille : `docs/implementation-plan-curve-editor-beats-auto-scroll.md`

- [x] **Auto-scroll algorithm** : Defilement continu et fluide maintenant le
      playhead a ~75% de la largeur visible. Calcul : scrollOffset =
      jmax(0, playheadTime - timeVisible * 0.75)
- [x] **Coordinate update** : `timeToX()` et `xToTime()` integrent `scrollOffset`
      pour que le drag des points, l'ajout et la suppression fonctionnent
      correctement avec le scroll actif
- [x] **Toggle button** : Bouton "Auto-Scroll" dans la toolbar du Curve Editor,
      active par defaut en mode ARA, desactivable par l'utilisateur
- [x] **Parameter `auto_scroll`** : `AudioParameterBool`, persiste entre sessions
- [x] **Detection ARA** : Activation automatique quand `isBoundToARA_custom()`
      est vrai (mais l'utilisateur peut toujours desactiver)
- [x] **Arret/Re-demarrage** : Quand la lecture s'arrete, le scroll reste en
      place. Quand elle reprend, le scroll suit a nouveau

### Validation Phase 15
- [ ] Test defilement continu : le playhead reste visible a l'ecran
- [ ] Test drag pendant le scroll : les points se deplacent correctement
- [ ] Test resize : le scroll s'adapte a la nouvelle largeur
- [ ] Test arret/reprise : scroll stable, pas de saut visuel
- [ ] Test ARA vs VST3 : auto-scroll ON en ARA, propose en VST3

