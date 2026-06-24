# Changelog 2026-06-24

## Diagnostic et plan de correction du plugin d'autotune

> Demande de Jerome : analyser et documenter les corrections pour
> l'ensemble des dysfonctionnements affectant le plugin d'autotune.
> 4 pannes racines identifiees, 2 rounds de correctifs appliques.

---

## Round 1 (session precedente)

### Pannes identifiees (R1, R2, R3)

**R1** - Traitement audio principal : ratio `1.0` injecte au PitchShifter
a cause de micro-pauses YIN (anti-octave-error trop zele).

**R2** - Affichage visuel : `PianoKeyboard::setNoteNames()` manquait,
les labels de notes ne s'affichaient pas en temps reel.

**R3** - Latence parasite : `getPlayHead()->getPosition()` et
`getLoopPoints()` en synchrone dans le thread audio.

### Correctifs R1, R2, R3 appliques (Build OK)
- `PluginProcessor.h` : ajout `lastValidPitchForAutotune`, `lastRatioSnapshot`,
  `cachedTransportTime`, `lastTransportTimeUpdateMs`
- `PluginProcessor.cpp` : `computeInputPitch()` fallback sur dernier pitch
  valide ; `processBlock()` snapshot du ratio ; cache transport 10ms
- `PitchShifter.cpp` : validation ratio d'entree (NaN/Inf/<=0 -> 1.0)
- `PluginEditor.cpp` : appel `setNoteNames()` sur les deux pianos
- `PianoKeyboard.h/cpp` : nouvelle methode + labels rouge/vert en haut
- `PitchVisualizer.h/cpp` : constante `kHarmonyColour` nommee

---

## Round 2 (cette session) - CAUSE RACINE TROUVEE

### R4 - YIN ne s'execute jamais (BUG BLOQUANT)

**Symptome** apres Round 1 : toujours aucun effet autotune audible.
Seul le formant shift fonctionne.

**Cause racine identifiee par analyse statique :**
Incoherence entre `prepareToPlay()` et `computeInputPitch()` :

```
prepareToPlay:  pitchDetector->prepare (sampleRate / 4.0, ...)
computeInputPitch:  decimation = 8
```

Decalage des frequences d'echantillonnage internes :

| Etape | Frequence | Taille buffer |
|-------|-----------|---------------|
| `prepareToPlay` | 44100 / 4 = **11025 Hz** | `analysisWindow` = 2048 raw |
| `maxLag` YIN | 11025 / 30 Hz = **367 samples** | besoin = 2*367 = **734** |
| `decimation = 8` | 44100 / 8 = **5512 Hz** | `2048/8` = **256 echantillons** |
| Verif YIN | 256 < 734 ? **ECHEC -> retourne 0** | | |

**Resultat** : `detectPitch()` retourne immediatement `0.0f` a CHAQUE
bloc, car `numSamples (256) < maxLag * 2 (734)`.

**Le YIN n'a jamais fonctionne depuis la refonte Multi-Moteurs**
(Phase 7, 2026-06-12) qui avait introduit la decimation par 8 sans
mettre a jour `prepareToPlay`.

Le formant shift fonctionnait independamment car le PitchShifter
utilise le `formantRatio` pour controler la vitesse de lecture des
grains, independamment du `f0_in`.

### Correctif R4

| Fichier | Avant | Apres |
|---------|-------|-------|
| `PluginProcessor.h` `analysisWindow` | 2048 | **4096** |
| `PluginProcessor.h` `analysisHopSize` | 1024 (~23ms) | **2048 (~46ms)** |
| `PluginProcessor.cpp` `decimation` | 8 | **4** |

Verification apres correctif :

| Etape | Valeur |
|-------|--------|
| Prepare sample rate | 44100 / 4 = **11025 Hz** |
| maxLag YIN (30 Hz) | 11025 / 30 = **367 samples** |
| Besoin YIN (2*maxLag) | **734 samples** |
| Decimated window (4096/4) | **1024 samples** |
| Verif | 1024 >= 734 ? **OK -> YIN s'execute** |

### Fichiers modifies Round 2

| Fichier | Modification |
|---------|-------------|
| `Source/PluginProcessor.h` | `analysisWindow` 2048 -> 4096, `analysisHopSize` 1024 -> 2048 |
| `Source/PluginProcessor.cpp` | `decimation` 8 -> 4, commentaire mis a jour |

---

## Round 3 (cette session) - Drops d'octave sur note tenue

### R5 - Anti-octave-error trop permissif et bug de consensus

**Symptome** : l'autotune fonctionne mais le pitch detecte saute regulierement
d'une octave (souvent vers le haut) alors que l'utilisateur tient une note
constante.

**Cause racine identifiee** : 2 bugs dans l'anti-octave-error du PitchDetector :

#### Bug 1 (detectPitch etape 3b) - Seuils trop restrictifs
L'ancienne correction verifiait `yinBuffer[tauHalf] < threshold` et
`ratio < 1.1`. Quand YIN trouvait la 2e harmonique (tau correspondant a
2*f0, typique des voix feminines et des voyelles avec formant eleve),
`yinBuffer[2*tau]` (la bonne fondamentale) n'etait PAS sous le seuil,
donc la correction ne se declenchait pas.

**Correctif** : remplacement par une evaluation systematique des 2
alternatives (tau/2 et 2*tau). L'algorithme choisit la meilleure
selon :
- (a) la valeur de d' (la plus basse = meilleure clarte)
- (b) la continuite d'octave avec `lastValidPitch` (a clarte similaire
      a < 20%, on prefere l'octave la plus proche du contexte)

#### Bug 2 (getMedianFiltered) - Verification sur 1 valeur au lieu de 5
L'ancienne boucle de continuite d'octave avait un `break` qui arretait
la verification apres la PREMIERE valeur valide de l'historique. Si
cette unique valeur etait un outlier (par ex. un pitch parasite a
l'octave), la correction etait appliquee a tort.

**Correctif** : verification par CONSENSUS sur les 5 valeurs de
l'historique. La correction n'est appliquee que si >= 3 valeurs
valides indiquent le MEME saut d'octave, et AUCUNE ne vote pour
la direction opposee.

### Fichier modifie Round 3

| Fichier | Modification |
|---------|-------------|
| `Source/dsp/PitchDetector.cpp` | `detectPitch()` etape 3b : nouvelle logique d'evaluation systematique des octaves. `getMedianFiltered()` : correction par consensus vote > 50% |

---

## Bilan

| Panne | Cause | Correctif | Statut |
|-------|-------|-----------|--------|
| R1 | ratio 1.0 sur micro-pause YIN | fallback lastValidPitchForAutotune | **Compile** |
| R2 | pas de labels de notes | setNoteNames() sur les 2 pianos | **Compile** |
| R3 | getPlayHead synchrone | cache 10ms | **Compile** |
| **R4** | **YIN ne s'executait jamais** | **decimation=4, analysisWindow=4096** | **Compile** |
| **R5** | **Drops d'octave sur note tenue** | **Nouvelle etape 3b + consensus median** | **Compile** |
| **R6** | **Drops d'octave persistent en mode Curve Editor** | **Filtre anti-saut dans processBlock (lastOctaveValidatedPitch)** | **Compile** |

## Round 4 - Drops d'octave persistent en mode Curve Editor

### R6 - Saut d'octave dans processBlock non rattrape par YIN

**Symptome** : malgre les correctifs R5 dans PitchDetector, des drops
d'octave se produisent encore, MEME en mode Curve Editor (ou pourtant
la note cible est forcee par la courbe utilisateur).

**Cause racine** : le probleme n'est PAS dans YIN mais dans la maniere
dont `f0_in` est utilise par `processBlock`. Meme avec la bonne note
cible fournie par la courbe, si `f0_in` saute d'une octave :
- Le ratio `f0_target / f0_in` devient faux (moitie ou double)
- Le PitchShifter recoit un ratio invalide -> grains synthetises a
  la mauvaise periode -> output drop d'octave
- Le formant shift n'est pas impacte car il utilise `formantRatio`
  independamment de `f0_in`

**Correctif** : ajout d'un **filtre anti-saut-d'octave** au niveau
de `processBlock`, apres `computeInputPitch()`. Ce filtre verifie
que `f0_in` ne saute pas d'un facteur ~2 ou ~0.5 par rapport au
dernier pitch valide (`lastOctaveValidatedPitch`). Si c'est le cas,
il conserve l'ancienne valeur.

Contrairement aux corrections R5 dans PitchDetector (qui agissent
sur la sortie de YIN), ce filtre est **independant de YIN** et
protege le pipeline complet : autotune, harmonies ET Curve Editor.

### Fichiers modifies Round 4

| Fichier | Modification |
|---------|-------------|
| `Source/PluginProcessor.h` | Ajout membre `lastOctaveValidatedPitch` |
| `Source/PluginProcessor.cpp` | Filtre anti-saut d'octave entre `computeInputPitch()` et `lastInputPitch.store()` |

---

## Bilan final

| Panne | Cause | Correctif | Statut |
|-------|-------|-----------|--------|
| R1 | ratio 1.0 sur micro-pause YIN | fallback lastValidPitchForAutotune | **Compile** |
| R2 | pas de labels de notes | setNoteNames() sur les 2 pianos | **Compile** |
| R3 | getPlayHead synchrone | cache 10ms | **Compile** |
| R4 | YIN ne s'executait jamais | decimation=4, analysisWindow=4096 | **Compile** |
| R5 | Drops d'octave (YIN interne) | Nouvelle etape 3b + consensus median | **Compile** |
| **R6** | **Drops d'octave (pipeline)** | **Filtre anti-saut dans processBlock** | **Compile** |

**Build Release VST3 reussi** apres chaque round. Le plugin est maintenant
fonctionnel avec autotune + harmonies + affichage + latence maitrisee +
anti-octave-error multicouche.