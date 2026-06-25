# Changelog 2026-06-23

## Pitch Detector — Anti-octave-error par continuite d'octave

### Problem
L'algorithme YIN detecte par erreur une octave trop haute sur les notes graves (ex: F#1 chante -> detecte F#2) quand le fondamental est faible et que la 1ere harmonique domine. L'anti-octave-error existant (etape 3b) ne corrige que les sauts vers le bas (2*f0 detecte au lieu de f0), pas vers le haut.

### Solution
Ajout d'une verification de continuite d'octave dans `PitchDetector::getMedianFiltered()` :
- Si la mediane differ du dernier pitch valide d'un facteur ~2 (octave au-dessus) ou ~0.5 (octave en-dessous), on ajuste vers l'octave la plus proche du contexte precedent.
- Nouveau membre `lastValidPitch` pour suivre le dernier pitch valide.

### Fichiers modifies
- `Source/dsp/PitchDetector.h` : ajout membre `lastValidPitch`
- `Source/dsp/PitchDetector.cpp` : correction d'octave bidirectionnelle dans `getMedianFiltered()`, mise a jour de `lastValidPitch` dans `detectPitch()`
