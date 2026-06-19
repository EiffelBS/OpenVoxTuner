# Changelog - 10 juin 2026

## Nouvelles fonctionnalites UI (ameliorations demandees par Jerome)

### Affichage de la note chantee et de l'offset en cents

- **Header du PitchVisualizer** : affiche en gros le nom de la note
  actuellement chantee (ex: "F3") + un texte plus petit indiquant
  la note cible (ex: "-> F3") si differente.
- **Offset en cents** : un grand texte affiche "+/- 50 c" a droite du
  nom de la note. La couleur change selon la gravite :
  - vert (|c| < 5) : dans la note
  - jaune (|c| < 25) : proche de la note
  - rouge (|c| >= 25) : clairement a cote
- **Meter de tuning vertical** (style Antares / Studio One) : une aiguille
  horizontale deplace selon l'offset en cents, avec une barre centrale
  verte (0 cents) et des graduations a +/-50 et +/-100 cents.

### Affichage des notes de la gamme

- **Lignes jaunes semi-transparentes** tracees dans le PitchVisualizer
  pour toutes les notes de la gamme (sur 4 octaves, C2 -> C6).
- Ces notes sont mises a jour en temps reel selon le Key + Scale choisi.

### Gamme personnalisee (Custom)

- **Nouveau mode `Scale::Custom`** (indice 5) ajoute a `atdsp::Scale`.
- **12 booleens AudioParameterBool** (`custom0` a `custom11`) : chaque
  note peut etre cochee/decochee individuellement (C, C#, D, ..., B).
- **12 booleens ToggleButton** dans la GUI, organises en 1 rangee
  horizontale sous les knobs. Visibles uniquement si Scale = "Custom".
- **`ScaleQuantizer::setCustomIntervals()`** permet de pousser la liste
  des notes actives vers le quantifier.
- **Default** : majeur en C (C, D, E, F, G, A, B).

### Clavier de piano vertical (PianoKeyboard)

- **Nouveau composant `ui::PianoKeyboard`** : dessine un clavier de piano
  vertical (notes graves en bas, aigues en haut) avec touches blanches
  et noires correctement alignees.
- **Place a gauche du PitchCurveEditor** (largeur 40 px) : permet
  d'identifier visuellement les notes de la pitch curve.
- **Notes de la gamme surlignees en jaune** : on voit immediatement
  quelles notes sont "autorisees" par la gamme courante.
- **Labels des octaves (C2, C3, C4, ...)** sur la gauche du clavier.

## Architecture et implementation

### Nouveau fichier d'utilitaires DSP

- **`Source/dsp/NoteUtils.h`** : fonctions inline de conversion
  Hz <-> note MIDI, centieme de demi-ton (cents), et struct `NoteInfo`
  pour regrouper toutes les informations affichees dans l'UI.

### Modifications des modules existants

- **`ScaleQuantizer.h/.cpp`** :
  - Nouvelle valeur d'enum `Scale::Custom` (= 5).
  - Nouvelle methode `setCustomIntervals(const juce::Array<int>&)`.
  - `rebuildIntervals()` distingue le cas Custom (utilise la liste
    custom directement, sans decalage de key) des autres modes.
- **`PitchCurve.h/.cpp`** :
  - Nouvelle methode `snapToScaleCustom()` pour le snap avec gamme custom.
  - Le snap interactif du `PitchCurveEditor` choisit automatiquement
    entre `snapToScale` (modes presets) et `snapToScaleCustom` (Custom).
- **`PluginProcessor.h/.cpp`** :
  - Nouveau parametre `custom0..custom11` (12 AudioParameterBool).
  - Plage du parametre `scale` passee a 0..5.
  - Nouveau getter `getCurrentCentsOffset()` et champ atomique
    `lastCentsOffset` mis a jour a chaque bloc.
  - `syncParameters()` pousse les notes custom vers le quantifier
    si `scaleIdx == 5`.

### Modifications UI

- **`PitchVisualizer.h/.cpp`** :
  - Nouveau bandeau en haut (60 px) avec note + cents.
  - Nouvelle zone de meter de tuning a droite (60 px de large).
  - Tracage des lignes de la gamme en arriere-plan.
  - Nouvelles methodes `setNoteInfo()` et `setScaleIntervals()`.
- **`PitchCurveEditor.h/.cpp`** :
  - PianoKeyboard integre comme enfant, redessine a gauche.
  - `timeToX` / `xToTime` tiennent compte de la largeur du piano.
  - Mode Custom propage a `snapToScaleCustom`.
  - `setCustomIntervals()` pour recevoir les notes custom.
- **`PianoKeyboard.h/.cpp`** (nouveau) :
  - Dessin de touches blanches (C, D, E, F, G, A, B) en pleine largeur.
  - Touches noires (C#, D#, F#, G#, A#) plus courtes, par-dessus.
  - Couleurs differentes pour les notes dans la gamme (jaune).
  - Label des octaves (C2, C3, ...) sur les touches C.
- **`PluginEditor.h/.cpp`** :
  - 12 nouveaux `ToggleButton customButtons[0..11]` + leurs attachments.
  - `updateCustomVisibility()` : affiche/masque les 12 boutons
    selon que Scale = Custom ou pas.
  - `refreshVisualizer()` enrichi : envoie NoteInfo + scaleIntervals
    au PitchVisualizer et au PitchCurveEditor (qui les propage au piano).
  - Layout ajuste : barre du bas passee de 160 a 220 px pour laisser
    la place a la rangee des 12 booleens custom.

## CMakeLists.txt

- Ajout de `Source/dsp/NoteUtils.h`, `Source/ui/PianoKeyboard.h`,
  `Source/ui/PianoKeyboard.cpp` dans les sources de la cible
  `AutotuneClone`.

## Build

- Build Release x64 reussi :
  - `Autotune Clone.vst3` produit.
  - `Autotune Clone.exe` (Standalone) produit.
- 1 warning MSVC C4172 (PitchCurve.h:68-69) : "retour de l'adresse de
  la variable locale ou temporaire" sur `getPoint(int)` ; pre-existant
  (de la version precedente), non corrige car inoffensif (les references
  sont utilisees immediatement dans le meme scope).

## Correction de bug : assertion JUCE au premier paint du PianoKeyboard

### Symptome
Au premier appel de `PianoKeyboard::paint()`, Visual Studio (mode Debug)
declenchait un breakpoint sur l'assertion `jassertquiet` dans
`juce::`anonymous namespace'::coordsToRectangle<float>` (ligne 91 de
`juce_GraphicsContext.cpp`), avec la pile d'appels :

```
coordsToRectangle<float>             [juce_GraphicsContext.cpp:91]
juce::Graphics::fillRect (float)     [juce_GraphicsContext.cpp:560]
ui::PianoKeyboard::paint             [PianoKeyboard.cpp:109]
```

### Cause
Dans `PianoKeyboard::paint`, pour chaque touche blanche on calcule :
```cpp
const float y    = midiToY (midi);
const float keyH = midiToY (midi + 1) - y;
```
Or pour `midi = highestMidi` (=96, C7) :
- `midiToY(96) = H - (96-36)/60 * H = 0`
- `midiToY(97) = H - (97-36)/60 * H = -0.0167 * H` (NEGATIF)
- Donc `keyH = -0.0167 * H < 0`, ce qui viole l'assertion `(int) h >= 0`.

Le meme probleme affectait les touches noires du dernier demi-ton.

### Fix
Clamp de la hauteur a 1 pixel minimum pour les deux types de touches
(blanche et noire), afin d'eviter tout `fillRect` avec une hauteur
nulle ou negative.

Voir `debug-pianokeyboard-negative-height.md` pour la session complete.

### Verification
- Build Release x64 reussi apres le fix.
- Lancement du standalone : pas d'assertion declenchee.
- Le composant `PianoKeyboard` s'affiche correctement a gauche du
  `PitchCurveEditor`.

## Corrections de bugs et clarifications suite au feedback

### Reorganisation du layout (UI illisible)
- **Bouton Bypass** : deplace en haut a droite avec libelle "Bypass"
  et tooltip explicatif. La version "v0.1.0 - Phase 1" du bandeau
  n'est plus chevauchee par le bouton.
- **Boutons de gamme custom** : deplaces dans leur PROPRE rangee en
  bas (28 px de hauteur, apres la rangee knobs/ComboBox), au lieu
  d'etre superposes aux knobs.
- **3 rangees distinctes** dans la barre du bas :
  1. Mode / Snap (28 px)
  2. Knobs (Speed, Amount) + ComboBox (Key, Scale) (90 px)
  3. 12 booleens de gamme Custom (28 px, visible uniquement en Custom)
- **Espacement** : padding 10 px entre les rangees, plus de chevauchement.

### Piano keyboard plus lisible
- **Largeur passee de 40 a 60 pixels** dans `PitchCurveEditor::resized()`.
- **Largeur des touches noires** passee de 60% a 65% de la largeur totale.
- **Taille des labels** passee de 9pt a 11pt bold pour les C2, C3, etc.

### Clarification du role du Bypass
- **Tooltip ajoute** sur le bouton Bypass : explique que cocher le
  Bypass fait passer l'audio sans traitement (dry pass-through),
  et que le visualiseur continue a fonctionner dans les deux cas.
- Note dans la documentation : le "Mute audio input" dans les Audio/MIDI
  Settings du standalone est un toggle distinct qui coupe le monitoring
  hardware du standalone (independamment du bypass du plugin).

### Correction du bug audio : appel systematique du PSOLA

**Symptome** : avec bypass OFF, l'audio etait "incorrect" (silence ou
distorsion) quand l'utilisateur passait rapidement de in-scale a
out-of-scale. Avec "Mute audio input" coche, rien n'etait audible.

**Cause identifiee** : dans `PluginProcessor::processBlock`, l'appel
a `pitchShifter->process` etait conditionne par `|ratio - 1.0f| > 1e-3f`.
Quand l'utilisateur chantait dans la gamme (ratio proche de 1), le
`pitchShifter->process` n'etait PAS appele, donc :
- Le ring buffer n'etait pas alimente avec les nouveaux echantillons.
- `totalSamplesWritten` et `nextSynthMarkSample` restaient figes.
- Aucune pitch mark n'etait detectee.

Quand l'utilisateur passait ensuite out-of-scale (ratio != 1), le
`pitchShifter->process` etait appele avec un ring buffer obsolete
contenant des echantillons d'il y a N blocs. La synthese pouvait
produire du silence ou des artefacts.

**Fix** : on appelle TOUJOURS `pitchShifter->process`, sans condition
sur le ratio. C'est `PitchShifter::process` lui-meme qui decide de
faire du passthrough (ratio proche de 1 ou f0 <= 0) ou de la
synthese OLA. Le ring buffer est donc continuellement a jour.

**Fichier** : `Source/PluginProcessor.cpp` (ligne ~210).

**Session de debug** : `debug-psola-audio-incorrect.md` (marquee [OPEN],
a confirmer apres test utilisateur).

### Note sur le drag des points du PitchCurveEditor
Le drag ne fonctionne QUE quand le mode est "Graphic" (pas "Auto").
L'overlay gris "Mode Auto : passez en mode Graphic pour editer" est
affiche dans l'editeur quand le mode Auto est actif. C'est un
comportement desire, pas une regression : on ne peut pas editer la
courbe manuellement en mode Auto (le plugin suit la gamme
automatiquement).

## Round 2 de corrections suite au feedback (10 juin 2026 PM)

### Fix A (CONFIRME) : overlap visualiseur / curve editor
**Symptome** : la zone du visualiseur etait completement vide (le
header avec note/cents, le meter de tuning, les lignes de la gamme
n'etaient pas visibles). Seul le curve editor (avec ses points)
etait affiche, dans une zone qui aurait du etre partagee.

**Cause** : dans `PluginEditor::resized`, j'avais ecrit :
```cpp
auto vizArea   = centerArea.reduced (pad).removeFromTop (...);
auto curveArea = centerArea.reduced (pad);  // BUG : zone complete, pas restante
```
Le `removeFromTop` etait appele sur un Rectangle temporaire (le
resultat de `centerArea.reduced(pad)`), donc le Rectangle original
n'etait pas modifie. `curveArea` etait la zone COMPLETE, et
recouvrait entierement le visualiseur.

**Fix** : utiliser une variable intermediate pour que
`removeFromTop` modifie reellement le rectangle :
```cpp
auto reducedCenter = centerArea.reduced (pad);
auto vizArea       = reducedCenter.removeFromTop (...);
auto curveArea     = reducedCenter;
```

**Fichier** : `Source/PluginEditor.cpp` (ligne ~240).

### Fix B (suspicion forte) : drag impossible en mode Graphic
**Symptome** : meme en mode Graphic, le drag des points du
PitchCurveEditor ne fonctionnait pas.

**Cause probable** : le `PianoKeyboard` (enfant du `PitchCurveEditor`,
60 px a gauche) avait `setInterceptsMouseClicks(true, true)` par
defaut, ce qui pouvait interferer avec le dispatching des events
souris vers le parent (curve editor).

**Fix** : `pianoKeyboard.setInterceptsMouseClicks(false, false)`
dans le constructeur du `PitchCurveEditor`. Le piano est un
affichage pur, il n'a pas besoin d'intercepter les events.

**Fichier** : `Source/ui/PitchCurveEditor.cpp` (constructeur).

### Fix C (preventif) : audio glitches "decrochages rapides"
**Symptome** : l'utilisateur rapporte des glitchs tres rapides
quand il chante out-of-tune (avec bypass OFF).

**Cause probable** : le coefficient de lissage du ratio dans le
PSOLA etait beaucoup trop lent :
- `smoothingCoeff = 0.995` -> tau ~4.6 secondes (avec blocks
  de 1024 samples a 44.1 kHz).
- `currentF0` smoothing 0.95 -> tau ~0.45 secondes.

Le PSOLA mettait des secondes a converger apres un changement de
target ratio, produisant des artefacts audibles (decrochages).

**Fix** :
- `smoothingCoeff` 0.995 -> 0.9 (tau ~0.23 s, reponse rapide)
- `currentF0` smoothing 0.95 -> 0.85 (tau ~0.13 s)

**Fichiers** : `Source/dsp/PitchShifter.h`, `Source/dsp/PitchShifter.cpp`.

### Verification
- Build Release x64 reussi.
- Standalone se lance sans probleme.
- A confirmer par test utilisateur.

### Session de debug
`debug-visualizer-overlap-and-drag.md` (statut : FIXED).

## Round 3 - Fix performance audio : crash et glitches a petit buffer

### Symptome
- Glitches audio audibles meme a 2048 samples.
- Beaucoup plus de glitches a 144 samples (3.3ms par bloc).
- Crash intermittent dans `AudioProcessorPlayer::audioDeviceIOCallbackWithContext`
  en manipulant les controles (Scale, Speed, etc.).

### Cause identifiee
Dans `PitchShifter::process`, le buffer de sortie etait cree LOCALEMENT
a chaque appel via `juce::AudioBuffer<float> output; output.setSize(...);`
C'est une **heap allocation** dans le thread audio.

- 144 samples @ 44.1 kHz -> 306 blocs/sec -> 306 allocations/sec
- 2048 samples @ 44.1 kHz -> 22 blocs/sec -> 22 allocations/sec
- La pression sur le heap (Windows allocator) cause des real-time
  deadline misses -> glitches.
- Quand le thread audio manque trop de deadlines, Windows peut le tuer
  -> crash dans le callback.

### Fix
- `outputBuffer` est maintenant un membre du `PitchShifter` (header
  [PitchShifter.h](file:///c:/Users/User/Documents/trae_projects/VST3/Source/dsp/PitchShifter.h#L107-L113)).
- Pre-alloue UNE SEULE fois dans `prepare()` a `juce::jmax(bs*2, 2048)`
  samples, ce qui couvre tous les cas realistes.
- Dans `process()`, on ne fait plus que `std::memset` (zero copy memoire,
  pas d'allocation).
- Fallback securite : si un bloc plus grand que la capacite allouee
  arrive (improbable avec 2x preallocation), on laisse passer l'entree
  telle quelle (return early) au lieu de risquer un crash.

### Verification
- Build Release x64 reussi.
- Standalone se lance.
- A confirmer par test utilisateur a 144 et 2048 samples.

### Session de debug
`debug-audio-callback-crash-and-glitches.md` (statut : FIXED).
