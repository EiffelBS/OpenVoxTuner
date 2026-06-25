# Changelog - 9 juin 2026

## Initialisation du projet Autotune Clone

### Decisions architecturales
- Framework : **JUCE 8.0.12** (clone git dans `C:\JUCE`)
- Toolchain : **CMake + Visual Studio 2022 + NMake** sur Windows 11
- Build system : `CMakeLists.txt` (pas de Projucer, plus moderne et plus simple)
- Formats actifs : **VST3** et **Standalone**
- Formats prepares mais non activables depuis Windows : **AU** (macOS), **AAX** (macOS + dev Avid)
- Approche DSP : demarrage simple, iteration (YIN -> quantificateur -> PSOLA)
- Gammes : tonique + mode custom (majeur, mineur, pentatoniques, chromatique)
- GUI : 4 knobs (Speed, Amount, Key, Scale) + visualiseur de pitch en temps reel

### Fichiers crees

**Pipeline DSP (Phase 1 + 4)** :
- `Source/PluginProcessor.h` / `.cpp` : pipeline complet
- `Source/PluginEditor.h` / `.cpp` : GUI personnalisee (knobs + visualiseur)
- `Source/dsp/PitchDetector.h` / `.cpp` : algorithme YIN complet
- `Source/dsp/ScaleQuantizer.h` / `.cpp` : quantification tonique + mode (5 modes)
- `Source/dsp/PitchShifter.h` / `.cpp` : PSOLA (Phase 4) - ring buffer + pitch marks + OLA
- `Source/dsp/FormantPreserver.h` / `.cpp` : biquad passe-bas Butterworth
- `Source/dsp/RetargetEnvelope.h` / `.cpp` : IIR 1er ordre pour le Speed
- `Source/dsp/PitchCurve.h` / `.cpp` : pitch curve editable (mode Graphic)
- `Source/ui/PitchVisualizer.h` / `.cpp` : visualisation semi-log des pitch curves
- `Source/ui/PitchCurveEditor.h` / `.cpp` : editeur interactif (drag, add, delete, preset)

**Build system** :
- `CMakeLists.txt` : build CMake natif JUCE 8 (sans Projucer)
- `init_vs_env.ps1` : initialisation MSVC + Windows SDK en PowerShell pur
- `build.ps1` : configuration + compilation + tests
- `.gitignore`

**Documentation** :
- `roadmap.md` : suivi complet du projet avec cases a cocher par phase
- `docs/changelogs/changelog-2026-06-09.md` (ce fichier)
- `docs/architecture.md` : documentation complete (PSOLA, formants, retarget)

**Tests unitaires** :
- `test/Main.cpp` : point d'entree des tests
- `test/dsp/PitchDetectorTest.cpp` : 5 cas (440, 220, 100 Hz, silence, buffer trop petit)
- `test/dsp/ScaleQuantizerTest.cpp` : 7 cas (in/out gamme, tonique, chromatique, etc.)
- `test/dsp/RetargetEnvelopeTest.cpp` : 4 cas (Speed=0, Speed=200, cible=1, reset)
- `test/dsp/FormantPreserverTest.cpp` : 3 cas (desactive, ratio=1, ratio extreme)
- `test/dsp/PitchCurveTest.cpp` : 10 cas (vide, 1 point, 2 points, modif, suppression, tri, snap, serialisation, presets, perf)

### Installation
- JUCE 8.0.12 installe dans `C:\JUCE` via `git clone --branch 8.0.12`
- CMake 4.3.3 installe via winget (Kitware.CMake)

### Pipeline DSP complet implemente
1. **Detection YIN** (4 etapes, interpolation parabolique sub-sample)
2. **Mode Auto : Quantification** (5 modes, distance circulaire)
3. **Mode Graphic : PitchCurve editable** (interpolation lineaire, snap, presets)
4. **Selection du mode** (parametre AudioParameterInt 0/1)
5. **Filtre anti-formants** (biquad Butterworth, sqrt compensation)
6. **PSOLA** (pitch marks + OLA Hann 2-periodes, ring buffer)
7. **Retarget Envelope** (IIR 1er ordre pour le Speed style Antares)
8. **Cablage** : AudioIn -> PitchDetector -> [Auto: Quantif | Graphic: PitchCurve] -> Retarget -> FormantPreserver -> PSOLA -> AudioOut
9. **Transport time** lu via `getPlayHead()` (utilise en mode Graphic)
10. **Latence** declaree au host (`getLatencySamples`)
11. **PitchCurve serialisee** dans `getStateInformation()` (sous-element XML `<PITCH_CURVE>`)

### Bloqueur environnement
L'installation Visual Studio 2022 detectee est **incomplete** : pas de
headers STL (dossier `include/` vide), pas de libs Desktop (seulement
`onecore`). CMake configure OK, mais `cl.exe` ne peut rien compiler.

### Action requise cote utilisateur
Reinstaller ou completer Visual Studio 2022 avec le workload
**"Developpement Desktop en C++"** :
1. Ouvrir **Visual Studio Installer**
2. Modifier l'installation VS 2022 Community
3. Cocher l'onglet **"Composants individuels"** :
   - `MSVC v143 - VS 2022 C++ x64/x86 build tools`
   - `Windows 11 SDK` (ou Windows 10 SDK)
4. Cliquer "Modifier" et reinstaller (~5 Go)

Une fois VS repare, lancer :
```powershell
. .\init_vs_env.ps1
.\build.ps1 -RunTests
```

### Prochaines etapes
- Phase 4bis : preservation des transitoires (onset detection)
- Phase 4bis : preservation exacte des formants par LPC
- Phase 4bis : mode Graphic - courbes de Bezier + capture pitch par clic
- Phase 5 : configurer AU/AAX depuis un Mac
- Phase 6 : tests audio subjectifs
- Phase bonus : zoom temporel sur la pitch curve


## Validation de la toolchain VS2022 (apres mise a jour de l'utilisateur)

### Build complet reussi (Release x64)
- CMake configure avec succes (generator Visual Studio 17 2022, A x64)
- Compilation reussie pour VST3 et Standalone
- Binaires produits :
  - Autotune Clone.vst3 (3.5 MB) - plugin VST3 Windows x64
  - Autotune Clone.exe (4.7 MB) - executable standalone

### Corrections appliquees au code source
- **Namespace dsp renomme en tdsp** dans tous les modules DSP
  - Cause : ambiguite avec juce::dsp apporte par le module juce_dsp
    (inclus via JuceHeader.h)
  - Fichiers modifies : ScaleQuantizer, PitchDetector, PitchShifter,
    FormantPreserver, RetargetEnvelope, PitchCurve (h+cpp), PluginProcessor
    (h+cpp), PluginEditor.cpp, PitchCurveEditor (h+cpp), test/dsp/*, docs
- **API JUCE 8 mises a jour** :
  - XmlElement::getFloatAttribute n'existe plus, remplace par
    getDoubleAttribute avec cast (PitchCurve.cpp)
  - juce::Array<T>::resize ne prend plus de valeur initiale en 2e arg,
    remplacement par clear() + dd() en boucle (PitchVisualizer.cpp)
  - AudioProcessor::getLatencySamples() n'est PAS virtuel en JUCE 8,
    suppression du override (PluginProcessor.h)
  - cceptsMidi(), producesMidi(), isMidiEffect() doivent etre
    declares const (PluginProcessor.cpp)
  - juce::Array<T>::indexOf requiert operator== sur T, ajout sur
    PitchPoint (PitchCurve.h)
- **Include manquant** : ScaleQuantizer.h ajoute dans PitchCurve.h
  (necessaire pour le type tdsp::Scale utilise par snapToScale)
- **VST2/VST3 warning** : JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1
  ajoute aux definitions de compilation (CMakeLists.txt) car on ne distribue
  pas de VST2

### Workaround build : generation manuelle de JuceHeader.h
- Le support CMake de JUCE 8 ne genere pas automatiquement JuceHeader.h
  dans toutes les configurations
- Ajout d'une etape dans uild.ps1 qui :
  1. Localise juceaide.exe dans _juce_build/
  2. Genere JuceHeader.h dans Release/ a partir de Defs.txt
  3. Copie le fichier au niveau superieur (JuceLibraryCode/) pour
     correspondre au include path par defaut

### Verification de l'environnement
- MSVC 14.44.35207 (VS 2022 17.14) : OK
- Windows SDK 10.0.19041.0 : OK
- CMake 3.28.6 : OK
- JUCE 8.0.12 dans C:\\JUCE : OK
- cl.exe, link.exe, rc.exe : tous operationnels

### Prochaines etapes
- Tests unitaires : valider avec -RunTests (apres verification du build)
- Proposer a Jerome les idees d'amelioration validees precedemment :
  onset detection, harmonisation automatique, presets supplementaires
  (vocoder, hard tune), documentation utilisateur (docs/user-guide.md)


## Resolution des plantages au lancement et corrections UI

### Crash #1 : violation d'acces `0x38` dans `Rectangle<int>::getX()`
- **Symptome** : `Autotune Clone.exe` se lance mais plante silencieusement
  (fenetre invisible)
- **Cause** : `setSize(800, 600)` et `setResizable(true,true)` etaient
  appeles AVANT la creation de `pitchVisualizer` et `curveEditor`.
  `resized()` est declenche pendant `setSize`, et accedait a ces
  unique_ptr encore null
- **Fix** : deplacer `setSize`, `setResizable`, `setResizeLimits` apres
  le `addAndMakeVisible(*curveEditor)`
- **Fichier** : `Source/PluginEditor.cpp`

### Crash #2 : `AudioProcessor::Bus::isLayoutSupported` (au bout de quelques secondes)
- **Symptome** : le plugin se lance, mais plante apres activation audio
- **Cause** : `AudioProcessor::BusesProperties` n'etait pas precise dans
  le constructeur ; les bus etaient crees avec `isActivatedByDefault=false`
  et une fois l'audio active le host demandait un layout qui n'etait pas
  supporte
- **Fix** : ajouter
  ```cpp
  AudioProcessor::BusesProperties()
      .withInput  ("Input",  AudioChannelSet::stereo(), true)
      .withOutput ("Output", AudioChannelSet::stereo(), true)
  ```
  au ctor de `AutotuneCloneAudioProcessor`
- **Fichier** : `Source/PluginProcessor.cpp`

### Crash #3 : `~HeapBlock` dans le pipeline audio
- **Symptome** : crash pendant la lecture audio, dans le destructeur
  de `juce::HeapBlock<float>`
- **Cause** : `PitchShifter::addGrain` bornait son ecriture par
  `outIdx < N` ou `N = ringBufferSize = 8192`, mais le buffer de sortie
  ne fait que `numSamples = 512` ; l'ecriture debordait silencieusement
  puis le destructeur detectait l'incoherence
- **Fix** : ajouter un parametre `outCapacity` a `addGrain` et utiliser
  `outIdx < outCapacity` ; le caller passe `numSamples`
- **Fichier** : `Source/dsp/PitchShifter.cpp`

### UI : knobs invisibles, drag inactif, key/scale incoherents
- **Knobs non visibles** : la barre du bas etait trop petite (80 px) ;
  fixee a 160 px. La formule de calcul de `centerArea` divisait par
  erreur toute la hauteur au lieu de retirer 160 px : remplace
  `bounds.removeFromBottom(getHeight() - 80)` par
  `bounds.removeFromTop(bounds.getHeight() - 160).reduced(10)`
- **Key/Scale en ComboBox** : un slider rotatif n'a pas de sens pour
  des valeurs discretes. Remplacement par des `juce::ComboBox`
  (C, C#, ..., B / Major, Minor, Pent. Maj, Pent. Min, Chromatic) avec
  binding manuel vers les `AudioParameterInt` via `getRawParameterValue`
- **Drag des points inactif (mode Graphic)** : `mouseDown` et `mouseDrag`
  etaient bien appeles, mais l'ecriture via le setter ne propageait pas
  la nouvelle valeur (la lecture dans `paint()` montrait toujours
  `pt.pitch=12.0`). Cause : `Array<T>::operator[]` retourne une
  *reference* mais la lecture se faisait probablement sur une copie
  temporaire
  - **Fix** : nouvelle methode `setPointPitch(int, float)` dans
    `PitchCurve` qui utilise `points.getReference(index).pitch = pitch`
    pour ecrire directement
  - **Hit-test** elargi de 12 a 30 px pour faciliter la selection
  - **Overlay grise** en mode Auto (texte "Auto mode - read only")
- **Fichiers** : `Source/PluginEditor.cpp`, `Source/PluginEditor.h`,
  `Source/dsp/PitchCurve.h`, `Source/ui/PitchCurveEditor.cpp`,
  `Source/ui/PitchCurveEditor.h`

### Cleanup de l'instrumentation debug
- Supprime la fonction `dbgLog` et tous ses appels dans
  `Source/PluginProcessor.cpp` et `Source/PluginEditor.cpp`
- Supprime la declaration `void dbgLog(...)` dans
  `Source/PluginProcessor.h`
- Supprime les `juce::Logger::writeToLog` et les blocs d'ecriture
  `autotune_paint.log` dans :
  - `Source/PluginEditor.cpp::paint`
  - `Source/ui/PitchCurveEditor.cpp::paint` et `mouseDrag`
  - `Source/ui/PitchVisualizer.cpp::paint`
- Supprime `Source/DebugMinidump.cpp` (SetUnhandledExceptionFilter +
  AddVectoredExceptionHandler) et sa ligne dans `CMakeLists.txt`
  (le sandbox ne pouvait pas ecrire sur `C:\Users\User\Desktop`, donc
  le minidump n'a jamais fonctionne ; approche productive = breakpoints VS)
- Supprime `debug-standalone-no-window.md` (note de session debug)

### Regression connue : PSOLA audible mais distordu
- Les grains PSOLA sont produits sans depassement memoire (post-fix #3)
  mais le son resultant reste distordu (grains perdus, alignement
  approximatif). A reecrire proprement en Phase 4bis (preservation des
  transitoires + onset detection)


## Reecriture complete du PSOLA

### Bugs identifies dans l'implementation precedente
- **Grain = 4*T0 au lieu de 2*T0** : `halfLen = 2*period` rendait le grain
  4 periodes au lieu des 2 attendues, ce qui etalait excessivement le grain
- **Position de synthese completement fausse** : `(m * synthPeriod) %
  ringBufferSize` modulo l'anneau (8192) au lieu du numero d'echantillon
  de sortie reel (typiquement 512) ; la majorite des grains etait
  silencieusement jetee
- **Mapping markA/markB non utilise** : la variable `markB` etait
  calculee puis explicitement mise a `(void)` ; aucun interet reel
- **Pas de continuite entre blocs** : aucune memoire de la position du
  prochain mark de synthese, donc impossible de gerer correctement
  plusieurs blocs successifs

### Nouvelle implementation (PitchShifter.cpp)
- **PitchMark = { absoluteSample, ringIdx }** : on garde le temps absolu
  en plus de l'index dans le ring buffer, ce qui permet de raisonner
  en temps lineaire et de retomber sur le bon echantillon d'entree
- **Grain = 2*T0** : demi-fenetre = T0, fenetre Hann centree sur la mark
  d'analyse, comme specifie dans la litterature PSOLA
- **Synthese par recherche dichotomique** : pour chaque marque de
  synthese au temps `t_out` (intervalle T0'), on retrouve l'analyse
  mark la plus proche via `findClosestAnalysisMark()` (bsearch sur
  `absoluteSample`)
- **Continuite inter-blocs** : `nextSynthMarkSample` conserve la
  position du prochain mark entre les appels a `process()`, et est
  re-aligne sur `blockStart` si jamais on a glisse
- **Normalisation COLA adaptive** : `grainGain = 1 / max(1, 2*T0/T0p)`,
  divise par le nombre reel de grains qui se superposent (1, 2, 4, ...
  selon le ratio)
- **`addGrain` simplifie** : le `gain` passe en parametre est deja
  normalise (pas de division magique par 2 a l'interieur)
- **Renommage `findNextPitchMark` -> `findPeak`** : la fonction cherche
  juste un max, pas un "prochain" mark
- **Passage par zero en passthrough** : si `f0 <= 0` ou `ratio = 1`,
  le ring buffer est quand meme alimente et `nextSynthMarkSample` est
  reinitialise a `totalSamplesWritten` pour eviter un gros saut si
  l'utilisateur reactive le pitch

### Fichiers modifies
- `Source/dsp/PitchShifter.h` : nouveau membre `nextSynthMarkSample`,
  nouvelle struct `PitchMark` avec `absoluteSample`, nouvelle methode
  `findClosestAnalysisMark`, signature de `findNextPitchMark` ->
  `findPeak`
- `Source/dsp/PitchShifter.cpp` : `process()` completement reecrit,
  `addGrain` simplifie, `findPeak` ajoute, `findClosestAnalysisMark`
  ajoute, `prepare` et `reset` mettent a jour `nextSynthMarkSample`

### Resultat attendu
- Pitch shift audible et propre pour `ratio` dans [0.5, 2.0]
- Pas de clicks ni de grains perdus
- Latence inchangee (meme ring buffer de 8192 samples)

### Limites connues (a ameliorer ensuite)
- Pas de preservation des transitoires (onset detection a faire)
- Pour `ratio < 0.5` ou `ratio > 2.0`, le grain devient trop long
  par rapport au hop et l'OLA produit des artefacts
- MPM ou autre detecteur de pitch plus robuste complementaire a YIN
  ameliorerait la stabilite des pitch marks


## Correction du bug de wraparound du ring buffer (PSOLA)

### Symptome rapporte par Jerome
"je n'ai pas l'impression d'entendre ma voix tunée quel que soit le
paramètre modifié, même en mettant à 0ms et amount=1. De plus, si
j'enlève le bypass du visualiseur et que le Amount>0, l'audio
complètement incorrect, hachuré et distordu."

### Cause racine
Dans la reecriture du PSOLA, chaque PitchMark stocke un `ringIdx`
(l'index dans le ring buffer au moment de la detection du pic). Mais
apres ~186 ms (8192 samples a 44100 Hz), le ring buffer wrappe et cet
index devient obsolete. En phase 2, on lit alors de la VIEILLE data
(qui date de plusieurs centaines de ms) au lieu des echantillons
courants. Cela produit :
- Un output incoherent (mix de donnees actuelles et anciennes)
- Des "trous" et des "pics" qui donnent un son hachuré
- Aucune note correcte en sortie (le pitch shift est completement
  perdu dans le bruit)

### Fix
Nouvelle methode `ringPosAt(long long absSample)` qui calcule
dynamiquement la position dans le ring buffer a partir du temps
absolu de la marque :
```cpp
pos = (writeIndex - (totalSamplesWritten - absSample)) mod N
```
Appelee en phase 2 a la place de `analysisMarks[idx].ringIdx`.
Le ringIdx stocke dans la PitchMark est conserve a titre informatif
(utile pour debug) mais n'est plus utilise pour la lecture.

### Verification
- L'index du ring buffer change a chaque echantillon (writeIndex
  incremente). En stockant ringIdx statiquement, on lirait un
  echantillon de plus en plus loin dans le passe. Apres 8192 echantillons,
  on lit un echantillon tres ancien (0 ou N-1) selon le wraparound.
- Avec le calcul dynamique, ringPosAt renvoie systematiquement la
  position de l'echantillon a l'instant absolu `absSample` dans
  l'etat COURANT du ring buffer.

### Fichiers modifies
- `Source/dsp/PitchShifter.h` : declaration de `ringPosAt`
- `Source/dsp/PitchShifter.cpp` : implementation de `ringPosAt`,
  modification de la phase 2 pour l'utiliser au lieu de `ringIdx`


## Correction du bug stereo (canal droit silencieux)

### Symptome rapporte par Jerome
"j'ai toujours l'impression d'entendre ma voix et qu'aucun tunage n'est
appliqué, que la visualisation soit activée ou non. Par contre, si la
visualisation est activé, le son est complètement distordu dans l'oreille
gauche et silencieux, hachuré ou intermittent sur l'oreille droite"

### Cause racine
Dans la phase 2 du PSOLA, la mise a jour de `nextSynthMarkSample`
(= `t_out` apres le while loop) se faisait A L'INTERIEUR de la boucle
sur les canaux. Consequence :
- Canal 0 (gauche) : la synthese s'execute normalement, t_out avance
  jusqu'a `blockEnd + T0p`, puis `nextSynthMarkSample = blockEnd + T0p`
- Canal 1 (droite) : `t_out = nextSynthMarkSample = blockEnd + T0p`,
  la condition `t_out < blockEnd` est FAUSSE, le while loop ne tourne
  JAMAIS, donc le canal droit n'est jamais ecrit
- Le buffer de sortie du canal droit reste a zero (= silence)
- En sortie : oreille gauche = PSOLA (avec l'autre bug de wraparound),
  oreille droite = silence (avec parfois des transitoires quand le
  block etait partiellement traite, d'ou le "hachure/intermittent")

### Bug bonus detecte en meme temps
La detection des pitch marks etait elle aussi a l'interieur de la
boucle canaux. Resultat : la meme pitch mark etait ajoutee N fois
(une fois par canal) dans `analysisMarks`, ce qui :
- Fait grossir la liste 2x plus vite que necessaire
- Le bsearch renvoie toujours la 1ere occurrence (canal 0), donc pas
  d'impact fonctionnel visible, mais c'est du gaspillage

### Fix
- Phase 1 refactoree : on ecrit TOUS les canaux dans le ring buffer
  (memoire de position partagee), puis on detecte les pitch marks UNE
  SEULE fois sur le canal 0
- Phase 2 refactoree : `synthStart` est calcule une fois avant la
  boucle canaux, `t_out` est une variable LOCALE dans la boucle while,
  et `nextSynthMarkSample` n'est mis a jour qu'apres la boucle (et
  systematiquement, pour eviter toute derive)

### Fichiers modifies
- `Source/dsp/PitchShifter.cpp` : refactoring des phases 1 et 2


## Correction off-by-one sur absoluteSample des pitch marks

### Symptome rapporte par Jerome
"Plus de problème de stéréo. Par contre, toujours les glitch sur
l'audio lorsque le visualiseur est activé. J'ai remarqué que le
problème n'est que lorsqu'il y a une ligne rouge, donc un out-of-tune
détecté et normalement corrigé. J'entends toujours ma voix, mais la
version qui est censée être tunée est 100% distordue."

### Cause racine
Dans la phase 1 du PSOLA, apres avoir ecrit un echantillon et
incremente `writeIndex` + `totalSamplesWritten`, on ajoutait la pitch
mark avec `analysisMarks.add({ totalSamplesWritten, ... })`. Mais
`totalSamplesWritten` represente le PROCHAIN echantillon a ecrire, pas
le DERNIER ecrit. Le dernier ecrit est a l'instant `totalSamplesWritten
- 1` en position `writeIndex - 1`.

Consequence pour `ringPosAt()` :
- Formule : pos = (writeIndex - totalSamplesWritten + absSample) mod N
- Avec absoluteSample = totalSamplesWritten (bug) : pos = writeIndex
  (la position de la PROCHAINE ecriture = vieille data de N samples
  en arriere, jusqu'a 186 ms)
- Avec absoluteSample = totalSamplesWritten - 1 (fix) : pos = writeIndex
  - 1 (la position du DERNIER ecrit = donnee courante)

Effet audio : pour les notes in-scale (ratio = 1.0), le code prend la
branche passthrough, donc rien ne passe par le PSOLA. Pour les notes
out-of-scale (ratio != 1.0), le PSOLA lit de la vieille data dans le
ring buffer a partir de ~186 ms, ce qui produit un mix incoherent de
donnees actuelles et anciennes -> "100% distordu" rapporte par Jerome.

### Fix
- `analysisMarks.add({ totalSamplesWritten - 1, markRingPos })` au
  lieu de `analysisMarks.add({ totalSamplesWritten, markRingPos })`
- `ringPosAt` conserve sa formule : pos = writeIndex - totalSamplesWritten
  + absSample, qui est maintenant correcte puisque absoluteSample
  designe bien le temps du dernier ecrit

### Notes
- L'offset de T0/2 entre la position de detection (markRingPos) et le
  temps "totalSamplesWritten - 1" est acceptable : la fenetre Hann de
  longueur 2*T0 est suffisamment large pour absorber un decalage de
  quelques % du grain.
- Pour une version "exacte", on pourrait stocker l'offset exact du
  pic par rapport au dernier ecrit, mais cela complexifie le code
  pour un gain audio negligeable.

### Fichier modifie
- `Source/dsp/PitchShifter.cpp` : phase 1, ajout de "- 1" sur
  totalSamplesWritten lors de l'ajout de la pitch mark





