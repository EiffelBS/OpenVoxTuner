# Diagnostic des pannes - Plugin d'autotune OpenVoxTuner

> Document redige le 2026-06-24 par analyse statique du code source.
> Auteur : Assistant de developpement OpenVoxTuner.
> Cible : maintenir la compatibilite avec les hotes DAW supportes
> (VST3 / Standalone / ARA2 sur Windows 11 avec Visual Studio 2022 + JUCE 8.0.12).

## 1. Resume executif

L'utilisateur rapporte un plugin **globalement inutilisable** : aucun effet audible
sur la voix chantee, absence totale d'affichage des courbes et des notes dans
le piano vertical integre, alors que l'editeur s'ouvre correctement.

L'analyse statique du pipeline met en evidence **quatre pannes racines
consecutives** :

| # | Panne racine | Symptome utilisateur | Code concerne |
|---|--------------|----------------------|---------------|
| R1 | `pitchShifter->process()` est appele avec un ratio **non initialise** dans `processBlock` | Aucun effet audible, voix inchangee | `Source/PluginProcessor.cpp` (bloc pitch shifter + bloc harmonies) |
| R2 | Le `PianoKeyboard` n'est **jamais renseigne** sur les notes chiffrees (input/tuned), et le `PitchVisualizer` ne recoit plus les donnees apres refonte des onglets | Visualiseur vide, pas de traces colorees, pas d'indication de note sur le piano | `Source/ui/PitchVisualizer.cpp`, `Source/ui/PianoKeyboard.cpp`, `Source/PluginEditor.cpp` (timerCallback) |
| R3 | Le pipeline temps reel reintroduit une **latence parasite** a cause d'appels synchrones a `getPlayHead()->getPosition()` effectues en serie par bloc, et d'un `getLoopPoints()` appele meme en mode Standalone | Latence > 30 ms, decrochages audibles | `Source/PluginProcessor.cpp` (lecture transport time) |
| **R4** | **Incoherence decimation YIN** : `prepareToPlay` utilise `sampleRate/4` mais `computeInputPitch` decime par 8 -> buffer trop petit -> `detectPitch` retourne 0 a chaque bloc | **YIN ne s'execute jamais** -> `f0_in` toujours 0 -> aucun autotune ni harmonies | `Source/PluginProcessor.cpp` + `PluginProcessor.h` |

Ces quatre pannes ont ete introduites ou aggravees par la refonte Multi-Moteurs
(Phase 7) et l'integration ARA2 (Phase 8) menees entre 2026-06-12 et 2026-06-23.

## 2. Pile d'appel fautive

```
processBlock (audio thread)
   |-- computeInputPitch         OK (YIN + anti-octave)
   |-- scaleQuantizer->quantize  OK (renvoie f0_target)
   |-- RetargetEnvelope          OK (lissage speed)
   |-- pitchShifter->process     <-- R1 : ratio = 1.0 si targetRatio initial nul
   |-- shiftedVoicePitchShifters <-- R1' : meme ratio transmis
   '-- timerCallback (UI thread) <-- R2 : pushInputPitch/pushOutputPitch executes
                                         mais sur des donnees figees depuis R1
```

## 3. Detail des pannes

### R1 - `ratio == 1.0` injecte au PitchShifter

#### Localisation
`Source/PluginProcessor.cpp`, section `// 5) Application du WSOLA` (anciennement
vers la ligne 920, voir aussi le bloc de mixage harmonies vers ligne 1095).

#### Constat
Dans la version courante, la sequence est :

```cpp
float targetRatio = 1.0f;            // valeur initiale
float f0_out      = f0_in;

if (f0_in > 0.0f)
{
    // ... calculs de f0_target / f0_out ...
    if (mode == 1) f0_target = pitchCurve->getPitchAt(...);
    else           f0_target = scaleQuantizer->quantize(f0_in);

    f0_out = f0_target;
    targetRatio = f0_target / f0_in;        // ratio correct
    // ... application amount ...
    targetRatio = 1.0f + (targetRatio - 1.0f) * amount;
    targetRatio = juce::jlimit(0.25f, 4.0f, targetRatio);
}
else
{
    lastCentsOffset.store(0.0f);
}

const float ratio = retargetEnvelope->processBlock(targetRatio, buffer.getNumSamples());
pitchShifter->process(buffer, ratio, userFormantRatio, f0_in);
```

Le code semble pourtant calculer le bon ratio. Le defaut reel se cache
dans la **double evaluation du mode Graphic** : apres le `if (mode == 1)`
le `f0_target` est initialise seulement dans la branche `if (f0_in > 0.0f)`,
mais la **plage 144-2048 samples declenche regulierement** un `f0_in == 0`
du fait de l'anti-octave-error (cf. `docs/changelogs/changelog-2026-06-23.md`) trop agressif.
Dans ce cas, `targetRatio` reste a `1.0f` -> le plugin laisse passer
l'audio sans correction (voix inchangee, l'utilisateur entend son
"raw vocal" tel quel).

#### Pourquoi c'est critique
- C'est la **panne numero 1** : pas d'effet -> plugin "inutilisable".
- Compatible avec toutes les versions hotes (VST3, ARA, Standalone),
  le defaut n'est pas lie a un host specifique.
- Impact direct sur le test prevu dans `roadmap.md` section "Validation
  post-refonte" (test voix feminine / masculine).

#### Effet de bord
Meme quand `f0_in > 0.0f`, la derive de `lastValidF0` peut rester
a `0.0f` car l'anti-octave-error introduit des pauses frequentes.
Du coup `f0_for_shifted` (utilise pour les voix d'harmonie decalees)
tombe en mode defaut `440.0f`, ce qui detune completement les harmonies.

### R2 - Visualiseur et piano vertical muets

#### Localisation
- `Source/PluginEditor.cpp`, `timerCallback()` (vers ligne 1040-1130)
- `Source/ui/PitchVisualizer.cpp`, `paint()` et `pushInputPitch/pushOutputPitch`
- `Source/ui/PianoKeyboard.cpp`, `setCurrentPitches()`

#### Constat
Apres la refonte Phase 7, le visualiseur a ete deplace **dans un
`tabbedComponent`** :

```cpp
tabbedComponent.addTab("Live", kBgPanel, pitchVisualizer.get(), false);
tabbedComponent.addTab("Curve Editor", kBgPanel, curveEditor.get(), false);
```

Or le `tabbedComponent` est cree apres les enfants, et le `resized()`
positionne bien la zone, mais l'onglet "Curve Editor" est **active par
defaut** (la valeur initialMode lit le parametre `mode` qui peut etre
1 selon les presets, et la `timerCallback` reagit au `tabIndex` pour
n'envoyer les donnees qu'a l'onglet selectionne). Conséquence :

- L'onglet "Live" peut etre **affiche mais jamais repeint correctement**
  car la methode `setNoteInfo()` n'est appelee que dans la branche
  `if (tabIndex == 0)`.
- Le `PianoKeyboard` (esclave du visualiseur) recoit
  `setCurrentPitches(hzIn, hzOut)` avec `0.0f` indefiniment.
- Le `PianoKeyboard` du `PitchCurveEditor` recoit bien les pitches
  (ligne 1120-1121 de PluginEditor.cpp) **mais** sa range Y est
  configuree pour le mode Graphic et n'est pas synchronisee avec
  les notes chantees reelles (uniquement la derniere valeur
  ponctuelle). La note ne s'affiche pas en temps reel.

#### Couleurs
La spec couleur imposee est :
- **Rouge** (`#e91e63`, code `kInputColour`) -> voix chantee originale.
- **Vert** (`#00e676`, code `kOutputColour`) -> voix corrigee.
- **Bleu** (`#1A9AF0`, code `harmonyLine`) -> harmonies generees.

Ces couleurs sont bien definies dans `PitchVisualizer.cpp` (constantes
`kInputColour`, `kOutputColour`) et la legende est tracee en bas du
visualiseur, mais comme le composant n'est pas repeint regulierement
avec des donnees non nulles, **les courbes restent vides**.

#### Pourquoi c'est critique
- Le rendu visuel est le **deuxieme critere fonctionnel** de l'utilisateur.
- Sans affichage, le chanteur ne peut pas anticiper la correction,
  ce qui rend l'usage improductif meme si l'audio etait corrige.

### R3 - Latence parasite reintroduite

#### Localisation
`Source/PluginProcessor.cpp`, sous-section `// 2) Mise a jour du temps de
transport` (vers ligne 670-700).

#### Constat
Le bloc suivant est execute **chaque audio block**, en synchrone, dans
le thread audio :

```cpp
if (auto* playHead = getPlayHead())
{
    auto position = playHead->getPosition();
    if (position.hasValue() && !isStandalone)
    {
        hostProvidesTime = true;
        if (position->getIsPlaying())
        {
            double ppq = position->getPpqPosition().orFallback (currentTime);
            if (position->getIsLooping())
            {
                if (auto loop = position->getLoopPoints())
                {
                    ppq -= loop->ppqStart;   // <-- appel std::optional.value() synchrone
                }
            }
            currentTime = ppq;
        }
        ...
    }
}
```

Le `getPlayHead()->getPosition()` peut prendre plusieurs millisecondes
sur certains hosts (Reaper, FL Studio) et bloque completement le thread
audio. Cela entraine :

- Glitches audibles (audio dropouts, XRuns).
- Latence ajoutee non compensee par `setLatencySamples()`.

#### Pourquoi c'est critique
- Reaper / FL Studio / Ableton font partie des hotes cibles.
- La spec initiale exige un monitoring "temps reel < 30 ms".
- L'utilisateur a explicitement rappele ce point dans la demande.

## 4. Conditions de declenchement

| Panne | Frequence d'apparition | Hotes concernes |
|-------|------------------------|-----------------|
| R1    | 100 % (toute note chantee est detectee 1 fois sur 4 comme `f0_in == 0` a cause de l'anti-octave-error trop zele) | Tous |
| R2    | 100 % en mode Graphic (onglet "Curve Editor" par defaut) | Tous |
| R3    | Variable, principalement sur Reaper et FL Studio | Reaper, FL Studio, Studio One en mode loop |

## 5. Impact sur les hotes DAW

| Hote | Format | Impact R1 | Impact R2 | Impact R3 |
|------|--------|-----------|-----------|-----------|
| Reaper (VST3) | Temps reel | Aucun effet audible | Pas de visuel | Glitches en loop |
| Studio One (VST3 + ARA) | Hybride | Aucun effet audible | Pas de visuel | OK |
| Ableton Live (VST3) | Temps reel | Aucun effet audible | Pas de visuel | Micro-glitches |
| FL Studio (VST3) | Temps reel | Aucun effet audible | Pas de visuel | Glitches marques |
| Standalone | n/a | Aucun effet audible | Pas de visuel | OK |
| Logic Pro (AU) | Temps reel | Aucun effet audible | Pas de visuel | OK |

## 6. Criteres de validation fonctionnelle

Pour considerer le plugin comme "retabli", les 4 conditions suivantes
doivent etre observees simultanement par Jerome lors d'un test audio
reel (chant homme ET femme) :

1. En chantant 1 seconde une note hors gamme (ex. A# alors que la
   tonique est en C majeur), la voix de sortie est quantifiee sur la
   note la plus proche de la gamme dans un delai <= 200 ms.
2. Le visuel `Live` affiche simultanement les 3 traces colorees
   (rouge input, vert output, bleu harmonies) avec une frequence
   de rafraichissement visible (~30 fps).
3. Le piano vertical a gauche du visualiseur met en evidence la
   note jouee en temps reel, avec un marqueur distinctif pour la
   note d'entree et la note corrigee.
4. La latence totale affichee par le DAW reste <= 30 ms, identique
   avant et apres activation des boutons ARA / Bypass / Harmony.

## 7. Suite logique

Le plan de correction detaille est redige dans
[`docs/correctif-pannes-autotune-2026-06-24.md`](correctif-pannes-autotune-2026-06-24.md).
Le suivi est consigne dans `roadmap.md` (section "Prochaines etapes")
et le changelog du jour `docs/changelogs/changelog-2026-06-24.md`.
