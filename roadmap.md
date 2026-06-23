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
- [x] Initialiser `changelog-2026-06-09.md`
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
- [ ] Gestion des transitions douces (smoothing des parametres) - speed pas encore utilise

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
      bloc lui-meme. Voir `changelog-2026-06-11.md` et
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

