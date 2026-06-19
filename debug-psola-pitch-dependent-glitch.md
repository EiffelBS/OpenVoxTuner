# Debug Session: psola-pitch-dependent-glitch

- **Status**: [IN-PROGRESS] - Fixes appliques pour H1 et H5, en attente de validation utilisateur
- **Date**: 2026-06-11
- **Issue**: Glitches audio pitch-dependent (changent avec le pitch chante) et pops persistants a toutes les tailles de buffer.

## Symptomes (rapportes par Jerome, plusieurs rounds)

| Buffer | Symptome | Evolution |
|--------|----------|-----------|
| 2048 | Pops frequents quand out-of-tune | Pas d'amelioration notable apres rounds 4-5-6 |
| 144 | Clicks ET pops OU audio glitches qui changent avec le pitch | In-tune maintenant possible (round 6), mais glitches persistent |

## Fixes appliques (round 7)

### H1 (YIN octave error) - FIX
- Ajout d'une **etape 3b** dans `PitchDetector::detectPitch` :
  apres avoir trouve un tau sous le seuil, on verifie si 2*tau est
  AUSSI sous le seuil. Si oui, tau est une sous-harmonique et on
  prend 2*tau a la place (= le fondamental).
- Reference : de Cheveigne & Kawahara (2002), "YIN, a fundamental
  frequency estimator for speech and music".

### H5 (COLA gain modulation) - FIX
- Le gain COLA `1/overlapCount` changeait par paliers quand T0p
  changeait entre blocs (saut d'amplitude = pop audible).
- Ajout d'un **lissage time-based** du gain (tau=20 ms) : nouveau
  membre `smoothedGain` dans `PitchShifter`, mis a jour dans process()
  avec alpha = 1 - exp(-blockDuration/0.02).

## Notes

- Ces fixes n'ont PAS ete valides par instrumentation runtime (le
  debug-server HTTP n'est pas utilisable dans le thread audio). Ils
  sont bases sur l'analyse statique du code et les hypotheses
  les plus probables.
- Pour validation : l'utilisateur doit tester a 144 et 2048, et
  confirmer si le "glitch pitch-dependent" et les "pops" sont
  reduits.
- Si les glitches persistent, les hypotheses H2 (phasiness PSOLA),
  H3 (findPeak jitter) et H4 (RetargetEnvelope) restent a tester.
  Pour H2 et H3, il faudra des algos plus sophistiques (phase
  vocoder, interpolation) qui depassent le scope de cette session.
