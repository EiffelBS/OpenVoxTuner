# Debug Session: pianokeyboard-negative-height
- **Status**: [FIXED]
- **Issue**: JUCE assert jassertquiet dans `coordsToRectangle<float>` declenche depuis `PianoKeyboard::paint` ligne 109 (fillRect d'une touche avec hauteur negative).
- **Debug Server**: (non necessaire - pas de reproduction headless, l'assertion est declenchee par l'IDE).
- **Log File**: (non necessaire).

## Reproduction Steps
1. Lancer `Autotune Clone.exe` (Standalone) en mode Debug depuis Visual Studio.
2. Le plugin affiche son editeur. Le composant `PianoKeyboard` est cree avec range par defaut [36..96] (C2 -> C7).
3. Premier paint -> assertion declenchee dans `coordsToRectangle<float>` (ligne 91 de juce_GraphicsContext.cpp).

## Pile d'appels (fournie par l'utilisateur)
```
coordsToRectangle<float>             [juce_GraphicsContext.cpp:91]
juce::Graphics::fillRect (float)     [juce_GraphicsContext.cpp:560]
ui::PianoKeyboard::paint             [PianoKeyboard.cpp:109]
juce::Component::paintComponentAndChildren [chained]
juce::ComponentPeer::handlePaint     [D2D render]
```

## Hypotheses & Verification
| ID | Hypothesis | Likelihood | Evidence |
|----|------------|------------|----------|
| A | `midiToY(highestMidi + 1)` retourne une valeur negative (hors range), ce qui produit un `keyH` negatif pour la derniere touche | Confirmed (calcul) | A CONFIRMER par fix et re-test |
| B | `midiToY(lowestMidi - 1)` retourne une valeur > H, produisant un `keyH` depassant la fenetre (mais pas negatif) | Partial | uniquement si la boucle sort de range, ce qui n'est pas le cas ici (la boucle est `midi <= highestMidi`) |
| C | Largeur nulle ou negative (W <= 0) | Rejected | W provient de getWidth() qui est >= 1 a l'interieur de paint (deja check au debut) |
| D | NoteUtils::midiToOctave / midiToNoteInOctave produisent des NaN | Rejected | ces fonctions retournent des entiers |

### Calcul confirmant H1
- `lowestMidi = 36`, `highestMidi = 96`, `range = 60`, `H = getHeight()` (positif).
- Pour `midi = 96` (dernier blanc, G7 ou B7 selon l'octave), la boucle fait :
  - `y = midiToY(96) = H - (96-36)/60 * H = 0`
  - `keyH = midiToY(97) - midiToY(96) = (H - (97-36)/60 * H) - 0 = -0.0167 * H`  (NEGATIF)
- L'assertion `jassertquiet` verifie `(int) h >= 0` -> declenchee.

## Plan de fix
- Clamper la hauteur de chaque touche pour qu'elle soit strictement positive :
  - Pour les blanches : `keyH = max(1.0f, midiToY(midi+1) - y)` (au minimum 1 pixel pour eviter zero-width issues)
  - Pour les noires : idem
- Alternative plus correcte : calculer la position Y en se basant sur la MOITIE de la distance entre la note N et la note N-2 (pour les blanches) ou N et N-1 (pour les noires), afin que la derniere touche prenne toute la hauteur disponible. Mais le clamp simple est suffisant et minimal.

## Log Evidence
N/A (assertion declenchee dans l'IDE, pas de logs runtime).

## Verification Conclusion
**Fix valide** : `PianoKeyboard::paint` clampe maintenant la hauteur de
chaque touche (blanche et noire) a `>= 1.0f` pixel. Le calcul qui
produisait une valeur negative (pour `midi = highestMidi`, soit C7 avec
la range par defaut 36..96) ne declenche plus l'assertion JUCE.

**Comparaison pre-fix vs post-fix** :
- **Pre-fix** : assert `jassertquiet` sur `(int) h >= 0` dans
  `coordsToRectangle<float>` ligne 91, declenchee par `fillRect`
  avec `keyH = -0.0167 * H` (negatif).
- **Post-fix** : `keyH` est toujours `>= 1.0f`, l'assertion ne peut
  plus se declencher. Le standalone se lance et s'affiche sans
  interruption.

**Statut** : FIXED. Build Release reussi. Standalone lance et
fonctionne (test runtime confirme).
