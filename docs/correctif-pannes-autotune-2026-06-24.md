# Plan de correction - Plugin d'autotune OpenVoxTuner

> Date : 2026-06-24
> Documents lies : `diagnostic-pannes-autotune-2026-06-24.md`,
> `roadmap.md`, `changelog-2026-06-24.md`.
> Cible de compatibilite : VST3 / Standalone / ARA2 sous Windows 11,
> Visual Studio 2022 17.14, JUCE 8.0.12, Projucer -> CMake.

Ce document detaille les correctifs a appliquer pour retablir les 3
fonctionnalites degradees :

1. Traitement audio principal (pitch detection + autotune + temps reel).
2. Affichage visuel temps reel (3 courbes + piano vertical integre).
3. Latence maitrisee (monitoring < 30 ms sans glitch).

## Architecture des correctifs

Les modifications touchent 5 fichiers source + 1 fichier de configuration.
Aucun changement d'API publique JUCE -> la compatibilite VST3, AU, AAX,
Standalone est preservee.

```
Source/PluginProcessor.cpp        R1, R3
Source/PluginEditor.cpp           R2 (timerCallback)
Source/ui/PitchVisualizer.cpp     R2 (push input/output, harmonisation couleurs)
Source/ui/PianoKeyboard.cpp       R2 (setCurrentPitches + NoteInfo)
Source/dsp/PitchDetector.h        R1 (anti-octave-error conditionnel)
Source/dsp/PitchShifter.cpp       R1 (defaut ratio 1.0)
```

Aucun changement de :
- Format binaire VST3 (UID, categories, bus).
- Format binaire ARA2 (factory, document controller).
- Version Projucer (`JucePlugin_VersionString` reste `0.1.1`).

## Correctif 1 - R1 : Garantir un ratio non nul vers le PitchShifter

### Objectif
L'effet doit etre applique des qu'un pitch d'entree valide est detecte,
meme en cas de micro-pause d'anti-octave-error.

### Fichiers concernes
- `Source/PluginProcessor.cpp` (processBlock)
- `Source/dsp/PitchDetector.h` (mode "fallback" conditionnel)
- `Source/dsp/PitchShifter.cpp` (validation ratio d'entree)

### Modifications

#### 1.1 - Reprise immediate du dernier pitch valide
Dans `processBlock`, remplacer :

```cpp
// AVANT
float newPitch = pitchDetector->detectPitch (linear, decimatedWindow);
if (newPitch == 0.0f)
    return lastInputPitch.load();
return newPitch;
```

par :

```cpp
// APRES
float newPitch = pitchDetector->detectPitch (linear, decimatedWindow);
if (newPitch > 0.0f)
    lastValidPitchForAutotune.store (newPitch);
return (newPitch > 0.0f) ? newPitch : lastValidPitchForAutotune.load();
```

Justification : preserve la continuite du signal meme si l'anti-octave-error
rejette transitoirement un pic, conformement au pattern deja utilise pour
`lastValidF0` (harmonies).

#### 1.2 - Forcer un ratio non nul dans processBlock
Dans la branche `else { lastCentsOffset.store(0.0f); }`, ajouter :

```cpp
// APRES
if (targetRatio == 1.0f && lastValidPitchForAutotune.load() > 0.0f)
{
    // Reprise immediate du dernier ratio connu pour eviter un saut
    // d'amplitude lors de la reprise apres micro-pause YIN.
    float previousRatio = lastRatioSnapshot.load();
    if (std::abs(previousRatio - 1.0f) > 0.001f)
        targetRatio = previousRatio;
}
```

Et immediatement apres `retargetEnvelope->processBlock` :

```cpp
// APRES
lastRatioSnapshot.store (ratio);
```

Justification : empeche le retour brutal a ratio=1.0 lors des micro-pauses,
ce qui est la source directe du "aucun effet audible".

#### 1.3 - Validation cote PitchShifter
Dans `PitchShifter::process`, ajouter en debut de methode :

```cpp
// APRES
if (pitchRatio <= 0.0f || std::isnan(pitchRatio) || std::isinf(pitchRatio))
    pitchRatio = 1.0f;  // securite : ratio invalide -> pass-through neutre
```

Justification : defense en profondeur, conforme a la regle "validate at
system boundaries" du brief.

### Tests de validation R1
1. Chanter un A# (hors gamme C majeur) pendant 2 s -> la sortie
   doit etre quantifiee sur A (note la plus proche) ou G# (selon
   algorithme) sans interruption.
2. Verifier dans le log (`OVT_LOG`) la presence de `f0_in > 0` et
   `ratio != 1.0` sur au moins 80 % des blocs pendant le chant.
3. Aucun "pop" audible a la frontiere d'un silence < 100 ms (anti-pop
   par lissage time-based du `RetargetEnvelope`).

## Correctif 2 - R2 : Visualiseur et piano vertical fonctionnels

### Objectif
Les 3 traces (rouge / vert / bleu) doivent s'afficher en temps reel
et le piano vertical doit indiquer la note chantee + la note corrigee.

### Fichiers concernes
- `Source/PluginEditor.cpp` (timerCallback)
- `Source/ui/PitchVisualizer.cpp` (push + couleurs)
- `Source/ui/PianoKeyboard.cpp` (setCurrentPitches + NoteInfo)

### Modifications

#### 2.1 - Mise a jour inconditionnelle du visualiseur
Dans `OpenVoxTunerAudioProcessorEditor::timerCallback`, remplacer :

```cpp
// AVANT
if (tabIndex == 0) {
    pitchVisualizer->pushInputPitch(hzIn);
    pitchVisualizer->pushOutputPitch(hzOut);
    pitchVisualizer->setNoteInfo(info);
}
```

par :

```cpp
// APRES
// Toujours pousser les donnees dans le visualiseur (peu importe l'onglet)
// pour eviter tout "blanc visuel" quand on switch d'onglet.
pitchVisualizer->pushInputPitch(hzIn);
pitchVisualizer->pushOutputPitch(hzOut);
pitchVisualizer->setNoteInfo(info);

// Le piano du curve editor est mis a jour seulement en mode Graphic.
if (curveEditor != nullptr && tabIndex == 1) {
    curveEditor->getPianoKeyboard().setCurrentPitches(hzIn, hzOut);
}
```

Justification : l'utilisateur a signale que les deux modes (Auto / Graphic)
doivent afficher les notes sur le piano, conformement a la spec.

#### 2.2 - NoteInfo sur le piano vertical integre au visualiseur
Dans `PianoKeyboard.h`, ajouter :

```cpp
void setNoteNames (const juce::String& inputName, const juce::String& outputName);
```

Dans `PianoKeyboard.cpp`, implementation :

```cpp
void PianoKeyboard::setNoteNames (const juce::String& inputName,
                                  const juce::String& outputName)
{
    if (inputNoteLabel == nullptr) {
        inputNoteLabel = std::make_unique<juce::Label>();
        addAndMakeVisible (inputNoteLabel.get());
    }
    inputNoteLabel->setText (inputName, juce::dontSendNotification);
    // ... (style : couleur rouge)
}
```

Justification : la spec utilisateur precise
"affichage en temps reel de la note chantee originale et de la note
tunee dans le piano vertical integre au plugin".

#### 2.3 - Couleurs spec conformes
Verifier dans `PitchVisualizer.cpp` que les constantes restent :

```cpp
const juce::Colour kInputColour  = juce::Colour (0xffe91e63).withAlpha(0.4f); // rouge
const juce::Colour kOutputColour = juce::Colour (0xff00e676);                // vert
const juce::Colour kHarmonyColour= juce::Colour (0xff1A9AF0).withAlpha(0.7f); // bleu
```

Si la specification design impose des valeurs differentes, creer une
constante unique `kHarmonyColour` deja nommee pour eviter l'usage de
la valeur magique `0xff1A9AF0` au milieu de la fonction `paint`.

### Tests de validation R2
1. Chanter en temps reel, observer simultanement la trace rouge
   (input) et verte (output) dans l'onglet "Live".
2. Activer l'harmonie, verifier qu'une 3e trace bleue apparait
   pour chaque voix d'harmonie generee.
3. Verifier que le piano vertical a gauche du visualiseur affiche
   en permanence un marqueur rouge pour la note d'entree et un
   marqueur vert pour la note corrigee.

## Correctif 3 - R3 : Latence < 30 ms garantie

### Objectif
Eliminer les appels synchrones lourds dans le thread audio et garantir
une latence de monitoring inferieure a 30 ms sur tous les hotes.

### Fichiers concernes
- `Source/PluginProcessor.cpp` (transport time)

### Modifications

#### 3.1 - Cache du transport time
Introduire un membre prive dans `OpenVoxTunerAudioProcessor` :

```cpp
// APRES
std::atomic<double> cachedTransportTime { 0.0 };
std::atomic<uint32_t> lastTransportTimeUpdateMs { 0 };
```

Dans `processBlock`, remplacer la section transport par :

```cpp
// APRES
uint32_t nowMs = juce::Time::getMillisecondCounter();
uint32_t lastUpd = lastTransportTimeUpdateMs.load();
if (nowMs - lastUpd > 10)   // mise a jour au plus 1x / 10 ms
{
    lastTransportTimeUpdateMs.store (nowMs);
    // ... ancien code de recuperation du playhead ...
    cachedTransportTime.store (currentTime);
}
else
{
    currentTime = cachedTransportTime.load();
}
```

Justification : 10 ms = granularite suffisante pour le mode Graphic
(transport en beats), sans bloquer le thread audio.

#### 3.2 - Eviter `getLoopPoints()` en mode Standalone
Dans la branche Standalone (deja testee par `isStandalone`), la
boucle n'est pas supportee. Ajouter une condition supplementaire :

```cpp
// APRES
if (position->getIsLooping() && !isStandalone)
{
    if (auto loop = position->getLoopPoints())
        ppq -= loop->ppqStart;
}
```

Justification : `getLoopPoints()` est un appel `std::optional::value()`
qui peut etre couteux sur certains hosts.

### Tests de validation R3
1. Mesurer la latence via `setLatencySamples()` : doit etre <=
   `(0.030 * sampleRate)` = 1320 samples a 44.1 kHz.
2. Activer la lecture en boucle dans Reaper, observer zero glitch
   sur 5 minutes de monitoring.
3. Charger un projet dans Studio One en mode ARA, basculer en mode
   "Live" du plugin, mesurer la latence declaree = 20 ms (mode
   "Quality" par defaut).

## Calendrier d'integration (sans estimation temporelle)

| Etape | Description | Validation | Bloquant |
|-------|-------------|------------|----------|
| E1 | Correctif R1 (1.1 + 1.2 + 1.3) | Test voix reelle 30 s | Oui |
| E2 | Correctif R2 (2.1 + 2.2 + 2.3) | Test visuel 30 fps | Oui |
| E3 | Correctif R3 (3.1 + 3.2) | Test loop 5 min | Oui |
| E4 | Tests d'integration voix homme + femme | Reaper + Studio One | Oui |
| E5 | Build Release x64 + installation VST3 | build_installer.ps1 | Oui |
| E6 | Documentation utilisateur (option) | `docs/user-guide.md` | Non |

## Prevention des regressions futures

Pour eviter qu'un futur commit reintroduise l'un de ces 3 bugs,
les garde-fous suivants sont mis en place :

1. **R1** : ajout d'un test unitaire dans `test/dsp/PitchShifterTest.cpp`
   qui verifie que `process(..., 0.0f, ...)` declenche un warning
   compile-time ou runtime.
2. **R2** : ajout d'un assert `jassert(pitchVisualizer != nullptr)`
   dans `timerCallback` et d'un log OVT en mode DEBUG si la trace
   rouge est vide pendant > 1 s de chant detecte.
3. **R3** : encapsulation de la lecture du transport time dans une
   nouvelle methode `updateTransportTimeIfNeeded()` documentee
   "NE PAS APPELER PLUS D'UNE FOIS PAR TRANCHE DE 10 ms".

## Validation finale

Une fois les 3 correctifs appliques et valides separement (E1, E2, E3),
lancer la procedure de validation globale :

1. Build Release x64 via `build.ps1 -configuration Release`.
2. Installation via `install_vst3.ps1` dans
   `C:\Program Files\Common Files\VST3\`.
3. Lancement du Standalone et test des 4 criteres du diagnostic
   (`docs/diagnostic-pannes-autotune-2026-06-24.md` section 6).
4. Si succes, mise a jour du `changelog-2026-06-24.md` et increment
   de la version Projucer vers `0.1.2`.

## Annexes

- Architecture globale : `docs/architecture.md`
- Refonte multi-moteurs (Phase 7) : section Phase 7 du `roadmap.md`
- ARA Specifications : `docs/ARA_Specifications.md`
- Build Windows : `BUILD_GUIDE.md`
- Tests unitaires : `test/dsp/`
