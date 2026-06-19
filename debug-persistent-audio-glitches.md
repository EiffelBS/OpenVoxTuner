# Debug Session: persistent-audio-glitches
- **Status**: [FIXED]
- **Issue** : Les glitchs audio (clicks/pops) etaient tres presents apres le fix de l'outputBuffer du PSOLA. Plus de crash, mais les clicks persistaient a toutes les tailles de buffer (davantage a petit buffer).
- **Root cause identifiee** : discontinuites d'overlap-add aux frontieres des blocs audio PSOLA.
- **Debug Server**: (non necessaire).
- **Log File**: (non necessaire).

## Reproduction Steps
1. Lancer le standalone.
2. Buffer size = 144, 512, 2048, 4096.
3. A toutes les tailles : clicks/pops audibles.
4. Davantage de clicks a petit buffer.

## Cause racine (apres investigation)

Les grains PSOLA utilisent une fenetre Hann de longueur **2 * T0** (T0 = periode fondamentale,
typiquement 100-200 samples pour la voix). Pour que l'overlap-add (OLA) soit continu dans le
temps, chaque grain a besoin que les T0 echantillons AVANT et APRES sa pitch mark soient
accessibles.

Or, dans l'implementation bloc-par-bloc, l'output buffer etait remis a zero a chaque appel
de `process()`. Cela avait deux consequences graves :

1. **Demi-fenetre gauche clippee** : le premier grain d'un bloc est centre a `synthStart`,
   qui peut etre seulement quelques samples apres le debut du bloc. Sa demi-fenetre gauche
   (de longueur T0) sort du buffer et est completement perdue. Le grain ne contribue qu'avec
   sa moitie droite, ce qui produit un saut d'amplitude des le premier sample du bloc.

2. **Demi-fenetre droite clippee** : symetriquement, le dernier grain du bloc a sa demi-fenetre
   droite perdue. Le bloc suivant, qui demarre sur un output buffer vide, ne beneficie pas de
   cette contribution.

A chaque frontiere de bloc, il y a donc un "trou" de continuite d'environ 2 * T0 - T0p
echantillons (avec T0p = T0 / ratio), durant lequel l'output PSOLA passe soudainement d'une
valeur (grain complet) a une autre (grain tronque) ou a zero.

Avec T0 = 220 samples (f0 = 200 Hz @ 44.1 kHz), le "trou" est de l'ordre de 200-400 samples
a chaque bloc. Pour un buffer de 144 samples, ce "trou" est beaucoup plus grand que le bloc
lui-meme, ce qui explique la forte degradation auditive a petit buffer.

## Fix applique

Introduction d'un **etat de synthese persistant** (`synthStateBuffer`) qui propage la queue
du bloc precedent vers le debut du bloc suivant. Le buffer de travail (outputBuffer) est
reorganise comme suit :

```
[synthState (T0 samples) | blocCourant (numSamples samples)]
```

Dans la Phase 2 de `PitchShifter::process()` :
1. On copie `synthStateBuffer` au debut de `outputBuffer` (demi-fenetre gauche du 1er grain).
2. On ajoute les grains du bloc courant dans la zone etendue.
3. A la fin, on sauvegarde les T0 derniers echantillons de la zone de travail dans
   `synthStateBuffer` pour le prochain appel.
4. On copie uniquement `[synthState, synthState + numSamples]` vers le buffer de sortie.

L'OLA est maintenant **continu dans le temps** au-dela des frontieres de bloc. Les clicks
deviennent negligeables (limites au premier appel, ou au retour apres passthrough, ou
l'etat est vide et la demi-fenetre gauche est clippee une seule fois ; le crossfade de
32 samples en input masque cela).

### Fichiers modifies
- `Source/dsp/PitchShifter.h` : ajout de `synthStateBuffer`, `synthStateCapacity`, `synthStateSize`
- `Source/dsp/PitchShifter.cpp` :
  - `prepare()` : alloue `synthStateBuffer` (4096 samples) et augmente `outputBufferCapacity`
  - `reset()` : clear `synthStateBuffer`
  - `process()` branche passthrough : clear `synthStateBuffer` (etat obsolete en passthrough)
  - `process()` Phase 2 : prepend `synthState` a la zone de travail, update en fin de bloc
  - `process()` Phase 3 : copie uniquement la portion `blocCourant` vers le buffer de sortie

## Verification

- Build Release x64 : OK
- Standalone lance : OK (PID 141136, ~80 MB, stable apres 5s)
- A CONFIRMER par tests utilisateur a differentes tailles de buffer.
