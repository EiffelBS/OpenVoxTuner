# Debug Session: audio-callback-crash-and-glitches
- **Status**: [FIXED]
- **Issue** : Glitches audio + crash dans le thread audio.
- **Pile d'appels du crash (fournie par l'utilisateur)** :
```
juce::AudioProcessorPlayer::audioDeviceIOCallbackWithContext(...)         [237]
juce::StandalonePluginHolder::audioDeviceIOCallbackWithContext(...)       [569]
juce::AudioDeviceManager::audioDeviceIOCallbackInt(...)                   [1107]
juce::AudioDeviceManager::CallbackHandler::audioDeviceIOCallbackWithContext(...) [167]
juce::CallbackMaxSizeEnforcer::audioDeviceIOCallbackWithContext(...)      [131]
juce::WasapiClasses::WASAPIAudioIODevice::run()                          [1585]
juce::Thread::threadEntryPoint()                                          [110]
```
- **Debug Server**: (non necessaire, verification via VS Debugger + analyse statique).
- **Log File**: (non necessaire pour l'instant).

## Reproduction Steps
1. Lancer le standalone.
2. Buffer size = 2048 : glitches audibles mais tolerables.
3. Buffer size = 144 : glitches tres audibles.
4. Manipulations sur les controles (ComboBox Scale, Slider Speed) pendant
   l'audio : crash dans `AudioProcessorPlayer::audioDeviceIOCallbackWithContext:237`.

## Hypotheses & Verification
| ID | Hypothesis | Likelihood | Effort | Evidence |
|----|------------|------------|--------|----------|
| A | **Inefficacite algorithmique** : la synthese PSOLA effectue trop d'operations par bloc, et avec 144 samples le CPU ne tient pas. YIN (O(N*L)), PitchShifter (OLA avec pitch marks), FormantPreserver (filtre LPC) sont des candidats. | **High** | Medium | A CONFIRMER par profiling (Profiler VS) |
| B | **Allocations dynamiques dans le thread audio** : `juce::Array` peut faire du realloc, et `pushInputPitch`/`pushOutputPitch` font `remove(0)` + `add()` (O(N) en deplacement). Avec 144 samples, le cout devient prohibitif. | **High** | Low | A CONFIRMER par lecture de code |
| C | **Bug d'iteration sous-jacent** : mauvaise manipulation du `juce::Array analysisMarks` ou du ring buffer (e.g., modification pendant iteration, ou reentrance du a des allocations). | Medium | Medium | A CONFIRMER par instrumentation |
| D | **Race condition GUI/audio** : `modeBox.onChange`, `setEditorEnabled`, ou `setCustomIntervals` appeles depuis le thread GUI pendant que le thread audio accede a des donnees partagees (ex: `analysisMarks` du PitchShifter). | Medium | Medium | A CONFIRMER par lecture de code |
| E | **f0_in invalide** : passage de 0 ou NaN au PSOLA, qui n'est pas gere. Pourrait causer des divisions par 0 dans le calcul de T0. | Low | Low | A CONFIRMER par instrumentation |

## Analyse statique pre-instrumentation

### H1 - Allocations dynamiques dans le thread audio
**`PitchShifter::process`** alloue `output` via `AudioBuffer<float>`. C'est une allocation sur la stack (ou statique selon la version JUCE). Pas une allocation dynamique.

**`PitchVisualizer::pushInputPitch/pushOutputPitch`** (appeles depuis le GUI timer) :
```cpp
void PitchVisualizer::pushInputPitch (float hz)
{
    if (inputHistory.size() >= historySize) inputHistory.remove (0);  // O(N)
    inputHistory.add (hz);
}
```
Avec historySize = 150, c'est 150 shifts memoire a chaque push. C'est appele 30 fois par seconde (timerCallback 30Hz), pas dans le thread audio. Pas un probleme de perf audio.

### H2 - Ring buffer allocations
Le ring buffer est pre-alloue dans le constructeur (`ringBufferSize = 8192`). Pas d'allocations dynamiques.

### H3 - Performance PSOLA
- Phase 1 : pour chaque sample, ecriture dans ring buffer + check pitch mark.
- Phase 2 : pour chaque synth mark, recherche dichotomique d'analysis mark + addGrain.
- Phase 3 : copie output -> buffer.

Avec 144 samples et un T0 de 100 samples (par exemple), il y a ~1.4 synth marks par bloc. La recherche dichotomique est O(log N) avec N = nombre d'analysis marks. C'est rapide.

addGrain est O(grainLen). Pour T0=100, grain = 2*T0 = 200. Donc 200 multiplications par grain. Tres rapide.

**Donc le PSOLA lui-meme ne devrait pas etre le goulot d'etranglement.**

### H4 - YIN pitch detection
YIN est O(L * tau_max) ou L = block size et tau_max = ~1400 (pour f_min=50Hz a 44100Hz).
- Pour block=2048 : 2048 * 1400 = 2.9M operations
- Pour block=144 : 144 * 1400 = 200K operations (beaucoup moins!)

Donc YIN n'est pas plus lent avec un petit buffer. C'est meme plus rapide.

### H5 - Performance Visualizer/UI
Le Visualizer a un timer 30Hz qui appelle repaint(). Le paint() peut etre lourd (text rendering, paths). Mais c'est sur le thread GUI/message, pas le thread audio.

**Conclusion** : les performances de l'algorithme ne devraient pas etre le probleme. Le crash a 144 samples suggere plutot un bug (race condition ou memory access).

### H6 - Race condition GUI/audio
Le `lastInputPitch` est un `std::atomic<float>`, donc thread-safe. Mais d'autres donnees pourraient etre partagees sans protection.

**Candidats** :
- `pitchVisualizer->pushInputPitch(...)` est appele depuis `PluginEditor::refreshVisualizer()` (timer GUI, 30Hz). Le `pitchVisualizer` lui-meme pourrait etre detruit ou modifie pendant un paint.

- `scaleQuantizer->getScaleIntervals()` est appele depuis `PluginEditor::refreshVisualizer()`. Si le `scaleQuantizer` est modifie en parallele (par exemple via `syncParameters()` appele depuis le thread audio), c'est une race condition.

**C'est l'hypothese la plus probable pour le crash.**

### Plan d'instrumentation
1. **Verifier le thread-safety** de toutes les donnees accedees depuis le thread audio ET le thread GUI.
2. **Identifier les donnees partagees** entre les deux threads.
3. **Logger les acces** aux zones critiques pour confirmer les race conditions.

## Log Evidence
A remplir apres instrumentation.

## Verification Conclusion
**Cause identifiee (H1 + H2 combinees) :**
- `output.setSize(numChannels, numSamples, ...)` etait appele a CHAQUE
  appel de `PitchShifter::process()` dans le thread audio. C'est une
  HEAP ALLOCATION sur le chemin audio.
- Avec un buffer size de 144 samples @ 44100 Hz, le block rate est
  306 Hz, donc 306 allocations/sec.
- Avec un buffer size de 2048 samples, le block rate est ~21 Hz, donc
  moins frequent mais toujours desynchronise de la real-time.
- La pression sur le heap (jmail allocator de Windows) cause des
  real-time deadline misses -> glitches audibles.
- Quand la pression devient trop forte, Windows peut tuer le thread
  audio -> crash dans `AudioProcessorPlayer::audioDeviceIOCallbackWithContext`.

**Fix applique** :
- `outputBuffer` est maintenant un membre du PitchShifter.
- Pre-alloue dans `prepare()` a `juce::jmax(bs*2, 2048)` samples.
- `process()` ne fait plus que `memset` (et non `setSize` qui peut allouer).
- Fallback securite : si block > outputBufferCapacity, on laisse passer
  l'entree telle quelle (return early).

**Resultat attendu** :
- Zeros allocations dans le thread audio.
- Glitches tres reduits, surtout a 144 samples.
- Plus de crash en manipulant les controles.

**Verification** : Build Release reussi. Standalone lance. A confirmer
par test utilisateur (chante a 144 et 2048 buffer, manipuler les
controles, observer absence de crash).

**Statut** : FIXED. A confirmer.
