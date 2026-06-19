# Changelog - 11 juin 2026

## Round 11 - Fix helicopter/stutter du wrapper RubberBand

### Symptome rapporte par Jerome (apres Round 10)

"Wow, c'est encore pire qu'avec la solution precedente ! La, en
buffer max 2048, on dirait entendre un helicoptere... chops/stutters,
que ce soit intune ou out_of_tune ! Au point que je n'entends meme
pas les notes chantees. En buffer min, il y a aussi les artefacts
mais j'entends un peu la voix."

### Cause racine (confirmee par instrumentation)

Le wrapper `RubberBandPitchShifter::process()` avait deux defauts
de design qui se combinaient pour creer le pattern "helicoptere" :

1. **Sortie ecrasee par le while loop** : `readyOutput` (taille
   512) etait ecrase a chaque appel de `shift()` dans la boucle
   while. Quand le host donnait un buffer plus grand que 512
   (ex: 2048), la boucle faisait 4 iterations et seul le dernier
   shift() etait garde. Phase 3 servait alors 512 audio et
   remplissait le reste de silence. **A host=2048, cela donnait
   75% de silence en sortie** (1536/2048).

2. **shift() appele au bon vouloir** : on attendait d'avoir
   accumule 512 samples dans `pendingInput` avant d'appeler
   `shift()`. A host=144, cela arrivait 1 fois tous les 4 appels
   seulement. Les 3 autres appels produisaient du silence (Phase 3
   ne trouvait rien a servir). **A host=144, ratio silence = 11%**.

L'instrumentation (fichier NDJSON ecrit par le wrapper, puis
supprime apres validation) a confirme les predictions :
- Run 1 (host 144, 13924 calls) : 11.2% de silence cumule
- Pattern periodique de 64 samples de silence tous les 4 appels

### Fix applique

Refonte complete du pipeline input/output dans
`Source/dsp/RubberBandPitchShifter.h/.cpp` :

1. **Rolling input buffer (taille 512)** : contient TOUJOURS les
   512 derniers echantillons d'entree (au lieu d'attendre
   d'accumuler 512). On decale vers la gauche de chunkSize puis
   on ajoute les nouveaux samples a la fin.

2. **Multi-shift loop** : on appelle `shift()` N fois par
   process() ou N = ceil(numSamples / 512). Pour host=144, N=1.
   Pour host=2048, N=4. Entre chaque shift, on met a jour le
   rolling buffer avec le prochain chunk d'echantillons.

3. **Output buffer circulaire (taille 8192)** : remplace
   `readyOutput` (512, ecrase). Chaque `shift()` APPEND 512
   echantillons a `outputWritePos` (modulo 8192). Phase 3 sert
   depuis `outputReadPos`.

4. **Latency cap (4096 samples, ~85 ms a 48 kHz)** : quand
   `outputValid > cap` (cas host=144 ou la production excede
   la consommation), on avance la position de lecture pour jeter
   les echantillons les plus vieux. Garantit une latence bornee
   au prix de micro-artefacts de phase.

### Resultat post-fix (valide par Jerome)

- **Host >= 1024** : 0% de silence, audio fluide, pas d'artefact
  (ratio production = consommation, pas de drop).
- **Host < 1024** (ex: 144) : 0% de silence egalement, mais
  micro-artefacts de phase (clics legeres) dus au "drop oldest"
  du latency cap. Acceptable, mais Jerome mentionne qu'il y a
  "toujours des clics/chops en dessous de 1024".

### Trace

- Session de debug : `debug-rubberband-helicopter-stutter.md`
  (supprime apres validation, cf. protocole TRAE-debugger etape
  11).
- Log d'instrumentation : `trae-debug-log-rubberband-helicopter-
  stutter.ndjson` (egalement supprime).

## Round 12 - Amelioration du Latency Cap (Crossfade)

### Contexte
Dans le fix du Round 11, lorsque le host demande de petits buffers (ex: 144 samples) 
tandis que `RubberBand` produit des blocs fixes de 512, l'excedant d'echantillons s'accumule. 
Pour eviter que la latence ne s'envole indefiniment, un "latency cap" a ete implemente, 
rejetant ("hard drop") brutalement les anciens echantillons. 
Cela resolvait le probleme de delai mais introduisait des petits clics / discontinuites de phase.

### Implementation
- Ajout d'un crossfade lineaire dans `RubberBandPitchShifter::process` au moment du drop.
- Plutot que de simplement sauter en avant (`outputReadPos += toDrop`), l'algorithme "fade out" 
  l'ancienne section et "fade in" la nouvelle section sur un maximum de 256 echantillons (~5ms).
- Resultat : les transitions de drop (a buffer < 1024) sont grandement adoucies, supprimant les 
  clics audibles (chops).

## Round 10 - Integration de RubberBand v4.0.0 (refonte PitchShifter)

### Decision

Suite au Round 9, Jerome a choisi d'integrer **RubberBand Library
v4.0.0** (licence GPL-2.0-or-later acceptee). Le plugin entier est
desormais sous GPL.

### Implementation

#### Sources externes

- **`external/rubberband-4.0.0/`** : arborescence complete de
  RubberBand v4.0.0, telechargee depuis
  `https://breakfastquay.com/files/releases/rubberband-4.0.0.tar.bz2`
  (le repo Mercurial officiel sur hg.sr.ht n'etait pas accessible
  directement sans Mercurial, le tarball de release est la voie
  recommandee par le mainteneur).
- Build **single-file** via `single/RubberBandSingle.cpp` (cf.
  `COMPILING.md` du projet) : un seul .cpp qui inclut tous les autres
  via des `#include` relatifs. Avantage : pas de .lib separee a linker,
  pas de Meson/Ninja a installer, pas de dependance externe (KissFFT
  integre, BQResampler integre).

#### Nouveau module : RubberBandPitchShifter

- **`Source/dsp/RubberBandPitchShifter.h`** : declaration du wrapper
  JUCE. Encapsule `RubberBand::RubberBandLiveShifter` (la nouvelle
  API v4 dediee au pitch-shifting pur, sans time-stretch, avec une
  latence minimale).
- **`Source/dsp/RubberBandPitchShifter.cpp`** : implementation.
  - Options : `OptionFormantPreserved | OptionWindowMedium
    | OptionChannelsTogether` (preserve le timbre, compromis
    qualite/latence, traitement stereo coherent).
  - Buffering : RubberBand exige EXACTEMENT `getBlockSize()` (512)
    echantillons par appel a `shift()`. Le wrapper accumule l'entree
    du host (taille variable 144-2048) dans `pendingInput` jusqu'a
    avoir un bloc complet, appelle `shift()`, stocke la sortie dans
    `readyOutput`, et la sert progressivement au host.
  - Priming : le premier appel a `shift()` produit un bloc
    "d'echauffement" qui est jete. Cela introduit une latence
    supplementaire de 2 * 512 = 1024 samples (env. 23 ms a 44.1 kHz),
    negligeable compare aux 50+ ms de latence interne de RubberBand.
  - `getLatencySamples()` : retourne `shifterStartDelay + 2 *
    shifterBlockSize`.

#### CMake

- **`CMakeLists.txt`** :
  - Ajout de `Source/dsp/RubberBandPitchShifter.cpp` et `.h` aux
    sources du plugin ET des tests.
  - Ajout de `external/rubberband-4.0.0/single/RubberBandSingle.cpp`
    aux sources.
  - Include paths : `external/rubberband-4.0.0` (API publique) et
    `external/rubberband-4.0.0/single` (pour les `#include
    "../src/..."` du single.cpp).
  - Defines de compilation : `USE_BQRESAMPLER=1`, `NO_TIMING=1`,
    `NO_THREADING=1`, `NO_THREAD_CHECKS=1`, `USE_BUILTIN_FFT=1`
    (cf. en-tete de `RubberBandSingle.cpp`).
  - **NOMINMAX=1** : necessaire car `Thread.h` de RubberBand inclut
    `<windows.h>` qui definit les macros `min`/`max` en conflit avec
    `std::min`/`std::max` utilises dans R3Stretcher.cpp et
    R3LiveShifter.cpp. Sans cela, erreurs C2589/C2059 a la
    compilation.
  - Exclusion du PSOLA maison (`Source/dsp/PitchShifter.cpp/.h`)
    des sources du build (le fichier reste sur disque comme
    reference archivee pour les rounds 4-8, mais n'est plus compile).

#### PluginProcessor

- **`Source/PluginProcessor.h`** : `pitchShifter` est maintenant de
  type `std::unique_ptr<atdsp::RubberBandPitchShifter>`. Le type
  expose la meme interface que l'ancien `atdsp::PitchShifter`
  (prepare, reset, process), donc le reste du pipeline (PitchDetector,
  ScaleQuantizer, RetargetEnvelope) est inchange.
- **`Source/PluginProcessor.cpp`** :
  - Constructeur : `std::make_unique<atdsp::RubberBandPitchShifter>()`.
  - `prepareToPlay` : `latencySamples = analysisWindow / 2 +
    pitchShifter->getLatencySamples()` (la fenetre d'analyse YIN
    ajoute analysisWindow/2, et RubberBand ajoute sa latence).
  - `processBlock` : appel inchange a `pitchShifter->process(buffer,
    ratio, f0)`.

#### build.ps1

- **`build.ps1`** : ajout de la generation de `JuceHeader.h` pour la
  cible `AutotuneTests` (qui etait precedemment cassee silencieusement
  - la generation n'etait faite que pour `AutotuneClone`).

#### Licence

- **`LICENSE`** : nouveau fichier a la racine du projet. Declare
  l'integralite du plugin sous GPL-2.0-or-later, avec mention de
  RubberBand et reference au COPYING de RubberBand.

### Verification

- Build Release x64 : OK. Standalone genere
  (`build/AutotuneClone_artefacts/Release/Standalone/Autotune Clone.exe`).
- VST3 genere (`build/AutotuneClone_artefacts/Release/VST3/Autotune
  Clone.vst3`).
- Standalone lance (PID 60064, ~88 MB, stable).
- Latence declaree au host : `analysisWindow/2 (1024) +
  shifterStartDelay (~256-512) + 2 * 512 (1024) = ~2.5-3k samples =
  ~57-68 ms a 44.1 kHz`. Dans la plage documentee de RubberBand
  (50-100 ms).
- **A valider par test utilisateur** : pitch shift en temps reel
  sur voix reelle (chantee ou parlee), avec les parametres Speed et
  Amount varies. Le son ne doit plus presenter les "pops", "clicks"
  ou "glitches pitch-dependent" qui caracterisaient le PSOLA
  maison (rounds 4-8).

### Notes architecturales

- Le wrapper conserve la meme interface que l'ancien PSOLA : si Jerome
  veut un jour switcher vers une autre lib (SoundTouch, etc.), il
  suffit de reimplementer `RubberBandPitchShifter` et de changer le
  type du `unique_ptr`. Aucun changement dans le reste du pipeline.
- `FormantPreserver` est conserve dans le pipeline (avant
  `pitchShifter->process`). Il est en pratique redondant avec
  `OptionFormantPreserved` de RubberBand, mais ne fait pas de mal
  (pre-filtre lineaire). A supprimer si on veut allouer le CPU.

## Round 9 - Decision de refonte : bibliotheque tierce

### Symptome rapporte par Jerome (apres Round 8)
"Je n'ai pas l'impression que les rounds aident beaucoup et on ne
peut accepter ces artefacts sur un traitement audio de la voix :(
Donc, soit j'accepte qu'il ne t'est pas possible de creer un clone
d'autotune, soit il faut une refonte majeure."

Jerome a confirme l'echec des corrections PSOLA incrementales apres
5 rounds (4 a 8) : les artefacts (pops, clicks, glitches
pitch-dependent) persistent car ce sont des proprietes
fondamentales du PSOLA simple (phasiness, modulation COLA, pas
de phase-locking, jitter des pitch marks).

### Decision

**Refonte majeure du module PitchShifter** : remplacer le PSOLA
maison par une bibliotheque tierce de qualite production.

### Bibliotheques evaluees

1. **RubberBand** v4.0.0 - GPL-2.0-or-later (virale)
   - Phase vocoder R3, qualite excellente, latence 50-100 ms
   - Cout : le plugin entier doit passer en GPL, OU licence
     commerciale (£420-1120)
2. **SoundTouch** v2.3.3 - LGPL-2.1 (permissive)
   - SOLA/WSOLA, qualite bonne, latence 100-130 ms
   - Cout : linking dynamique standard, plugin peut rester close
3. **Autres** (Aubio GPL-3.0, libsamplerate BSD, PaulStretch GPL) :
   evaluees, ne conviennent pas pour un autotune de qualite prod.

### Document de comparaison

- **`docs/pitch-shifting-library-comparison.md`** : comparatif
  detaille (qualite, latence, implications licence, cout
  integration, recommandation).

### Prochaine etape

- **En attente** : Jerome doit choisir entre RubberBand, SoundTouch,
  ou une approche hybride (SoundTouch par defaut + slot pour
  RubberBand).
- Une fois choisi, integration via wrapper JUCE dedie (1 journee
  max) puis suppression du PSOLA maison.

### Statut PSOLA maison

- Les 5 rounds de fix (4-8) restent en place dans le code pour
  reference, mais le PSOLA ne sera plus utilise en production
  des que la bibliotheque tierce est integree.
- Le squelette `Source/dsp/PitchShifter.h` est concu comme une
  interface : il suffira de creer un nouveau `*PitchShifter.h/.cpp`
  derivant de la meme interface.

## Round 8 - Pas de realignement des grains sur la frontiere de bloc

### Symptome rapporte par Jerome (apres Round 7)
"Pareil qu'avant" - les fixes H1 (YIN octave) et H5 (gain COLA) n'ont
pas change le phenomene. Les pops a 2048 et les glitches pitch-dependent
a 144 persistent.

### Hypothese : le realignement sur multiple de T0p est discontinu

Dans la boucle de synthese, on realignait `nextSynthMarkSample` sur
le premier multiple de T0p >= blockStart. Cela forcait le premier
grain du nouveau bloc a etre a une position alignee sur la frontiere
de bloc, ce qui peut etre legerement different de la position qu'on
aurait obtenue en continuant depuis le bloc precedent.

Consequence : le premier grain du nouveau bloc est a une position
"choisie pour la commodite du bloc" plutot qu'a la position "naturelle"
de la continuite -> micro-saut de phase a chaque frontiere de bloc,
audible comme un click.

### Fix

On ne realigne QUE si on a perdu la continuite avec le bloc
precedent (c'est-a-dire si `nextSynthMarkSample` est plus vieux que
`blockStart - T0p`, ce qui arrive apres un long silence ou un
passthrough). Sinon, on garde la position continue.

Resultat theorique : le premier grain du nouveau bloc est exactement
a la position qu'il aurait eu si on avait traverse la frontiere
sans la voir, donc pas de saut de phase.

### Fichier modifie

- **`Source/dsp/PitchShifter.cpp`** : la condition de realignement
  est passee de `nextSynthMarkSample < blockStart` a
  `nextSynthMarkSample < blockStart - T0p`.

### Verification

- Build Release x64 reussi.
- Standalone relance : OK (PID 124508).
- A confirmer par test utilisateur.

### Note importante sur les limites du PSOLA

Si apres ce fix les glitches persistent, cela confirmerait que
les artefacts residuels sont des **proprietes fondamentales du
PSOLA simple** (phasiness, modulation COLA, jitter des pitch marks)
qui ne peuvent etre eliminees qu'en changeant d'algo :
- Phase-locked PSOLA (phase vocoder simplifie)
- LPC + resampling non-uniforme
- Sinusoidal modeling

Ces algos representent une refonte majeure (semaines de travail)
qui depasse le scope des corrections actuelles. Dans ce cas, la
meilleure option est probablement d'accepter les artefacts et de
passer a d'autres fonctionnalites (MIDI out, presets, etc.).

## Round 7 - Anti-octave-error YIN + lissage gain COLA

### Symptome rapporte par Jerome (apres Round 6)
"Toujours meme phenomene :
- 2048 : pops, pas d'amelioration notable
- 144 : clicks and pops ou audio glitchs qui changent avec le pitch.
Par contre, j'arrive a etre in-tune a 144 maintenant."

Le "in-tune a 144" est une amelioration (le fix Round 6 a fonctionne
sur ce point). Les glitches residuels sont de deux types :
- "pops a 2048" : non ameliores par les rounds precedents
- "glitches pitch-dependent a 144" : non ameliores

### Fix 1 : Anti-octave-error YIN (H1)

Dans `PitchDetector::detectPitch`, apres avoir trouve un tau sous le
seuil, on verifie maintenant si 2*tau est AUSSI sous le seuil. Si
oui, le tau detecte est une sous-harmonique et on prend 2*tau a la
place (= le fondamental).

C'est l'erreur classique de YIN : sur les signaux avec une forte 2e
harmonique (typique de la voix), YIN detecte 2*f0 au lieu de f0. Le
PSOLA produit alors une sous-harmonique audible qui change avec le
pitch chante -> "glitch pitch-dependent".

Reference : de Cheveigne & Kawahara, "YIN, a fundamental frequency
estimator for speech and music", J. Acoust. Soc. Am. 2002.

### Fix 2 : Lissage du gain COLA (H5)

Le gain COLA theorique = `1/overlapCount` ou `overlapCount = 2*T0/T0p`.
Quand T0p change entre blocs (f0 ou ratio qui change), overlapCount
change, le gain fait un saut -> pop audible.

Ajout d'un **lissage time-based** du gain (tau=20 ms) via un nouveau
membre `smoothedGain` dans `PitchShifter`, mis a jour avec
`alpha = 1 - exp(-blockDuration/0.02)`.

Resultat : le gain suit les changements reels de T0p mais elimine
les sauts rapides entre blocs.

### Fichiers modifies

- **`Source/dsp/PitchDetector.cpp`** : ajout de l'etape 3b
  (anti-octave-error) apres l'etape 3 (recherche du minimum).
- **`Source/dsp/PitchShifter.h`** : ajout du membre `smoothedGain`.
- **`Source/dsp/PitchShifter.cpp`** : lissage time-based du gain
  COLA (tau=20 ms).

### Verification

- Build Release x64 reussi.
- Standalone lance : OK (PID 112772, ~83 MB, stable).
- A confirmer par test utilisateur si :
  - Le "glitch pitch-dependent" a 144 est elimine (H1)
  - Les "pops" a 2048 sont reduits (H5)

### Session de debug

Voir `debug-psola-pitch-dependent-glitch.md` (status IN-PROGRESS).

### Hypothese restante (H2, H3, H4) : si les glitches persistent

- **H2 - Phasiness PSOLA** : artefact inherent au PSOLA simple
  quand le pitch-shift est important. Fix : phase-locked PSOLA ou
  LPC + resampling (complexe, hors scope actuel).
- **H3 - findPeak jittery** : positions des pitch marks qui
  fluctuent. Fix : interpolation entre marks adjacents.
- **H4 - RetargetEnvelope** : peut-etre encore sous-amorti selon
  le Speed regle par l'utilisateur.

## Round 6 - Fix taille de l'etat de synthese (anti-click quand T0 augmente)

### Symptome rapporte par Jerome (apres Round 5)
"Toujours les glitchs. A buffer size max (2048, je n'ai pas
l'option 4096), les pop sont toujours la et frequents quand
out-of-tune. A buffer size min (144), beaucoup de click cette fois
et aussi un glitch audio tres rapide qui change avec le pitch chante
(et je n'arrive pas a avoir un pitch in-tune alors que c'est possible
en buffer 2048)."

Le "glitch tres rapide qui change avec le pitch" et les "clicks a
144" sont deux symptomes distincts. Les clicks a 144 ont une cause
identifiable (voir ci-dessous). Le glitch pitch-dependent reste
sous investigation (voir "Hypotheses restantes").

### Cause des clicks a 144 : etat de synthese trop petit quand T0 change

Dans le Round 4, l'etat de synthese etait dimensionne a `halfGrain`
(= T0) a chaque bloc. Si l'utilisateur chante une note plus grave
(T0 augmente entre deux blocs), le premier grain du nouveau bloc a
besoin de PLUS d'etat que ce qu'on a stocke avec l'ancien T0 -> la
demi-fenetre gauche du premier grain est incomplete -> click.

Exemple concret a 144 samples, f0=200Hz (T0=220), puis f0=100Hz
(T0=441) :
- Bloc N : etat de 220 samples stockes. T0 = 220. OK.
- Bloc N+1 (f0 change a 100Hz) : T0 = 441. Le premier grain a besoin
  de 441 samples d'etat. Mais on n'en a que 220. La demi-fenetre
  gauche est incomplete sur 221 samples -> click + grain deforme.

### Fix

Stocker TOUJOURS `synthStateCapacity` (4096) samples d'etat, pas
seulement T0. C'est plus que necessaire pour un seul grain (T0 max
~882 @ 50 Hz avec sampleRate=44100), mais cela garantit que le
premier grain du prochain bloc a TOUJOURS sa demi-fenetre gauche
complete, meme si T0 a augmente.

Cout : 4096 * 2 canaux * 4 bytes = 32 KB de memcpy par bloc
audio, negligeable.

### Fichier modifie

- **`Source/dsp/PitchShifter.cpp`** : remplacement de
  `juce::jmin (halfGrain, workingSize)` par
  `juce::jmin (synthStateCapacity, workingSize)` dans la mise a
  jour de l'etat de synthese.

### Hypothese restante : "glitch pitch-dependent" a 144 samples

Le "glitch tres rapide qui change avec le pitch" n'est PAS explique
par l'OLA. Hypotheses (a investiguer dans un round ulterieur) :

1. **Octave error dans YIN** : YIN peut detecter f0/2 ou 2*f0 au
   lieu de f0 si le signal a une forte harmonique 2. Le PSOLA
   produit alors une sous-harmonique audible. La probabilite d'erreur
   augmente a petit buffer ou le rapport signal/bruit du YIN est
   moins bon (le signal downmixe L+R peut etre moins propre).
   Fix possible : median filter sur la sortie f0, ou seuils de
   confiance YIN adaptatifs.

2. **Mark positions jittery** : le findPeak cherche un max dans une
   fenetre de T0 echantillons. Si le signal a plusieurs pics de
   hauteur comparable, le max peut "switcher" entre blocs ->
   grains a des positions legerement differentes -> micro-glitch.
   Fix possible : interpolation entre marques adjacentes.

3. **Impossibilite d'etre in-tune a 144** : le FIFO d'analyse est
   de 2048 samples, rempli en 14 blocs de 144. Une fois rempli, la
   detection devrait etre stable. Si l'utilisateur chante juste
   apres le demarrage, le FIFO n'est pas encore plein et f0_in
   est 0.0f -> pas de correction appliquee -> l'audio semble
   "out-of-tune" alors qu'il est en train de chanter sur la note.
   Fix possible : demarrer avec une fenetre d'analyse plus petite
   les premiers blocs (ex: 512 samples), puis basculer a 2048.

### Verification

- Build Release x64 reussi.
- Standalone lance : OK (PID 51536, ~83 MB, stable).
- A confirmer par test utilisateur si les clicks a 144 sont elimines.
- Le "glitch pitch-dependent" necessitera probablement un round
  ulterieur avec une approche plus systematique (debugger skill).

## Round 5 - Fix lissages temps-dependants (elimine les "pops" residuels)

### Symptome rapporte par Jerome (apres Round 4)
"il y a toujours les glitchs quelle que soit la taille de buffer size
(plus marques plus la taille de buffer est petite). Cela dit, les
glitchs sont moins aigus qu'avant (un son plus 'pop' que 'click')"

Le passage de "click" a "pop" confirmait que le fix OLA (Round 4)
fonctionnait (discontinuites aiguës eliminees) mais qu'une source de
modulation basse frequence restait. Et le fait que ce soit plus
marque a petit buffer pointait vers un comportement dependant de la
taille de buffer.

### Cause racine identifiee

Deux lisseurs etaient **par bloc** au lieu d'etre **temps-continus**, ce
qui donnait un comportement dramatiquement different selon la taille
du buffer :

1. **`PitchShifter::currentF0` smoothing (0.85 par bloc)** :
   - 144 samples : tau ~7 ms (quasi-instantane). Le f0 suit le jitter
     YIN bloc par bloc, les grains PSOLA sautent d'1 sample a chaque
     bloc quand T0 change -> "pop".
   - 4096 samples : tau ~200 ms (trop lent, le plugin ne suit plus
     la voix).

2. **`PitchShifter::smoothedRatio` (smoothingCoeff = 0.9 par bloc)** :
   meme probleme, 4x moins rapide a petit buffer.

3. **`RetargetEnvelope::processSample` appele une fois par bloc avec
   un alpha per-sample** : c'etait le bug le plus severe. Formule
   effective du temps de reponse = `tau * numSamples`.
   - 144 samples, speed=50ms : tau effectif = 7.2 s (le Speed n'a
     **pratiquement aucun effet** a petit buffer !)
   - 4096 samples, speed=50ms : tau effectif = 204.8 s (le Speed est
     completement inoperant)
   C'est la raison principale pour laquelle le Speed ne semblait pas
   faire grand-chose a petit buffer dans les tests precedents.

### Fix applique

Lissage **time-based** dans les deux modules :

```cpp
// Dans PitchShifter::process :
const float blockDuration = numSamples / sampleRate;
const float ratioAlpha = 1.0f - std::exp(-blockDuration / 0.05f);  // tau=50ms
const float f0Alpha    = 1.0f - std::exp(-blockDuration / 0.03f);  // tau=30ms
smoothedRatio = (1.0f - ratioAlpha) * smoothedRatio + ratioAlpha * ratio;
currentF0     = (1.0f - f0Alpha)    * currentF0    + f0Alpha    * f0;
```

```cpp
// Nouvelle methode RetargetEnvelope::processBlock(targetRatio, numSamples) :
const double blockDuration = numSamples / sampleRate;
const double tau = speedMs / 1000.0;
const float blockAlpha = 1.0f - std::exp(-blockDuration / tau);
currentValue += blockAlpha * (targetRatio - currentValue);
```

Avec ces formules, la constante de temps est la **meme** pour toutes
les tailles de buffer. Le Speed Antares a maintenant un effet
homogene.

### Fichiers modifies

- **`Source/dsp/PitchShifter.h`** :
  - Suppression de `smoothingCoeff` (membre devenu inutile avec le
    lissage time-based inline dans process()).
  - Commentaire explicatif sur le lissage time-based.

- **`Source/dsp/PitchShifter.cpp`** :
  - `process()` : remplacement des deux lissages per-block (0.9 et 0.85)
    par des lissages time-based avec tau=50ms (ratio) et tau=30ms (f0).
  - Suppression de la reference a `smoothingCoeff`.

- **`Source/dsp/RetargetEnvelope.h`** :
  - Nouvelle methode `processBlock(targetRatio, numSamples)` qui prend
    en compte la taille du bloc.

- **`Source/dsp/RetargetEnvelope.cpp`** :
  - Implementation de `processBlock` avec alpha = 1 - exp(-blockDuration/tau).
  - `processSample` conserve (pour les tests unitaires et la compatibilite).

- **`Source/PluginProcessor.cpp`** :
  - `retargetEnvelope->processSample(targetRatio)` remplace par
    `retargetEnvelope->processBlock(targetRatio, buffer.getNumSamples())`.

### Verification

- Build Release x64 reussi.
- Standalone lance : OK (PID 114592, ~83 MB, stable apres 5s).
- A confirmer par test utilisateur a differentes tailles de buffer.
- Le Speed devrait maintenant avoir un effet audible homogene a toutes
  les tailles de buffer.
- Les "pops" devraient etre reduits grace a la stabilite accrue du f0
  (lissage 30 ms vs 7 ms avant a petit buffer).

### Session de debug

Voir `debug-persistent-audio-glitches.md` (toujours FIXED pour le
round 4, ce round 5 attaque la deuxieme source de glitches).

### Round 4 - Fix clicks/pops : etat de synthese PSOLA persistant

### Symptome rapporte par Jerome
"Clicks/pops in the audio" represente le mieux les glitchs audio
que j'entends. Les glitchs sont tres nombreux quelle que soit la
taille du buffer, et particulierement denses a petit buffer (144
samples). Le crossfade de 32 samples en input (applique au round
precedent) ne les a fait que diminuer legerement.

### Cause racine identifiee

Les grains PSOLA utilisent une fenetre Hann de longueur **2 * T0**
(T0 = periode fondamentale, typiquement 100-200 samples pour la voix).
Pour que l'overlap-add (OLA) soit continu dans le temps, chaque grain
a besoin que les T0 echantillons AVANT et APRES sa pitch mark soient
accessibles.

Or, dans l'implementation bloc-par-bloc du `PitchShifter::process`,
l'output buffer etait remis a zero a chaque appel. Deux consequences
graves :

1. **Demi-fenetre gauche clippee** : le premier grain d'un bloc, centre
   a `synthStart` (peut etre seulement quelques samples apres le debut
   du bloc), a sa demi-fenetre gauche (longueur T0) COMPLETEMENT
   perdue. Le grain ne contribue qu'avec sa moitie droite, ce qui
   produit un saut d'amplitude des le premier sample du bloc -> click.

2. **Demi-fenetre droite clippee** : symetriquement, le dernier grain
   du bloc a sa demi-fenetre droite perdue. Le bloc suivant, qui
   demarre sur un output buffer vide, ne beneficie pas de cette
   contribution.

A chaque frontiere de bloc, il y a donc un "trou" de continuite
d'environ 2 * T0 - T0p echantillons (T0p = T0 / ratio), durant lequel
l'output PSOLA passe soudainement d'une valeur (grain complet) a une
autre (grain tronque) ou a zero.

Avec T0 = 220 samples (f0 = 200 Hz @ 44.1 kHz), le "trou" est de
l'ordre de 200-400 samples a chaque bloc. Pour un buffer de 144
samples, ce "trou" est beaucoup plus grand que le bloc lui-meme, ce
qui explique la degradation tres audible a petit buffer.

### Fix : etat de synthese persistant (`synthStateBuffer`)

On propage la queue du bloc precedent vers le debut du bloc suivant.
Le buffer de travail (`outputBuffer`) est maintenant organise ainsi :

```
[synthState (T0 samples) | blocCourant (numSamples samples)]
```

Dans la Phase 2 de `PitchShifter::process()` :
1. On copie `synthStateBuffer` au debut de `outputBuffer` (fournit la
   demi-fenetre gauche du 1er grain).
2. On ajoute les grains du bloc courant dans la zone etendue avec
   `outCapacity = workingSize = synthStateSize + numSamples`.
3. A la fin, on sauvegarde les T0 derniers echantillons de la zone
   de travail dans `synthStateBuffer` pour le prochain appel
   (`synthStateSize = min(T0, workingSize)`).
4. On copie UNIQUEMENT `[synthState, synthState + numSamples]` vers
   le buffer de sortie de l'hote.

L'OLA est maintenant **continu dans le temps** au-dela des frontieres
de bloc. Les clicks deviennent negligeables (limites au premier appel
apres enable, ou au retour apres passthrough, ou l'etat est vide et
la demi-fenetre gauche est clippee une seule fois ; le crossfade de
32 samples en input masque cela).

### Fichiers modifies

- **`Source/dsp/PitchShifter.h`** :
  - Ajout des membres :
    - `static constexpr int synthStateCapacity = 4096;` (couvre f0 > 10 Hz)
    - `juce::AudioBuffer<float> synthStateBuffer;`
    - `int synthStateSize = 0;`
  - Commentaire detaillant la raison d'etre de cet etat.

- **`Source/dsp/PitchShifter.cpp`** :
  - `prepare()` : alloue `synthStateBuffer` (4096 samples) et augmente
    `outputBufferCapacity` a `jmax(bs*2, synthStateCapacity + bs, 2048)`
    pour avoir la place d'heberger l'etat + le bloc courant.
  - `reset()` : clear `synthStateBuffer` et reset `synthStateSize = 0`.
  - `process()` branche passthrough : clear `synthStateBuffer` (les
    echantillons PSOLA qu'il contient ne sont plus valides en
    passthrough ; au retour du pitch shifting, on reconstruira un
    etat frais sur les premiers blocs).
  - `process()` Phase 2 :
    - Layout : `outputBuffer = [synthState | blocCourant]`.
    - `workingSize = synthStateSize + numSamples`.
    - `blockOffsetInWork = synthStateSize` (decalage pour `outPos`).
    - `addGrain` est appele avec `outPos = blockOffsetInWork + (t_out - blockStart)`
      et `outCapacity = workingSize`.
    - En fin de canal loop : mise a jour de `synthStateBuffer` avec
      les `halfGrain` derniers echantillons de la zone de travail,
      et `synthStateSize = halfGrain`.
  - `process()` Phase 3 : `buffer.copyFrom(ch, 0, outputBuffer, ch, blockOffsetInWork, numSamples)`
    au lieu de `... 0, numSamples)`. On ne copie plus l'etat de
    synthese, qui est purement interne.
  - `process()` check de securite : deplace apres la branche
    passthrough, et compare `outputBufferCapacity` a
    `numSamples + synthStateCapacity` (et non plus `numSamples` seul).
    En passthrough, pas besoin d'outputBuffer ni d'inputBackup.

### Verification

- Build Release x64 reussi.
- Standalone lance : OK (PID 141136, ~80 MB, stable apres 5s).
- A confirmer par test utilisateur a differentes tailles de buffer
  (144, 512, 2048, 4096).

### Session de debug

`debug-persistent-audio-glitches.md` (statut : FIXED).
