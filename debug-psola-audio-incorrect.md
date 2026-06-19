# Debug Session: psola-audio-incorrect
- **Status**: [OPEN]
- **Issue**: Quand le bypass est OFF, l'audio est "incorrect" (probablement distordu ou silencieux) quand l'utilisateur chante out-of-tune. Avec bypass ON, l'audio passe en clair (dry). Avec "Mute audio input" coche dans les options audio du standalone, RIEN n'est audible (devrait etre l'audio traite).
- **Debug Server**: (non necessaire - pas de reproduction headless, verification IDE).
- **Log File**: (non necessaire).

## Reproduction Steps
1. Lancer le standalone.
2. Aller dans Options > Audio/MIDI Settings, COCHER "Mute audio input".
3. Verifier que Audio input = Microphone (Shure MVX2U), Audio output = Casque (Shure MVX2U).
4. Selectionner Scale = "Custom", avec C, D, E, F, G, A, B cochees (= Majeur en C par defaut).
5. Bypass decoche.
6. Chanter une note hors gamme (ex: C# au lieu de C).
7. Resultat observe : rien n'est audible (silence total ou distorsion).

## Hypotheses & Verification
| ID | Hypothesis | Likelihood | Effort | Evidence |
|----|------------|------------|--------|----------|
| A | Le ring buffer du PSOLA n'est pas mis a jour quand le calling code skip l'appel a `pitchShifter->process` (lorsque `|ratio - 1.0f| < 1e-3f`). Du coup quand l'utilisateur va out-of-tune, le ring buffer contient des donnees obsoletes et la synthese produit du silence ou du bruit. | High | Low | A CONFIRMER par instrumentation |
| B | `f0_in` est souvent 0 ou tres petit, le PSOLA recoit un f0 invalide | Medium | Low | A CONFIRMER par instrumentation |
| C | `RetargetEnvelope` produit un ratio errone (lissage qui ne converge pas) | Low | Low | A CONFIRMER par instrumentation |
| D | Le standalone "Mute audio input" coupe aussi la sortie du plugin (bug JUCE standalone ?) | Low | N/A | Peu probable, c'est un toggle bien documente |
| E | L'utilisateur entend le monitoring hardware de l'interface audio (Shure MVX2U) au lieu du plugin | Medium | N/A | Externe au code, mais l'explication la plus simple |

## Analyse statique pre-instrumentation

### Code actuel (PluginProcessor.cpp:209-213)
```cpp
// 6) Application du PSOLA sur le buffer.
if (std::abs (ratio - 1.0f) > 1e-3f)
{
    pitchShifter->process (buffer, ratio, f0_in);
}
```

### Bug potentiel identifie (H1)
Quand `ratio` est proche de 1 (l'utilisateur chante dans la gamme), le `pitchShifter->process` N'EST PAS APPELE. Du coup :
- Le ring buffer n'est pas alimente avec les nouveaux echantillons.
- `totalSamplesWritten` et `nextSynthMarkSample` ne sont pas mis a jour.
- Les pitch marks ne sont pas detectes.

Quand l'utilisateur passe out-of-scale :
- Le ratio devient != 1.
- `pitchShifter->process` est appele avec un ring buffer obsolete (contenant des echantillons d'il y a N blocs).
- La synthese peut etre silencieuse, distordue, ou produire des artefacts.

### PitchShifter::process passthrough branch
```cpp
if (f0 <= 0.0f || std::abs (ratio - 1.0f) < 1e-6f)
{
    // ... alimente le ring buffer pour ne pas perdre l'historique ...
    totalSamplesWritten += numSamples;
    nextSynthMarkSample = totalSamplesWritten;
    return;
}
```

Ce passthrough EST prevu pour mettre a jour le ring buffer meme quand le PSOLA ne fait rien. Mais le calling code ne l'appelle pas.

### Fix candidat
Retirer la condition `if (std::abs (ratio - 1.0f) > 1e-3f)` autour de l'appel. Toujours appeler `pitchShifter->process` ; c'est lui qui decide s'il fait du passthrough ou de la synthese. Cela garantit que le ring buffer est toujours a jour.

## Plan d'instrumentation
1. Logger `f0_in`, `targetRatio`, `smoothed ratio`, `totalSamplesWritten`, `nextSynthMarkSample`, `analysisMarks.size()` a chaque appel de processBlock.
2. Confirmer que le ring buffer reste a jour meme quand le ratio est proche de 1.
3. Confirmer que l'audio est audible apres le fix.

## Log Evidence
A remplir apres instrumentation.

## Verification Conclusion
A determiner apres fix.
