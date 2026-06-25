# Comparaison des bibliotheques open source pour le pitch shifting temps reel

> Document de travail - 2026-06-11
> But : choisir la bibliotheque de pitch shifting qui remplacera notre PSOLA
> maison dans le cadre de la refonte du pipeline DSP.

## Contexte

Le PSOLA maison de l'Autotune Clone genere des artefacts audibles
(phasiness, pops, glitches pitch-dependent) qui ne sont pas corrigibles
par ajustements incrementaux (rounds 4 a 8 vus). Le pipeline est
trop simpliste (OLA avec Hann, pas de phase-locking, COLA
discontinue) et doit etre remplace par une bibliotheque tierce de
qualite production.

Ce document compare les options reelles et fait une recommandation.

---

## Bibliotheques evaluees

### 1. RubberBand Library

- **Site officiel** : https://breakfastquay.com/rubberband/index.html
- **Auteur** : Chris Cannam (mainteneur actif)
- **Version courante** : 4.0.0 (2024)
- **Licence** : **GPL-2.0-or-later** (virale)
- **Langage** : C++, wrapper JNI, Python, LV2
- **Algorithme** : Phase vocoder R3 (canon de Griffin & Lim)
- **Latence typique** : 50-100 ms (configurable via `Option::WindowSize`)

#### Qualite audio

Excellente, standard de l'industrie. Utilise par :
- **MuseScore** (transposition audio dans les partitions)
- **Mixxx** (DJ software)
- **Ardour** (DAW open source)
- **Sonic Visualiser**
- Divers plugins LV2 et Audio Units commerciaux

Gestion particuliere des transitoires (`Option::PhaseLocus = Transient`),
du formant preservation, et des signaux non-stationnaires. Beaucoup
mieux que PSOLA sur les voix soutenues et les voyelles.

#### Integration

- Bibliotheque C++ classique (header + .lib/.so/.dll)
- API simple :
  ```cpp
  RubberBand::RubberBandStretcher stretcher(sampleRate, channels, options);
  stretcher.setPitchScale(pow(2.0, semitones/12.0));
  stretcher.process(input, inputSize, false);
  stretcher.retrieve(output, outputSize);
  ```
- Compatible Windows, macOS, Linux, iOS, Android
- Build CMake standard
- Wrapper JUCE : il faut faire un wrapper maison (50-100 lignes)

#### Implications licence

**GPL = virale** : si on linke statiquement RubberBand dans notre
plugin VST3, le plugin lui-meme doit etre distribue sous GPL.
- Les utilisateurs peuvent voir et modifier le code source
- Ils peuvent le redistribuer librement
- Obligation de fournir le source avec le binaire
- Pas de vente "close-source" possible sans acheter une **licence
  commerciale** (Standard Licence : £420 / Non-Attribution : £1120)

#### Cout integration

- Build CMake : ~30 min
- Wrapper JUCE (`RubberBandPitchShifter` derivant de notre `PitchShifter`) : ~2-3 h
- Tests audio (buffer sizes 144/512/2048) : ~1 h
- Latence a declarer dans `getLatencySamples` : 5 min
- **Total : ~1 journee de travail**

---

### 2. SoundTouch

- **Site officiel** : https://soundtouch.surina.su/
- **Auteur** : Olli Parviainen (mainteneur actif)
- **Version courante** : 2.3.3 (2023)
- **Licence** : **LGPL-2.1** (moins contraignante)
- **Langage** : C++ (bibliotheque classique)
- **Algorithme** : SOLA/WSOLA (Synchronous OverLap-Add)
- **Latence typique** : ~100-130 ms

#### Qualite audio

Correcte a bonne sur la majorite des signaux. Legerement en retrait
par rapport a RubberBand sur :
- Voix tres soutenues (un peu de "phasiness" sur les voyelles tenues)
- Transitoires (moins bonne preservation des attaques)
- Pitch shifts importants (> 5 demi-tons)

Mais nettement superieure a notre PSOLA maison. Utilisee par :
- **MuseScore** (egalement, pour comparaison)
- **Auralé** (lecteur audio Android)
- **BPM Analyzer**
- **Mixxx** (en option)
- Plusieurs plugins LV2

#### Integration

- API C++ classique, simple :
  ```cpp
  soundtouch::SoundTouch st;
  st.setSampleRate(sampleRate);
  st.setChannels(channels);
  st.setPitchSemiTones(semitones);
  st.putSamples(input, numSamples);
  st.receiveSamples(output, numSamples);
  ```
- Header-only pour le wrapper haut-niveau, lib dynamique sinon
- Build CMake officiel disponible
- Compatible Windows, macOS, Linux, Android, iOS
- Wrapper JUCE : ~1-2 h

#### Implications licence

**LGPL = permissive** : on peut lier SoundTouch **dynamiquement**
(.dll / .so livre separe) sans contaminer la licence de notre
plugin.
- Notre plugin peut rester **close-source**
- On peut le **vendre** sans restriction
- Obligation : permettre a l'utilisateur de remplacer la .dll
  SoundTouch (standard sous Windows)
- Alternative : licence commerciale SoundTouch disponible (contact
  auteur, tarif non public mais raisonnable)

#### Cout integration

- Build CMake : ~20 min
- Wrapper JUCE : ~1-2 h
- Tests audio : ~1 h
- Documentation utilisateur (comment remplacer la .dll) : ~30 min
- **Total : ~0.5-1 journee de travail**

---

### 3. Autres bibliotheques evaluees rapidement

| Lib | Licence | Algo | Qualite voix | Verdict |
|---|---|---|---|---|
| **Aubio** | GPL-3.0 | PSOLA + variantes | Variable | Oriente analyse, pas de qualite prod |
| **libsamplerate** | BSD-2 | SRC best-fit | Mediocre | SRC, pas vraiment pitch shifting |
| **PaulStretch** | GPL | Extreme TS | Excellent pour extreme | Hors scope (TS extreme) |
| **JUCE `dsp::PitchShift`** | (closed JUCE) | Phase vocoder basique | Mauvaise sur voix | Pas de qualite prod |
| **TAL-Vocoder** (style) | GPL-2.0 | Divers | Variable | Reference uniquement |

**Verdict** : les seules options serieuses pour un autotune de
qualite sont **RubberBand** et **SoundTouch**.

---

## Comparatif synthetique

| Critere | RubberBand 4.0.0 | SoundTouch 2.3.3 |
|---|---|---|
| Licence | GPL-2.0-or-later (virale) | LGPL-2.1 (permissive) |
| Qualite vocale | Excellente | Bonne (un cran en-dessous) |
| Transitoires | Tres bien preservees | Correct |
| Latence | 50-100 ms (configurable) | 100-130 ms |
| CPU | Moyen | Leger |
| Build | CMake | CMake |
| Mainteneur | Actif (breakfastquay) | Actif (Parviainen) |
| Cout commercial | £420-1120 (optionnel) | Inconnu (contacter auteur) |
| Cout integration | ~1 jour | ~0.5-1 jour |
| **Plugin doit etre GPL ?** | **OUI** (obligatoire) | **NON** (LGPL dynamique) |

---

## Recommandation

### Si tu acceptes la GPL sur le plugin

**RubberBand**, sans hesitation. Qualite audio superieure, gestion
des transitoires et formants tres soignee, latence configurable.
C'est ce qu'utilisent les projets open source audio serieux
(Ardour, MuseScore, Mixxx).

### Si tu veux garder la liberte de licence

**SoundTouch**. La qualite est legerement en retrait mais tres
superieure a notre PSOLA actuel. Le gain net sera enorme (de
"inutilisable sur voix out-of-tune" a "utilisable en production
legere"). Le cout licence est zero et tu gardes la possibilite
de vendre ton plugin ferme.

### Alternative hybride

Utiliser **SoundTouch en LGPL** comme defaut, et **laisser un
slot compile-time** pour un wrapper RubberBand. Tu commences
par SoundTouch (rapidement fonctionnel, pas de contraintes),
et tu pourras switcher plus tard en acceptant la GPL.

---

## Prochaines etapes

1. **Decision utilisateur** (FAIT 2026-06-11) : Jerome a choisi
   **RubberBand (GPL)**. Voir le Round 10 du
   `docs/changelogs/changelog-2026-06-11.md` pour le detail de l'integration.
2. ~~Si l'utilisateur choisit RubberBand~~ : fait, voir
   `Source/dsp/RubberBandPitchShifter.h/.cpp` et le Round 10 du
   changelog.
3. ~~Si l'utilisateur choisit SoundTouch~~ : non applicable.

## References externes

- RubberBand API : https://breakfastquay.com/rubberband/code.html
- SoundTouch API : http://www.surina.net/soundtouch/README.html
- GPL-2.0 implications : https://www.gnu.org/licenses/old-licenses/gpl-2.0.html
- LGPL-2.1 implications : https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
