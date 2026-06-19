# Architecture du plugin Autotune Clone

## Vue d'ensemble

Le plugin est un **effet audio** (pas un instrument) : il recoit un signal audio
mono ou stereo et le renvoie transpose en pitch selon une gamme musicale choisie.

Il est implemente en **C++** avec le framework **JUCE 8**, qui fournit :
- l'interface `AudioProcessor` (le pipeline audio) ;
- l'interface `AudioProcessorEditor` (la GUI) ;
- l'integration VST3/Standalone/AU/AAX ;
- les outils DSP (FFT, fenetrage, smoothing).

## Arborescence des sources

```
Source/
  PluginProcessor.h / .cpp        # Pipeline audio (point d'entree DSP)
  PluginEditor.h / .cpp          # GUI (point d'entree visuel)
  dsp/
    PitchDetector.h / .cpp        # Module 1 : detection de pitch (YIN)
    ScaleQuantizer.h / .cpp       # Module 2 : quantification tonique+mode
    PitchShifter.h / .cpp         # Module 3 : transposition (PSOLA)
    NoteUtils.h                   # Utilitaires Hz <-> MIDI <-> nom de note
  ui/
    PitchVisualizer.h / .cpp      # Composant GUI : courbes de pitch
                                  # + note chantee + cents + meter
    PitchCurveEditor.h / .cpp     # Editeur interactif de pitch curve
    PianoKeyboard.h / .cpp        # Clavier de piano vertical (gauche)
```

## Pipeline DSP

```
                   +-------------------+   f0_in
   Audio In  ----> |  PitchDetector    | ------------+
                   +-------------------+             |
                                                    v
                                          +----------------------+
                                          |   ScaleQuantizer     |
                                          |   (key, scale)       |
                                          +----------------------+
                                                    |
                                                    v  f0_target
                                          +----------------------+
   Audio Out <--- |    PitchShifter     | <----------------------+
                   |    (PSOLA)         |   ratio = f0_target/f0_in
                   +--------------------+
```

## Algorithmes utilises

### Phase 1 - MVP fonctionnel

- **Detection de pitch** : algorithme YIN (de Cheveigne & Kawahara, 2002)
  - Difference function
  - Fonction d'auto-correlation cumulee normalisee
  - Seuil de clarte (0.10-0.15)
  - Parabolic interpolation pour la precision sub-sample

- **Quantification** : projection sur la gamme la plus proche
  - Conversion Hz -> demi-tons (par rapport a A4 = 440 Hz)
  - Recherche du demi-ton le plus proche appartenant a la gamme
  - Conversion inverse vers Hz

- **Pitch shifting** : PSOLA simplifie
  - Detection des marques de pitch (zero-crossings du pitch detecte)
  - Decoupage en grains fenetres (Hann, 2-4 periodes fondamentales)
  - Overlap-add avec les positions recalculees selon le ratio
  - Phase vocoder simplifie pour la composante stationnaire (Phase 4)

### Phase 4 - Qualite

#### PSOLA (Phase 4 implementation)

L'algorithme PSOLA (Pitch-Synchronous Overlap-Add) est la technique standard
pour transposer des signaux quasi-periodiques comme la voix sans modifier
la duree.

**Pipeline** :
1. **Detection de f0** : appel du `PitchDetector` (YIN) sur le buffer d'entree.
2. **Detection des pitch marks** : pour chaque periode fondamentale (echantillon
   `period = sr / f0`), on cherche le maximum local d'amplitude dans une
   fenetre de +/- 25% de la periode. Cela fixe la "phase" de chaque grain.
3. **Grain PSOLA** : pour chaque pitch mark d'analyse, on extrait un grain de
   2 periodes fenetre par Hann, centre sur la mark.
4. **Re-positionnement** : on depose ce grain a la position de synthese
   (`synthMark = m * synthPeriod`, ou `synthPeriod = period / ratio`).
5. **Overlap-Add (OLA)** : on additionne les grains dans le buffer de sortie
   avec une fenetre Hann 2-periodes / hop-1-periode, ce qui satisfait la
   condition COLA (Constant OverLap-Add factor = 1 sur les zones stationnaires).

**Notes** :
- Le grain fait 2 periodes pour eviter les coupures brutales.
- La fenetre Hann est normalisee par /2 (gain d'overlap theorique).
- L'algorithme est en O(N) ou N est le nombre de pitch marks.

#### Compensation de formants (Phase 4)

Quand on transpose un signal vocal par PSOLA, les formants (resonances du
conduit vocal) sont deplaces avec le pitch. Cela donne un effet "chipmunk"
non naturel (la voix devient plus "fine" quand on monte).

**Solution implementee** : technique "LP-filter + resample" simplifiee.
- **Etude du probleme** : les formants se trouvent dans la partie haute du
  spectre. En montant le pitch, on les deplace vers le haut en valeur absolue
  mais leur position relative par rapport a f0 change.
- **Solution** : avant le PSOLA, on applique un filtre passe-bas Butterworth
  2nd ordre dont la frequence de coupure est inversement proportionnelle au
  ratio de transposition (compensation par `sqrt(ratio)`).
- **Pourquoi sqrt** : un simple `1/ratio` serait trop agressif ; un simple
  `1.0` ne corrige rien. Le compromis `sqrt` donne un resultat naturel.
- **Limites** : on ne preserve pas exactement les formants, on les deforme
  de maniere plausible. Pour une preservation exacte, il faudrait un modele
  de prediction lineaire (LPC) et resampling non-uniforme.

#### Retarget Envelope (style Antares "Speed")

Le parametre `Speed` controle la rapidite avec laquelle le pitch suit la
note cible :
- `Speed = 0 ms` : correction instantanee (effet "robotique" type T-Pain)
- `Speed = 50 ms` : correction rapide mais douce (defaut Antares)
- `Speed = 200 ms` : correction lente et tres naturelle (presque pas d'effet)

**Implementation** : un filtre IIR 1er ordre (exponential smoothing) :
```
y[n] = y[n-1] + alpha * (target - y[n-1])
alpha = 1 - exp(-dt / tau)   ou  tau = speedMs / 1000
```

Cela donne une reponse exponentielle de constante de temps `tau` :
- Apres `tau` : 63% de la cible atteinte
- Apres `3*tau` : 95%
- Apres `5*tau` : 99%

### Phase future

- Detection de pitch par MPM (McLeod Pitch Method) en complement de YIN
- Preservation exacte des formants par LPC + resampling non-uniforme
- Preservation des transitoires (detection onset -> bypass du PSOLA)
- Mode "graphique" editable de la pitch curve (style Melodyne) : VOIR CI-DESSOUS

## Mode "Graphic" (Phase 4 - Style Melodyne)

Le mode Graphic permet a l'utilisateur de **dessiner la pitch curve ideale**
que l'audio doit suivre au fil du temps. C'est ce qui distingue un Auto-Tune
Pro d'un plugin basique.

### Mode Auto vs Mode Graphic

| Mode    | Source du pitch cible                                    | Usage                       |
|---------|----------------------------------------------------------|-----------------------------|
| Auto    | Quantification automatique vers la gamme (Key/Scale)     | Chant en direct, rapide     |
| Graphic | Pitch curve dessinee a la souris (points + interpolation)| Mixage offline, perfection  |

L'utilisateur bascule entre les deux via la ComboBox "Mode" dans la GUI.
Le mode est sauvegarde dans l'etat du plugin.

### PitchCurve : structure de donnees

`atdsp::PitchCurve` est une liste triee de `PitchPoint { time (s), pitch (Hz) }`.
- Minimum 0 point : mode auto par defaut.
- 1 point : valeur constante (la pitch curve tient cette valeur).
- N points : interpolation lineaire entre 2 points consecutifs.
- Avant le 1er point / apres le dernier : on tient la valeur de l'extremite.
- L'evaluation se fait par recherche dichotomique (O(log N)).

### Edition interactive (`ui::PitchCurveEditor`)

Le composant `PitchCurveEditor` permet de :
- **Drag** d'un point : deplace le point verticalement (change le pitch).
- **Double-clic** : ajoute un point a la position du curseur.
- **Clic droit sur un point** (ou Alt+clic) : supprime le point.
- **Clic droit dans le vide** : menu preset (default, spoken, lyric, rap, robot).
- **Snap to scale** : si active, les points sont arrondis a la note la plus
  proche de la gamme courante (Key/Scale sliders).

Le composant est connecte au processor via un pattern Listener : a chaque
modification, `pitchCurveChanged()` est appele, et l'editeur copie la
courbe vers `processorRef.getPitchCurve()`.

### Presets factory

| Preset     | Description                                               |
|------------|-----------------------------------------------------------|
| default    | Courbe plate a 440 Hz (correction minimale)               |
| spoken     | Voix parlee : legere oscillation autour de 200 Hz         |
| lyric      | Chant lyrique : grandes variations expressives A3..A4     |
| rap        | Montée et descente montees (~200-250 Hz)                  |
| robot      | Identique a default (placeholder pour effet "T-Pain"     |
|            | extreme -> necessiterait Speed=0)                         |

### Cablage dans processBlock

Le `processBlock` du processor interroge `getPlayHead()->getPosition()` pour
connaitre le temps de transport en secondes. En mode Graphic, ce temps est
passe a `pitchCurve->getPitchAt(t, f0_in)` qui retourne le Hz cible. Le
reste du pipeline (amount blend, retarget, formants, PSOLA) est inchange.

Le mode est sauvegarde dans l'etat du plugin via le paramètre "mode"
(AudioParameterInt 0/1). La PitchCurve elle-meme est serialisee comme
sous-element XML `<PITCH_CURVE>` dans `getStateInformation()`.

### Limites actuelles

- Pas de courbes de Bezier (interpolation lineaire uniquement).
- Pas de magnétisme autre que le snap a la gamme.
- Pas de capture directe du pitch courant par clic (mais possible via
  `capturePitch()` expose dans l'API).
- Pas de zoom (la plage est fixee a 4 secondes, 50-1000 Hz).

## Parametres exposes

| Nom      | Type      | Plage              | Defaut | Description                            |
|----------|-----------|--------------------|--------|----------------------------------------|
| Speed    | float ms  | 0 - 200            | 50     | Temps de retargeting de la correction   |
| Amount   | float 0-1 | 0.0 - 1.0          | 1.0    | Intensite (0 = passthrough)            |
| Key      | int       | 0 - 11             | 0 (C)  | Tonique de la gamme                    |
| Scale    | int       | 0 - 5              | 0 (Maj)| Mode (Maj, Min, Pent Maj, Pent Min, Chr, Custom) |
| Bypass   | bool      | off / on           | off    | Bypass du traitement                   |
| custom0..custom11 | bool | off / on      | (Maj)  | Notes actives pour la gamme Custom (C, C#, D, ..., B) |

## Gamme personnalisee (mode Custom)

Le mode `Scale::Custom` (indice 5) permet a l'utilisateur de choisir
lui-meme quelles notes (en demi-tons 0..11) appartiennent a la gamme.
Implementation :
- 12 `AudioParameterBool` exposes au host (automation individuelle possible)
- 12 `juce::ToggleButton` dans la GUI, organises en rangee horizontale
- Visibles uniquement si Scale = "Custom" (gere par `updateCustomVisibility()`)
- Le `ScaleQuantizer` recoit la liste des notes via `setCustomIntervals()`
  (sans decalage de key, contrairement aux autres modes)
- Le snap interactif du `PitchCurveEditor` utilise `snapToScaleCustom()`
  pour la quantif sur les notes choisies

## Affichage temps reel (note chantee + cents + meter)

Le `PitchVisualizer` affiche en permanence :
- **Le nom de la note chantee** (ex: "F3") dans son header.
- **Le nom de la note cible** (ex: "-> F3") si elle differe.
- **L'offset en cents** (ex: "-50 c") avec code couleur :
  - vert (|c| < 5) : dans la note
  - jaune (|c| < 25) : proche
  - rouge (|c| >= 25) : clairement a cote
- **Un meter de tuning vertical** (aiguille selon les cents, graduations
  a +/-50 et +/-100, style Antares / Studio One).
- **Les lignes des notes de la gamme courante** en arriere-plan (jaune
  semi-transparent) sur 4 octaves (C2 -> C6).

Le calcul des informations est fait par `atdsp::describePitch()` dans
`NoteUtils.h` (conversion Hz -> MIDI -> nom + calcul des cents d'offset
entre pitch d'entree et pitch quantifie).

## Clavier de piano vertical (PianoKeyboard)

Le `ui::PianoKeyboard` est un composant place a gauche du `PitchCurveEditor`
(40 px de large) qui dessine un clavier de piano vertical :
- **Touches blanches** (C, D, E, F, G, A, B) en pleine largeur
- **Touches noires** (C#, D#, F#, G#, A#) par-dessus, 60% plus courtes
- **Notes de la gamme surlignees en jaune** (permet de voir immediatement
  quelles notes sont "autorisees" par la gamme courante)
- **Label des octaves** (C2, C3, ...) sur la gauche des touches C

L'axe Y est vertical : notes graves en BAS, notes aigues en HAUT.
Plage par defaut : C2 (MIDI 36) -> C7 (MIDI 96), suffisante pour la voix.

## Latence

La latence depend de la fenetre d'analyse du PSOLA.
Cible MVP : **20-30 ms** (acceptable pour monitoring).
La latence exacte est declaree au host via `AudioProcessor::getLatencySamples()`.

## Multi-format

| Format  | Statut         | Plateforme      | Notes                              |
|---------|----------------|-----------------|------------------------------------|
| VST3    | Actif          | Windows         | Compile et testable                |
| Standalone | Actif       | Windows         | Application .exe, test sans DAW    |
| AU      | Configure      | macOS           | Necessite un Mac pour compiler     |
| AAX     | Configure      | macOS           | Necessite Mac + dev Avid           |
| LV2     | Non configure  | Linux           | A ajouter si besoin                |

## Decisions architecturales

1. **Dsp modules isoles** : PitchDetector, ScaleQuantizer et PitchShifter sont
   des classes separees avec une responsabilite unique. Cela permet de les
   tester independamment et de les remplacer.

2. **AudioProcessorValueTreeState** : les parametres sont geres par l'arbre
   de valeurs de JUCE, qui synchronise automatiquement host <-> GUI <-> DSP.

3. **Pas de dependance externe** : on utilise uniquement JUCE (fourni).
   Pas de librairie tierce pour le DSP (pas de libsoxr, pas de rubberband)
   afin de garder le projet simple et maitrise.

4. **C++17** : on cible ce standard (defini par Projucer) pour beneficiaire
   de `<optional>`, `if constexpr`, etc. sans necessiter C++20.

## References

- de Cheveigne, A., & Kawahara, H. (2002). YIN, a fundamental frequency estimator for speech and music. JASA.
- Moulines, E., & Charpentier, F. (1990). Pitch-synchronous waveform processing techniques for text-to-speech synthesis using diphones. Speech Communication.
- McLeod, P., & Wyvill, G. (2005). A smarter way to find pitch.
- Zölzer, U. (2011). DAFX: Digital Audio Effects (2nd ed.). Wiley.
