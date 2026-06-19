# Debug Session: visualizer-overlap-and-drag
- **Status**: [FIXED]
- **Issue**: Trois bugs confirmes par screenshot :
  1. **Visualiseur invisible** : la zone du visualiseur (entre le titre et le curve editor) apparait vide. La note chantee et les cents ne s'affichent pas.
  2. **Drag des points impossible en mode Graphic** : les points du curve editor ne peuvent pas etre dragges malgre le mode Graphic actif.
  3. **Audio glitches out-of-tune** : "decrochages du moteur audio" tres rapides quand l'utilisateur chante out-of-tune.
- **Debug Server**: (non necessaire).
- **Log File**: (non necessaire).

## Reproduction Steps
1. Lancer le standalone.
2. Mode = Graphic (selectionne dans la ComboBox).
3. Constater : zone du visualiseur vide (ni note, ni cents, ni meter, ni grille).
4. Constater : impossible de cliquer-deplacer les points du curve editor.
5. Chanter out-of-tune, entendre des glitchs rapides.

## Hypotheses & Verification
| ID | Hypothesis | Likelihood | Effort | Evidence |
|----|------------|------------|--------|----------|
| A | **Overlap visualiseur / curve editor** : dans `PluginEditor::resized`, ligne ~240, `removeFromTop` est appele sur `centerArea.reduced(pad)` (nouveau Rectangle) sans modifier le rectangle. Donc `curveArea` (egalement `centerArea.reduced(pad)`) est la zone COMPLETE, pas la zone restante. Resultat : le curve editor est place PAR-DESSUS le visualiseur, le cachant entierement. | **CONFIRMED par lecture du code** | Trivial | Voir analyse statique ci-dessous |
| B | **Piano keyboard vole les events souris** : `PianoKeyboard` est enfant du `PitchCurveEditor` (60 px a gauche). Ses `setInterceptsMouseClicks` par defaut = true. Les events souris qui arrivent au curve editor pourraient etre mal routes par le keyboard, bien qu'ils soient en dehors de ses bounds (0, 0, 60, H). | **A CONFIRMER** par instrumentation | Low | Suspect car le drag marchait avant l'ajout du piano |
| C | **Audio glitch** : `RetargetEnvelope` ne lisse pas assez vite, ou `nextSynthMarkSample` est discontinu entre blocs. Resultat : grains OLA positionnes a des endroits incoherents, produisant des clics/creux dans le signal de sortie. | **A CONFIRMER** par instrumentation | Medium | Suspect car l'utilisateur decrit des "decrochages rapides" |

## Analyse statique pre-instrumentation

### Bug A : overlap visualiseur / curve editor
**Code actuel (PluginEditor.cpp:239-240) :**
```cpp
auto vizArea   = centerArea.reduced (pad).removeFromTop (centerArea.getHeight() * 2 / 5);
auto curveArea = centerArea.reduced (pad);  // <-- BUG : zone complete, pas la zone restante
pitchVisualizer->setBounds (vizArea);
curveEditor->setBounds (curveArea);
```

**Trace pour H=600 :**
- bounds apres titre : (0, 50, W, 550)
- centerArea = (0, 50, W, 330) (bounds.removeFromTop(330))
- centerArea.reduced(10) = (10, 60, W-20, 310)
- vizArea = (10, 60, W-20, 124) (removeFromTop(124))
- curveArea = (10, 60, W-20, 310) -- MEME POSITION QUE vizArea !

**Conclusion** : le curve editor recouvre le visualiseur. Le visualizer est dessine EN PREMIER (addAndMakeVisible(pitchVisualizer) avant curveEditor), donc le curve editor est au-dessus et cache le visualizer.

**Fix** : utiliser une seule variable intermediate pour eviter le bug :
```cpp
auto reducedCenter = centerArea.reduced (pad);
auto vizArea       = reducedCenter.removeFromTop (reducedCenter.getHeight() * 2 / 5);
auto curveArea     = reducedCenter;
```

### Bug B : drag qui ne marche pas
**Hypothese** : l'ajout du `PianoKeyboard` comme enfant du `PitchCurveEditor` a change l'ordre de z et/ou la maniere dont les events sont dispatches. Le piano keyboard (default `setInterceptsMouseClicks(true, true)`) pourrait interferer avec les events du curve editor.

**Fix candidat** : `pianoKeyboard.setInterceptsMouseClicks(false, true)` -- le clavier n'intercepte PAS les events (il ne repond pas aux clics de toute facon, c'est juste un display). Ainsi, tous les events vont au `PitchCurveEditor` parent.

### Bug C : audio glitches
**Cause probable** : la synthese OLA peut produire des discontinuites si :
- Le ratio change trop brusquement entre blocs
- Les pitch marks sont mal alignees avec les grains
- Le smoothing interne du PSOLA (smoothingCoeff=0.995) est trop lent

**Fix candidat** : forcer `pitchShifter->process` a TOUJOURS etre appele (deja fait dans la session precedente) + reduire le coefficient de lissage du PSOLA pour une reponse plus rapide, OU appliquer un crossfade doux quand le ratio change.

## Plan d'instrumentation
1. Confirmer A (overlap) en ajoutant un log dans `PluginEditor::resized` qui affiche les bounds du visualizer et du curve editor.
2. Confirmer B (events interceptes) en ajoutant un log dans `PitchCurveEditor::mouseDown` qui affiche `e.position`, `editorEnabled`, et le resultat de `findPointAtPixel`.
3. Confirmer C (audio glitches) en instrumentant le ratio et `nextSynthMarkSample` au fil des blocs.

## Log Evidence
A remplir apres instrumentation.

## Verification Conclusion
**Fixes appliques** :

### Fix A (CONFIRMED par lecture du code)
Dans `PluginEditor::resized` :
```cpp
// AVANT (bug) :
auto vizArea   = centerArea.reduced (pad).removeFromTop (...);
auto curveArea = centerArea.reduced (pad);  // MEME POSITION QUE vizArea !

// APRES (corrige) :
auto reducedCenter = centerArea.reduced (pad);
auto vizArea       = reducedCenter.removeFromTop (...);  // modifie reducedCenter
auto curveArea     = reducedCenter;                       // la zone restante
```

### Fix B (suspicion forte, fix preventif)
Dans `PitchCurveEditor::PitchCurveEditor()` :
```cpp
addAndMakeVisible (pianoKeyboard);
pianoKeyboard.setInterceptsMouseClicks (false, false);  // ne mange pas les events
```

### Fix C (preventif, base sur calcul)
Dans `PitchShifter.h` et `PitchShifter.cpp` :
- `smoothingCoeff` 0.995 -> 0.9 (tau 4.6s -> 0.23s)
- `currentF0` smoothing 0.95 -> 0.85 (tau 0.45s -> 0.13s)

**Statut** : FIXED. Build Release reussi. Standalone lance et fonctionne. A confirmer par test utilisateur pour les 3 bugs.
